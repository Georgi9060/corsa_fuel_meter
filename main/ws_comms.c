#include "ws_comms.h"

extern httpd_handle_t server;

extern char currently_open_page[32];

const char *TAG = "ws_comms";

/* Send */

void send_comms_data_pack(comms_data_pack_t data) {
    char buf[128];
    snprintf(buf, sizeof(buf), "c|%d|%d|%d|%d|%d|%.2f|%d|%d|%d",
                                data.load,
                                data.coolant_temp,
                                data.rpm,
                                data.speed,
                                data.intake_temp,
                                data.maf,
                                data.throttle,
                                data.attempt_cntr,
                                data.success_cntr
                                );

    if (trigger_async_send(server, buf) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send comms.");
    }
#ifdef COMMS_DEBUG
    else{
        printf("Sent: %s\n", buf);
    }
#endif
}

void send_debug_fuel_data_pack(debug_fuel_data_pack_t data) {
    char buf[128];
    snprintf(buf, sizeof(buf), "d|%.1f|%.1f|%d|%.2f|%d|%d|%d|%d|%d|%.1f",
                     data.inst_fuel,
                     data.avg_fuel,
                     data.coolant_temp,
                     data.cons_fuel,
                     data.rpm,
                     data.speed,
                     data.pcnt_isr,
                     data.pcnt_rpm,
                     data.pdelta,
                     data.avg_pwidth
                    );

    if (trigger_async_send(server, buf) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send debug_fuel.");
    }
#ifdef COMMS_DEBUG
    else{
        printf("Sent: %s\n", buf);
    }
#endif
}

void send_fuel_data_pack(fuel_data_pack_t data) {
    char buf[128];
    snprintf(buf, sizeof(buf), "f|%.1f|%.1f|%d|%.2f|%.1f|%.0f|",
                                data.inst_fuel,
                                data.avg_fuel,
                                data.coolant_temp,
                                data.cons_fuel,
                                data.fuel_last_6,
                                data.fuel_last_60
                                );

    if (trigger_async_send(server, buf) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send fuel.");
    }
#ifdef COMMS_DEBUG
    else{
        printf("Sent: %s\n", buf);
    }
#endif
}


/* Receive */

void set_open_page(cJSON *root) {
    cJSON *page = cJSON_GetObjectItem(root, "page");

    if(!cJSON_IsString(page)){
        ESP_LOGE(TAG, "'page' is not a string!"); return;
    }
    strcpy(currently_open_page, page->valuestring);
    ESP_LOGI(TAG,"Currently open page: %s", currently_open_page);
}