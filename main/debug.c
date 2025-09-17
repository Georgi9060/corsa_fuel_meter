#include "debug.h"

extern httpd_handle_t server;

static int64_t timer_start_time = 0;

// Debug function to fix sporadic changes in the httpd_handle_t server's address, leading to crashes/breakdown of communications
// TODO: Temporary fix until I discover what is causing the issue
void monitor_server_handle_task(void *arg) {
    const httpd_handle_t server_expected = server; // save initial address
    static bool just_restored = false;
    printf("\n\nserver INITIALLY: %p\n\n", server_expected);
    while (1) {
        if (server != server_expected) {
            printf("ALERT: server handle changed! Expected: %p, Actual: %p\n \nAttempting to restore...\n\n\n", server_expected, server);
            server = server_expected;
            just_restored = true;
        }
        else {
            if(just_restored){printf("Restored successfully!\n"); just_restored = false;}
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

float random_float_in_range(float min, float max) {
    uint32_t r = esp_random();                  // 0 … 2^32-1
    float norm = (float)r / (float)UINT32_MAX;  // 0.0 … 1.0
    return min + norm * (max - min);
}

void timer_start(const char *label) {
    timer_start_time = esp_timer_get_time();
    ESP_LOGI(label, "st: %.0f", timer_start_time / 1000.0);
}

void timer_stop(const char *label) {
    int64_t elapsed = esp_timer_get_time() - timer_start_time; // in us
    float ms = elapsed / 1000.0;
    ESP_LOGI(label,"%.1f ms", ms);
}