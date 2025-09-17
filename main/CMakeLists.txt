idf_component_register(SRCS 
                        "debug.c"
                        "fm_tasks.c"
                        "logs_to_web.c"
                        "main.c"
                        "obd9141.c"
                        "set_up_wifi.c"
                        "websocket.c"
                        "ws_comms.c"
                    INCLUDE_DIRS ".")
spiffs_create_partition_image(storage ../web FLASH_IN_PROJECT)