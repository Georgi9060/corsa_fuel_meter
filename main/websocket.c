#include "websocket.h"
#include "ws_comms.h"

extern SemaphoreHandle_t ws_connected_sem;

static char index_html[4096];
static char response_data[4096];
static int active_clients = 0;

httpd_handle_t server = NULL;

void initi_web_page_buffer(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 10,
        .format_if_mount_failed = true};

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    memset((void *)index_html, 0, sizeof(index_html));
    struct stat st;
    if (stat(INDEX_HTML_PATH, &st))
    {
#ifdef WS_DEBUG
        printf("index.html not found\n");
#endif
        return;
    }

    FILE *fp = fopen(INDEX_HTML_PATH, "r");
    if (fread(index_html, st.st_size, 1, fp) == 0)
    {
#ifdef WS_DEBUG
        printf("fread failed\n");
#endif
    }
    fclose(fp);
}

#ifdef WS_DEBUG
void list_spiffs_files(void) {
    DIR *dir = opendir("/spiffs");
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            printf("Found file: %s\n", entry->d_name);
        }
        closedir(dir);
    } else {
        printf("Failed to open SPIFFS directory\n");
    }
}
#endif

esp_err_t get_req_handler(httpd_req_t *req) {
    esp_err_t err = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);
    if (err != ESP_OK) {
#ifdef WS_DEBUG
        printf("Error sending response: %s\n", esp_err_to_name(err));
#endif
    }
    return err;
}

esp_err_t static_file_handler(httpd_req_t *req) {
    char filepath[1024];
    
    // Handle the root URI ("/") to serve index.html
    if (strcmp(req->uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "/spiffs/index.html");
    } else {
        snprintf(filepath, sizeof(filepath), "/spiffs%s", req->uri);
    }
#ifdef WS_DEBUG
    printf("Requested file: %s\n", filepath);
#endif

    FILE *file = fopen(filepath, "r");
    if (!file) {
#ifdef WS_DEBUG
        printf("Failed to open file: %s\n", filepath);
#endif
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    if (strstr(filepath, ".css")) {
        httpd_resp_set_type(req, "text/css");
    } else if (strstr(filepath, ".js")) {
        httpd_resp_set_type(req, "application/javascript");
    } else if (strstr(filepath, ".html")) {
        httpd_resp_set_type(req, "text/html");
    } else {
        httpd_resp_set_type(req, "text/plain");
    }

    char buffer[512];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(file);

    // End the response with an empty chunk
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static void ws_async_send(void *arg) {
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)arg;
    if (!resp_arg || !resp_arg->message) {
#ifdef WS_DEBUG
        printf("Invalid argument or message is NULL\n");
#endif
        free(resp_arg);
        return;
    }

    httpd_ws_frame_t ws_pkt = {
        .payload = (uint8_t *)resp_arg->message,
        .len = strlen(resp_arg->message),
        .type = HTTPD_WS_TYPE_TEXT,
    };

    size_t fds = 16;
    int client_fds[fds];
    memset(client_fds, 0, sizeof(client_fds));
    
    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);
    if (ret != ESP_OK) {
#ifdef WS_DEBUG
        printf("Failed to get client list: %s\n", esp_err_to_name(ret));
#endif
        free(resp_arg->message);
        free(resp_arg);
        return;
    }
#ifdef WS_DEBUG
    printf("Found %d clients\n", fds);
#endif

    for (size_t i = 0; i < fds; i++) {
        if (client_fds[i] < 0) {
#ifdef WS_DEBUG
            printf("Skipping invalid file descriptor: %d\n", client_fds[i]);
#endif
            continue;
        }

        int client_info = httpd_ws_get_fd_info(server, client_fds[i]);
        if (client_info != HTTPD_WS_CLIENT_WEBSOCKET) {
#ifdef WS_DEBUG
            printf("Skipping non-WebSocket client: %d\n", client_fds[i]);
#endif
            continue;
        }

        // Retry logic
        for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
            ret = httpd_ws_send_frame_async(resp_arg->hd, client_fds[i], &ws_pkt);
            if (ret == ESP_OK) {
#ifdef WS_DEBUG
                printf("Message sent to client %d on attempt %d\n", client_fds[i], attempt + 1);
#endif
                break;
            } else {
#ifdef WS_DEBUG
                printf("Retry %d failed for client %d: %d\n", attempt + 1, client_fds[i], ret);
#endif
            }
        }

        if (ret != ESP_OK) {
#ifdef WS_DEBUG
            printf("Failed to send frame to client %d after %d retries. Closing connection.\n", client_fds[i], MAX_RETRIES);
#endif
            close_websocket_client(server, client_fds[i]);
        }
    }

    free(resp_arg->message);
    free(resp_arg);
}

