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