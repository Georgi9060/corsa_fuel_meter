#ifndef __SET_UP_WIFI_H
#define __SET_UP_WIFI_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"


#define EXAMPLE_ESP_WIFI_SSID      "Fuel Meter"
#define EXAMPLE_ESP_WIFI_PASS      "Georgious"
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       4
#define CONFIG_ESP_GTK_REKEYING_ENABLE true

#if CONFIG_ESP_GTK_REKEYING_ENABLE
#define EXAMPLE_GTK_REKEY_INTERVAL 600
#else
#define EXAMPLE_GTK_REKEY_INTERVAL 0
#endif

void wifi_init_softap(void);

#endif