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

#ifndef __NVS_H
#define __NVS_H

#include "nvs_flash.h"
#include "esp_log.h"

void init_nvs(void);

/* Getter functions */

// [uL]
double get_fuel_consumed(void);

// [m]
double get_dist_tr(void);

/* Setter functions */

// [uL]
void set_fuel_consumed(double val);

// [m]
void set_dist_tr(double val);

#endif