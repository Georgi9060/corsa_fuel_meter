#ifndef __SEND_DATA_PACK
#define __SEND_DATA_PACK

#include "websocket.h"
#include "esp_log.h"

// #define COMMS_DEBUG // uncomment to enable WS communications debug

typedef struct __attribute__((packed)){
    uint8_t load;
    int16_t coolant_temp;
    uint16_t rpm;
    uint8_t speed;
    int16_t intake_temp;
    int16_t maf;
    uint8_t throttle;
    uint8_t attempt_cntr;
    uint8_t success_cntr;
} comms_data_pack_t; // Live data read from Corsa's KWP

typedef struct __attribute__((packed)){
    float inst_fuel;
    float avg_fuel;
    int16_t coolant_temp;
    float cons_fuel;
    uint16_t rpm;
    uint8_t speed;
    int16_t pcnt_rpm;
    int16_t pcnt_isr;
    float pdelta;
    float pdeltapc;
} debug_fuel_data_pack_t; // In-depth data for debugging

typedef struct __attribute__((packed)){
    float inst_fuel;
    float avg_fuel;
    int16_t coolant_temp;
    float cons_fuel;
} fuel_data_pack_t; // Brief data, what the whole project is about

// Send commands
void send_comms_data_pack(comms_data_pack_t data);

void send_debug_fuel_data_pack(debug_fuel_data_pack_t data);

void send_fuel_data_pack(fuel_data_pack_t data);

// Receive commands
void set_open_page(cJSON *root);

#endif