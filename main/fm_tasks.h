#ifndef __FUEL_METER_TASKS_H
#define __FUEL_METER_TASKS_H

#include "ws_comms.h"
#include "obd9141.h"
#include "debug.h"
#include "phys_const.h"

#include "esp_timer.h"
#include "rom/ets_sys.h"
#include <math.h>

#define INJECTOR_PIN GPIO_NUM_18
#define INBETWEEN_DELAY_MS 1
#define MAX_PULSES 64 // Max count of pulses per 500 ms, @ 12000 RPM you have 100 injections/sec or 50 injections per 500 ms, so 64 is way more than I will ever need (Corsa RPM limit 6-7k RPM)
 
// Stores runtime fuel statistics
typedef struct fuel_stats_t {
    // Instantaneous fuel consumption (based on fuel/distance in the last 600 ms)
    float fuel_cons_inst;       // [L/100 km]

    // Average fuel consumption (since boot)
    float fuel_cons_avg;        // [L/100 km]

    // Fuel consumed (since boot)
    double fuel_consumed;       // [uL]

    // Distance travelled (since boot)
    double dist_tr;             // [m]

    // Fuel consumed in the last 6 seconds
    double fuel_cons_last_6;    // [uL]

    // Fuel consumed in the last 60 seconds
    double fuel_cons_last_60;   // [uL]
} fuel_stats_t;

void init_pulse_width_gpio(void);

/* Fuel Meter tasks */

void fuel_meter_task(void *pvParameters);

void current_page_task(void *pvParameters);

void rpm_task(void *pvParameters);

void data_sync_task(void* pvParameters);




#endif