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

#ifndef __FUEL_METER_TASKS_H
#define __FUEL_METER_TASKS_H

#include "ws_comms.h"
#include "obd9141.h"
#include "debug.h"
#include "phys_const.h"

#include "esp_timer.h"
#include "rom/ets_sys.h"
#include <math.h>

#include <bmp280.h>
#include <pcf8574.h>
#include <hd44780.h>

#define INJECTOR_PIN    GPIO_NUM_18
#define SCL_PIN         GPIO_NUM_22
#define SDA_PIN         GPIO_NUM_21
#define BMP280_ADDR     0x76
#define PCF8574_ADDR    0x27
#define I2C_LCD1602_CHARACTER_DEGREE       0b11011111

#define INITS_DONE BIT0
#define KWP_INIT   BIT1



#define INBETWEEN_DELAY_MS 1
#define MAX_PULSES 64 // Max count of pulses per 500 ms, @ 12000 RPM you have 100 injections/sec or 50 injections per 500 ms, so 64 is way more than I will ever need (Corsa RPM limit 6-7k RPM)
 
typedef struct bmp280_data_t {
    float amb_temp;             // [°C] Ambient (cabin) temperature
    float baro_pressure;        // [Pa] Ambient barometric pressure
} bmp280_data_t;

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

/* Getter/setter for fuel_stats */

const fuel_stats_t *get_stats(void);

void set_stats(const fuel_stats_t *set_stats);

/* Inits */

void init_pulse_width_gpio(void);

void init_bmp280_sensor(void *pvParameters);

/* Fuel Meter tasks */

void fuel_meter_task(void *pvParameters);

void current_page_task(void *pvParameters);

void display_task(void *pvParameters);

#endif