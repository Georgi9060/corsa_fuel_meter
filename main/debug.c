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

void timer_start(const char *label) {
    timer_start_time = esp_timer_get_time();
    int64_t ms_hundredths = timer_start_time / 10; // 1 unit = 0.01 ms
    ESP_LOGI(label, "st: %lld.%02lld ms",
             ms_hundredths / 100,   // integer
             ms_hundredths % 100); // fraction
}

void timer_stop(const char *label) {
    int64_t elapsed_us = esp_timer_get_time() - timer_start_time;
    int64_t ms_hundredths = elapsed_us / 10; // 1 unit = 0.01 ms
    ESP_LOGI(label, "elapsed: %lld.%02lld ms",
             ms_hundredths / 100,   // integer
             ms_hundredths % 100); // fraction
}