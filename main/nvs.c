#include "nvs.h"

nvs_handle_t fuel_data_handle;

static const char *TAG = "nvs";

void init_nvs(void) {
    // Init NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open handle

    err = nvs_open("fuel_data", NVS_READWRITE, &fuel_data_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening fuel_data NVS handle!\n", esp_err_to_name(err));
    }
}

/* Getter functions */

double get_fuel_consumed(void) {
    uint64_t fuel_consumed = 0;
    esp_err_t err = nvs_get_u64(fuel_data_handle, "fuel_consumed", &fuel_consumed);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read fuel_consumed = %llu [uL]", fuel_consumed);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(TAG, "The value of fuel_consumed is not initialised yet!");
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading fuel_consumed!", esp_err_to_name(err));
    }
    return (double)fuel_consumed;
}


double get_dist_tr(void) {
    uint64_t dist_tr = 0;
    esp_err_t err = nvs_get_u64(fuel_data_handle, "dist_tr", &dist_tr);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read dist_tr = %llu [m]", dist_tr);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(TAG, "The value of dist_tr is not initialised yet!");
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading dist_tr!", esp_err_to_name(err));
    }
    return (double)dist_tr;
}

/* Setter functions */

void set_fuel_consumed(double val) {
    uint64_t fuel_consumed = (uint64_t)val;
    esp_err_t err = nvs_set_u64(fuel_data_handle, "fuel_consumed", fuel_consumed);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write fuel_consumed!");
    }
    err = nvs_commit(fuel_data_handle);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit fuel_consumed changes!");
    }
    else{
        ESP_LOGI(TAG,"Set fuel_consumed to %llu [uL]", fuel_consumed);
    }
}

void set_dist_tr(double val) {
    uint64_t dist_tr = (uint64_t)val;
    esp_err_t err = nvs_set_u64(fuel_data_handle, "dist_tr", dist_tr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write dist_tr!");
    }
    err = nvs_commit(fuel_data_handle);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit dist_tr changes!");
    }
    else{
        ESP_LOGI(TAG,"Set dist_tr to %llu [m]", dist_tr);
    }
}