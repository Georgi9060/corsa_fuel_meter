#include "logs_to_web.h"

extern httpd_handle_t server;

static QueueHandle_t log_queue;

// Override ESP-IDF logging
int my_log_vprintf(const char *fmt, va_list args) {
    char buf[LOG_LINE_MAX];
    vsnprintf(buf, LOG_LINE_MAX, fmt, args);

    // Queue the log line
    xQueueSend(log_queue, buf, 0);

    // Echo to UART
    return vprintf(fmt, args);
}

// FreeRTOS Task: send queued logs over WebSocket 
void log_task(void *param) {
    char line[LOG_LINE_MAX];

    while (1) {
        if (xQueueReceive(log_queue, line, portMAX_DELAY) == pdTRUE) {
            // Send over WebSocket
            if (trigger_async_send(server, line) != ESP_OK) {
                    printf("Failed to send log: %s\n", line);
                }
        }
    }
}

// Init logging system
void init_logging_system(void) {
    log_queue = xQueueCreate(LOG_QUEUE_LEN, LOG_LINE_MAX);
    if (!log_queue) {
        ESP_LOGE("LOG", "Failed to create log queue");
        return;
    }

    // Override ESP-IDF logs
    esp_log_set_vprintf(&my_log_vprintf);

    // Start log sending task
    xTaskCreate(log_task, "log_task", 4096, NULL, 5, NULL);
}
