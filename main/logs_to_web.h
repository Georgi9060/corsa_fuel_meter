/*
 * MIT License
 * Copyright (c) 2025 Georgi Georgiev
 * See LICENSE file for full license text.
 */

#ifndef __LOGS_TO_WEB
#define __LOGS_TO_WEB

#include <stdarg.h>
#include <unistd.h>
#include "esp_log.h"

#include "websocket.h"

#define LOG_LINE_MAX 256
#define LOG_QUEUE_LEN 32

int my_log_vprintf(const char *fmt, va_list args);

void log_task(void *param);

void init_logging_system(void);

#endif