esp_err_t trigger_async_send(httpd_handle_t handle, const char *message) {
    if (message == NULL) {
#ifdef WS_DEBUG
        printf("Message is NULL. Cannot proceed with async send.\n");
#endif
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate memory for the response argument
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
#ifdef WS_DEBUG
        printf("Failed to allocate memory for response argument.\n");
#endif
        return ESP_ERR_NO_MEM;
    }

    // Duplicate the message to ensure it's safely stored
    resp_arg->message = strdup(message);
    
    if (resp_arg->message == NULL) {
#ifdef WS_DEBUG
        printf("no heap: %lu \n", esp_get_free_internal_heap_size());
        printf("Failed to allocate memory for message.\n");
#endif
        free(resp_arg);
        return ESP_ERR_NO_MEM;
    }

    // Store the WebSocket handle
    resp_arg->hd = handle;

    // Queue the async task for sending
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
#ifdef WS_DEBUG
        printf("Failed to queue work for WebSocket async send. Err: %s\n", esp_err_to_name(ret));
#endif
        free(resp_arg->message); // Free message if queuing fails
        free(resp_arg);
    }
    
    return ret;
}

static esp_err_t handle_ws_req(httpd_req_t *req) {
    if (req->method == HTTP_GET)
    {
#ifdef WS_DEBUG
        printf("Handshake done, the new connection was opened\n");
#endif
        if(active_clients == 0){
            xSemaphoreGive(ws_connected_sem);
        }
        active_clients++;
        return ESP_OK;
    }

    // WebSocket frame processing
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // Receive the WebSocket frame
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK){
#ifdef WS_DEBUG
        printf("httpd_ws_recv_frame failed to get frame len: %s\n", esp_err_to_name(ret));
#endif
        return ret;
    }

    // Process the frame if it has payload
    if (ws_pkt.len){
        buf = calloc(1, ws_pkt.len + 1);
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK){
#ifdef WS_DEBUG
            printf("httpd_ws_recv_frame failed: %s\n", esp_err_to_name(ret));
#endif
            free(buf);
            return ret;
        }
#ifdef WS_DEBUG
        printf("Got packet with message: %s\n", ws_pkt.payload);
#endif
    }

    // Parse JSON message
    cJSON *root = cJSON_Parse((char *)buf);
    if (!root){
        return ESP_ERR_NOT_FOUND;
    }
    // Parse the type of message
    cJSON *cmd_type = cJSON_GetObjectItem(root, "type");
    if(!cJSON_IsString(cmd_type)){
#ifdef WS_DEBUG
        printf("No type field found\n");
#endif
        return ESP_ERR_NOT_FOUND;
    }

    if (strcmp(cmd_type->valuestring, "page_open") == 0) {
        set_open_page(root);
    }
    // else if (strcmp(cmd_type->valuestring, "filler") == 0) {
    //     do_something();
    // }

    cJSON_Delete(root);
    free(buf);

    return ESP_OK;
}

httpd_handle_t setup_websocket_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.stack_size = 8192;

    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_req_handler,
        .user_ctx = NULL};

    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handle_ws_req,
        .user_ctx = NULL,
        .is_websocket = true};

    // List of files to make uri handlers for
    const char * filenames[] = {
        "styles.css",
        "script.js",
        "index.html",
        "comms.html",
        "debugfuel.html",
        "fuel.html",
        "logs.html"
        };

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);

        char uri[128];
        for(size_t i = 0; i < (sizeof(filenames) / sizeof(filenames[0])); i++){
            snprintf(uri, sizeof(uri), "/%s", filenames[i]);
            httpd_uri_t static_file = {
                .uri = uri,
                .method = HTTP_GET,
                .handler = static_file_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &static_file);
        }

        httpd_register_uri_handler(server, &ws);
    }
    return server;
}

void close_websocket_client(httpd_handle_t server, int client_fd) {
    if (client_fd < 0) {
#ifdef WS_DEBUG
        printf("Invalid client file descriptor: %d\n", client_fd);
#endif
        return;
    }

    // Attempt to close the client session
    esp_err_t ret = httpd_sess_trigger_close(server, client_fd);
#ifdef WS_DEBUG
    if (ret != ESP_OK) {
        printf("Failed to close WebSocket client %d: %d\n", client_fd, ret);
    } else {
        printf("WebSocket client %d closed successfully\n", client_fd);
    }
#endif
}