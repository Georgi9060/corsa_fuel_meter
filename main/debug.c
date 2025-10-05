/* 
 * ESP32 Corsa Fuel Meter
 * Copyright (C) 2025 Georgi Georgiev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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