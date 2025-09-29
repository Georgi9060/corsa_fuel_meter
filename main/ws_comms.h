#ifndef __SEND_DATA_PACK
#define __SEND_DATA_PACK

#include "websocket.h"
#include "esp_log.h"
#include "fm_tasks.h"
#include "nvs.h"

// #define COMMS_DEBUG // uncomment to enable WS communications debug

typedef struct __attribute__((packed)){
    uint8_t load;           // [%]
    int16_t coolant_temp;   // [°C]
    uint16_t rpm;           // [RPM]
    uint8_t speed;          // [km/h]
    int16_t intake_temp;    // [°C]
    float maf;              // [g/s]
    uint8_t throttle;       // [%]
    uint8_t attempt_cntr;
    uint8_t success_cntr;
    bool can_calc_map;      // False if RPM/Load requests get no response
} comms_data_pack_t; // Live data read from Corsa's KWP

typedef struct __attribute__((packed)){
    float inst_fuel;        // [L/100 km]
    float avg_fuel;         // [L/100 km]
    float dist_tr;          // [m]
    float cons_fuel;        // [L]
    uint16_t rpm;           // [RPM]
    uint8_t speed;          // [km/h]
    uint16_t pcnt_isr;      // [-]
    uint16_t pcnt_rpm;      // [-]
    int16_t pdelta;         // [-]
    float avg_pwidth;       // [ms]
    float amb_temp;         // [°C]
    float baro_pressure;    // [kPa]

} debug_fuel_data_pack_t; // In-depth data for debugging

typedef struct __attribute__((packed)){
    float inst_fuel;        // [L/100 km]
    float avg_fuel;         // [L/100 km]
    int16_t coolant_temp;   // [°C]
    float cons_fuel;        // [L]
    float fuel_last_6;      // [mL]
    float fuel_last_60;     // [mL]
} fuel_data_pack_t; // Brief data, what the whole project is about

/* Send */

void send_comms_data_pack(comms_data_pack_t data);

void send_debug_fuel_data_pack(debug_fuel_data_pack_t data);

void send_fuel_data_pack(fuel_data_pack_t data);

/* Receive */

void set_open_page(cJSON *root);

void load_fuel_data(void);

void save_add_fuel_data(void);

void save_ovw_fuel_data(void);

void clear_fuel_data(void);

void delete_fuel_data(void);

#endif