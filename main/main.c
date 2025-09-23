#include "websocket.h"
#include "set_up_wifi.h"
#include "logs_to_web.h"
#include "ws_comms.h"
#include "obd9141.h"
#include "fm_tasks.h"
#include "debug.h"

#include "esp_log.h"

extern httpd_handle_t server;

extern TaskHandle_t fuel_meter_task_handle;
extern TaskHandle_t current_page_task_handle;

extern char currently_open_page[32];

static const char *TAG = "main";

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
    init_pulse_width_gpio();

    xTaskCreate(monitor_server_handle_task, "monitor_server_handle_task", 4096, NULL, 4, NULL);

    // Start logging system
    init_logging_system();

    // Init KWP comms
    OBD9141_begin();
    ESP_LOGI(TAG, "Initialisation...\n");
    OBD9141_delay(1000);
    while(1){
        bool init_success = OBD9141_init_kwp();
        ESP_LOGI(TAG, "Init success: %d\n", init_success);
        OBD9141_delay(50);
        if(init_success){
            xTaskCreate(fuel_meter_task, "fuel_meter_task", 8192, NULL, 15, &fuel_meter_task_handle);
            xTaskCreate(current_page_task, "current_page_task", 4096, NULL, 10, &current_page_task_handle);
            return;
        }
        OBD9141_delay(3000); // Wait before retrying connection
    }
}