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

#ifndef __WEBSOCKET_H
#define __WEBSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include <esp_http_server.h>
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include <dirent.h>
#include <sys/stat.h>

// #define WS_DEBUG            // uncomment to enable Websocket debug logs

#define MAX_RETRIES 3       // WebSocket packet retry
#define SERVER_RESERVED_SOCKETS 3
#define INDEX_HTML_PATH "/spiffs/index.html"

struct async_resp_arg {
    httpd_handle_t hd;
    char *message;
};

void initi_web_page_buffer(void);

#ifdef WS_DEBUG
void list_spiffs_files(void);
#endif

esp_err_t get_req_handler(httpd_req_t *req);

esp_err_t static_file_handler(httpd_req_t *req);

esp_err_t trigger_async_send(httpd_handle_t handle, const char *message);

httpd_handle_t setup_websocket_server(void);

void close_websocket_client(httpd_handle_t server, int client_fd);

#endif