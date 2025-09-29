#include "websocket.h"
#include "set_up_wifi.h"
#include "logs_to_web.h"
#include "ws_comms.h"
#include "obd9141.h"
#include "fm_tasks.h"
#include "debug.h"
#include "nvs.h"

#include "esp_log.h"

extern httpd_handle_t server;

extern EventGroupHandle_t startup_event_group;
extern TaskHandle_t fuel_meter_task_handle;
extern TaskHandle_t current_page_task_handle;
extern TaskHandle_t display_task_handle;

extern char currently_open_page[32];

static const char *TAG = "main";

bool kwp_init_success = false;

void app_main() {
    // Inits
    startup_event_group = xEventGroupCreate();
    i2cdev_init();
    xTaskCreate(display_task, "display_task", configMINIMAL_STACK_SIZE * 5, NULL, 5, &display_task_handle);
    init_nvs();
    wifi_init_softap();
    initi_web_page_buffer();
#ifdef WS_DEBUG
    list_spiffs_files();
#endif
    setup_websocket_server();
    init_pulse_width_gpio();
    xTaskCreate(init_bmp280_sensor, "init_bmp280_task", 4096, NULL, 3, NULL);
    xTaskCreate(monitor_server_handle_task, "monitor_server_handle_task", 4096, NULL, 4, NULL);

    init_logging_system();

    // Inits done
    xEventGroupSetBits(startup_event_group, INITS_DONE);

    // Start KWP comms
    OBD9141_begin();
    OBD9141_delay(1000);
    while(1){
        kwp_init_success = OBD9141_init_kwp();
        ESP_LOGI(TAG, "KWP init success: %d\n", kwp_init_success);
        xTaskNotifyGive(display_task_handle); // Indicate success/fail on display
        OBD9141_delay(50);
        if(kwp_init_success){
            xEventGroupSetBits(startup_event_group, KWP_INIT);
            // Create core functionality tasks and return from main
            xTaskCreate(fuel_meter_task, "fuel_meter_task", 8192, NULL, 15, &fuel_meter_task_handle);
            xTaskCreate(current_page_task, "current_page_task", 4096, NULL, 10, &current_page_task_handle);
            return;
        }
        else{
            xTaskNotifyGive(display_task_handle); // Indicate retry on display
            OBD9141_delay(3000); // Wait before retrying connection
        }
    }
}