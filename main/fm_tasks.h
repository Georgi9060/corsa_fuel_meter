#ifndef __FUEL_METER_TASKS_H
#define __FUEL_METER_TASKS_H

#include "ws_comms.h"
#include "obd9141.h"
#include "debug.h"

#include "esp_timer.h"
#include "rom/ets_sys.h"

#define INJECTOR_PIN GPIO_NUM_18
#define INBETWEEN_DELAY_MS 10

void init_pulse_width_gpio(void);

void rpm_task(void *pvParameters);

void data_sync_task(void* pvParameters);



void get_comms_data(comms_data_pack_t *data);

void get_debug_fuel_data(debug_fuel_data_pack_t *data);

void get_fuel_data(fuel_data_pack_t *data);


#endif