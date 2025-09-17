#ifndef __DEBUG_H
#define __DEBUG_H

#include "freertos/FreeRTOS.h"
#include <esp_http_server.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

void monitor_server_handle_task(void *arg);

float random_float_in_range(float min, float max);

void timer_start(const char *label);

void timer_stop(const char *label);

#endif