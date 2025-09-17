#include "websocket.h"
#include "set_up_wifi.h"
#include "logs_to_web.h"
#include "ws_comms.h"
#include "obd9141.h"
#include "fm_tasks.h"
#include "debug.h"

#include "esp_log.h"

extern httpd_handle_t server;
extern TaskHandle_t rpm_task_handle;
extern TaskHandle_t data_sync_task_handle;

static const char *TAG = "main"; // TAG for debug

char currently_open_page[30] = {0};  // used to indicate which type of packet to prepare & send
SemaphoreHandle_t ws_connected_sem;  // to block until a client connects



static void init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void app_main() {
    init_nvs();
    wifi_init_softap();
    ESP_LOGI(TAG, "ESP32 Access Point running...\n");
    initi_web_page_buffer();
#ifdef WS_DEBUG
    list_spiffs_files();
#endif
    setup_websocket_server();
    xTaskCreate(monitor_server_handle_task, "monitor_server_handle_task", 4096, NULL, 4, NULL);

    // Wait before spamming console
    ws_connected_sem = xSemaphoreCreateBinary();
    ESP_LOGI(TAG, "Waiting for WebSocket client...");
    xSemaphoreTake(ws_connected_sem, portMAX_DELAY);
    ESP_LOGI(TAG, "Client connected, continuing main flow!");

     // Start logging system
    init_logging_system();

    // OBD9141 EXAMPLE
    const char *TAG = "OBD";

    OBD9141_begin();
    ESP_LOGI(TAG, "Initialisation...\n");
    OBD9141_delay(1000);
    while(1){
        bool init_success = OBD9141_init_kwp();

        // init_success = 1; // force for dummy data to begin

        ESP_LOGI(TAG, "Init success: %d\n", init_success);

        OBD9141_delay(50);
        if(init_success){
#define PULSE_TEST
#ifdef PULSE_TEST
            init_pulse_width_gpio();
            xTaskCreate(rpm_task, "rpm_task", 4096, NULL, 5, &rpm_task_handle);
            xTaskCreate(data_sync_task, "data_sync_task", 4096, NULL, 6, &data_sync_task_handle);
            return;
#else
            TickType_t lastWakeTime = xTaskGetTickCount();
            while(1){
                if(strcmp(currently_open_page, "comms.html") == 0){
                    comms_data_pack_t data = {0};

                    get_comms_data(&data);
                    // data.load = random_float_in_range(0,100);
                    // data.throttle = random_float_in_range(0,100);
                    // data.coolant_temp = random_float_in_range(-20, 110);
                    // data.intake_temp = random_float_in_range(-20, 50);
                    // data.rpm =random_float_in_range(0,7000);
                    // data.speed = random_float_in_range(0, 255);
                    // data.maf = random_float_in_range(0, 255);
                    // attempt_cntr = random_float_in_range(0,255);
                    // success_cntr = random_float_in_range(0,255);

                    send_comms_data_pack(data);
                }
                else if(strcmp(currently_open_page, "debugfuel.html") == 0){
                    debug_fuel_data_pack_t data = {0};

                    get_debug_fuel_data(&data);
                    // data.inst_fuel = random_float_in_range(0,50);
                    // data.avg_fuel = random_float_in_range(0, 12);
                    // data.coolant_temp = random_float_in_range(-20, 110);
                    // data.cons_fuel = random_float_in_range(0, 40);
                    // data.rpm =random_float_in_range(0,7000);
                    // data.speed = random_float_in_range(0, 255);
                    // data.pcnt_rpm = random_float_in_range(0, 1000);
                    // data.pcnt_isr = random_float_in_range(0, 1000);
                    // data.pdelta = data.pcnt_rpm - data.pcnt_isr;
                    // data.pdeltapc = ((float)(data.pcnt_rpm - data.pcnt_isr) / (float)data.pcnt_rpm) * 100.0f;

                    send_debug_fuel_data_pack(data);
                }
                else if(strcmp(currently_open_page, "fuel.html") == 0){
                    fuel_data_pack_t data = {0};

                    // get_fuel_data(&data);
                    data.inst_fuel = random_float_in_range(0,50);
                    data.avg_fuel = random_float_in_range(0, 12);
                    data.coolant_temp = random_float_in_range(-20, 110);
                    data.cons_fuel = random_float_in_range(0, 40);

                    send_fuel_data_pack(data);
                }
                vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));
            }
#endif
        }
        OBD9141_delay(3000); // Wait before retrying connection
    }
}