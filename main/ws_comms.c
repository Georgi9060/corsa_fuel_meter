#include "ws_comms.h"

extern httpd_handle_t server;

extern TaskHandle_t current_page_task_handle;

extern char currently_open_page[32];

const char *TAG = "ws_comms";

/* Send */

static void send_stored_vals(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "stored_vals");
    cJSON_AddNumberToObject(root, "fuel", get_fuel_consumed() * 0.000001); // [uL] to [L]
    cJSON_AddNumberToObject(root, "dist", get_dist_tr() * 0.001);          // [m] to [km]

    char *json_str = cJSON_PrintUnformatted(root);
    if (trigger_async_send(server, json_str) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send stored_vals");
    }

#ifdef COMMS_DEBUG
    else{
        printf("Sent: %s\n", json_str);
    }
#endif

    free(json_str);
    cJSON_Delete(root); 
}

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
    snprintf(buf, sizeof(buf), "d|%.1f|%.1f|%.1f|%.2f|%d|%d|%d|%d|%d|%.1f|%.1f|%.1f|",
                     data.inst_fuel,
                     data.avg_fuel,
                     data.dist_tr,
                     data.cons_fuel,
                     data.rpm,
                     data.speed,
                     data.pcnt_isr,
                     data.pcnt_rpm,
                     data.pdelta,
                     data.avg_pwidth,
                     data.amb_temp,
                     data.baro_pressure
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
    if(strcmp(currently_open_page, "fuel.html") == 0){send_stored_vals();} // Load stored vals along with page load
    ESP_LOGI(TAG,"Currently open page: %s", currently_open_page);
}

void load_fuel_data(void) {
    double fuel_consumed = get_fuel_consumed();
    double dist_tr = get_dist_tr();
    float fuel_cons_avg = dist_tr ? fuel_consumed / dist_tr * 0.1 : -1;
    // 1 [uL/m] = 1 [mL/km] = 100 [mL/100 km] = 0.1 [L/100 km]
    fuel_stats_t fuel_stats = {
        .fuel_cons_inst = -1,
        .fuel_cons_avg = fuel_cons_avg,
        .fuel_consumed = fuel_consumed,
        .dist_tr = dist_tr,
        .fuel_cons_last_6 = 0,
        .fuel_cons_last_60 = 0
    };
    set_stats(&fuel_stats);
}

void save_ovw_fuel_data(void) {
    const fuel_stats_t *fuel_stats = get_stats();
    set_fuel_consumed(fuel_stats->fuel_consumed);
    set_dist_tr(fuel_stats->dist_tr);
    send_stored_vals();
}

void save_add_fuel_data(void) {
    const fuel_stats_t *fuel_stats = get_stats();
    double fuel_consumed = get_fuel_consumed();
    double dist_tr = get_dist_tr();
    fuel_consumed += fuel_stats->fuel_consumed;
    dist_tr += fuel_stats->dist_tr;
    set_fuel_consumed(fuel_consumed);
    set_dist_tr(dist_tr);
    send_stored_vals();
}

void clear_fuel_data(void) {
    fuel_stats_t fuel_stats = {
        .fuel_cons_inst = -1,
        .fuel_cons_avg = -1,
        .fuel_consumed = 0,
        .dist_tr = 0,
        .fuel_cons_last_6 = 0,
        .fuel_cons_last_60 = 0
    };
    set_stats(&fuel_stats);
}

void delete_fuel_data(void) {
    set_fuel_consumed(0);
    set_dist_tr(0);
    send_stored_vals();
}