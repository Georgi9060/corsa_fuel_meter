#include "fm_tasks.h"

static portMUX_TYPE rpm_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint16_t rpm = 0;
static volatile uint16_t pulse_count_rpm = 0;

static portMUX_TYPE pulse_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint64_t pulse_width_us = 0;
static volatile uint16_t pulse_count_isr = 0;

TaskHandle_t rpm_task_handle = NULL;
TaskHandle_t data_sync_task_handle = NULL;

static const char *TAG = "fm_tasks"; // TAG for debug

// ISR handler for both edges
static void IRAM_ATTR injector_isr_handler(void* arg) {
    static volatile uint64_t fall_time_us = 0;
    int level = gpio_get_level(INJECTOR_PIN);
    uint64_t now = esp_timer_get_time();

    if (level == 0) {
        // Falling edge: start timing
        fall_time_us = now;
    } else {
        // Rising edge: stop timing
        if (fall_time_us > 0) {
            uint64_t duration = now - fall_time_us;
            // Filter out very short glitches
            if (duration > 50) {
                taskENTER_CRITICAL(&pulse_mux);
                pulse_width_us = duration;
                pulse_count_isr++;
                taskEXIT_CRITICAL(&pulse_mux);
                ets_printf("%llu ms\n", pulse_width_us / 1000);
            }
        }
    }
}

void init_pulse_width_gpio(void) {
    // Configure GPIO as input with interrupts on both edges
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << INJECTOR_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Used to be disable, must test if it improves the noise when at idle/accelerating/decelerating
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Install ISR service
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INJECTOR_PIN, injector_isr_handler, NULL));

    ESP_LOGI(TAG, "Ready to measure injector pulses on GPIO %d...", INJECTOR_PIN);
}

void rpm_task(void *pvParameters){
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Takes 80-90 ms
        bool res = OBD9141_get_current_pid(0x0C, 2);
        // Compute safely in locals
        uint16_t rpm_local = 0;
        uint16_t pcnt_rpm_local = 0;
        if (res) {
            rpm_local = OBD9141_read_uint16() / 4;

            if (rpm_local > 0) {
                // Injections per 500 ms, as expected from RPM
                // revs/min / 60 s = revs/sec; revs/sec / 2 (because every other rotation has an injection) and / 2 because revs/0.5 sec
                pcnt_rpm_local = rpm_local / 60 / 2 / 2;
            }
        }
        taskENTER_CRITICAL(&rpm_mux);
        rpm = rpm_local;
        pulse_count_rpm = pcnt_rpm_local;
        taskEXIT_CRITICAL(&rpm_mux);

        xTaskNotifyGive(data_sync_task_handle);
    }
}

void data_sync_task(void* pvParameters) {
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(500);

    while (1) {
        xTaskNotifyGive(rpm_task_handle);

        if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(120))) {
            ESP_LOGW(TAG, "rpm_task timeout");
        }

        debug_fuel_data_pack_t local_data_pack = {0};

        taskENTER_CRITICAL(&rpm_mux);
        local_data_pack.rpm = rpm;
        local_data_pack.pcnt_rpm = pulse_count_rpm;
        taskEXIT_CRITICAL(&rpm_mux);

        taskENTER_CRITICAL(&pulse_mux);
        local_data_pack.pcnt_isr = pulse_count_isr;
        pulse_count_isr = 0;
        taskEXIT_CRITICAL(&pulse_mux);

        local_data_pack.pdelta = local_data_pack.pcnt_rpm - local_data_pack.pcnt_isr;
        if(local_data_pack.pcnt_rpm == 0){local_data_pack.pcnt_rpm = 1;}
        local_data_pack.pdeltapc = ((float)(local_data_pack.pcnt_rpm - local_data_pack.pcnt_isr) / (float)local_data_pack.pcnt_rpm) * 100.0f;

        send_debug_fuel_data_pack(local_data_pack);

        vTaskDelayUntil(&last_wake, period);
    }
}






void get_comms_data(comms_data_pack_t *data) {
    bool res;

    timer_start("ld");
    res = OBD9141_get_current_pid(0x04, 1);
    data->attempt_cntr++;
    if(res){
        data->load = OBD9141_read_uint8() / 2.55;
        data->success_cntr++;
    }
    timer_stop("ld");
    OBD9141_delay(INBETWEEN_DELAY_MS);

    timer_start("ctmp");
    res = OBD9141_get_current_pid(0x05, 1);
    data->attempt_cntr++;
    if (res){
        data->coolant_temp = OBD9141_read_uint8() - 40;
        data->success_cntr++;
    }
    timer_stop("ctmp");
    OBD9141_delay(INBETWEEN_DELAY_MS);

    timer_start("rpm");
    res = OBD9141_get_current_pid(0x0C, 2);
    data->attempt_cntr++;
    if (res){
        data->rpm = OBD9141_read_uint16() / 4;
        data->success_cntr++;
    }
    timer_stop("rpm");
    OBD9141_delay(INBETWEEN_DELAY_MS);

    timer_start("spd");
    res = OBD9141_get_current_pid(0x0D, 1);
    data->attempt_cntr++;
    if (res){
        data->speed = OBD9141_read_uint8();
        data->success_cntr++;
    }
    timer_stop("spd");
    OBD9141_delay(INBETWEEN_DELAY_MS);

    timer_start("itmp");
    res = OBD9141_get_current_pid(0x0F, 1);
    data->attempt_cntr++;
    if (res){
        data->intake_temp = OBD9141_read_uint8() - 40;
        data->success_cntr++;
    }
    timer_stop("itmp");
    OBD9141_delay(INBETWEEN_DELAY_MS);

    timer_start("maf");
    res = OBD9141_get_current_pid(0x10, 2);
    data->attempt_cntr++;
    if (res){
        data->maf = OBD9141_read_uint16() / 100;
        data->success_cntr++;
    }
    timer_stop("maf");
    OBD9141_delay(INBETWEEN_DELAY_MS);

    timer_start("trl");
    res = OBD9141_get_current_pid(0x11, 1);
    data->attempt_cntr++;
    if (res){
        data->throttle = OBD9141_read_uint8() * 100 / 255;
        data->success_cntr++;
    }
    timer_stop("trl");
    if(data->success_cntr != data->attempt_cntr){
        ESP_LOGW(TAG, "success_cntr != attempt_cntr: %d/%d", data->success_cntr, data->attempt_cntr);
    }
}

void get_debug_fuel_data(debug_fuel_data_pack_t *data) {
    // TODO: In the actual project!
}

void get_fuel_data(fuel_data_pack_t *data) {
    // TODO: In the actual project!
}
