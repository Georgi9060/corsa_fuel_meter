#include "fm_tasks.h"

static portMUX_TYPE pulse_spinlock = portMUX_INITIALIZER_UNLOCKED;
static volatile uint16_t pulse_count_isr = 0;
static uint32_t pulse_buffer[MAX_PULSES];

TaskHandle_t fuel_meter_task_handle = NULL;
TaskHandle_t current_page_task_handle = NULL;

SemaphoreHandle_t fuel_data_mutex = NULL; // Protects fuel_meter_task and data sending tasks from mutual access

/* Fuel meter data */
static fuel_stats_t stats = {0};                // Stores runtime fuel statistics
static uint16_t local_pulse_count = 0;         // Stores the pulse count accumulated every 600 ms
static uint64_t avg_pulse_width = 0;          // Stores the average pulse width calculated every 600 ms
static bool convert_to_currency = false;     // Whether the displayed data will be in litres or local currency (BGN/EUR)
static float price_per_litre = 1;           // In local currency, default is 1 so if we don't enter a price, nothing changes // TODO: MOVE TO WEB, THIS SHOULD NOT BE DONE BY ESP32!
static comms_data_pack_t car_data = {0};   // Stores the retrieved data from KWP comms, accessed by multiple tasks

char currently_open_page[32] = {0};      // used to indicate which type of packet to prepare & send

static const char *TAG = "fm_tasks";

// ISR handler for both edges
static void IRAM_ATTR injector_isr_handler(void* arg) {
    static volatile uint64_t fall_time_us = 0;
    int level = gpio_get_level(INJECTOR_PIN);
    uint64_t now = esp_timer_get_time();

    if (level == 0) {
        // Falling edge: start timing
        fall_time_us = now;
    } else {
        // Rising edge: stop timing
        if (fall_time_us > 0) {
            uint64_t duration = now - fall_time_us;
            // Filter anything below injector deadtime (injector will not physically open)
            if (duration > INJECTOR_DEADTIME && pulse_count_isr < MAX_PULSES) {
                taskENTER_CRITICAL_ISR(&pulse_spinlock);
                pulse_buffer[pulse_count_isr++] = (uint32_t)duration;
                taskEXIT_CRITICAL_ISR(&pulse_spinlock);
            }
        }
    }
}

// Init injector pulse measurements
void init_pulse_width_gpio(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << INJECTOR_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INJECTOR_PIN, injector_isr_handler, NULL));
    ESP_LOGI(TAG, "Ready to measure injector pulses on GPIO %d...", INJECTOR_PIN);
}

// Get data for fuel meter from KWP comms
static comms_data_pack_t get_car_data(void) {
    bool res;
    comms_data_pack_t data = car_data; // Takes the last period's data (if any requests fail, we fall back to the last valid data, and if it's the first time, we just assume 0)
    data.can_calc_map = true;

    // Load [%]
    res = OBD9141_get_current_pid(0x04, 1);
    data.attempt_cntr++;
    if(res){
        data.load = OBD9141_read_uint8() * 100 / 255;
        data.success_cntr++;
    }
    else{data.can_calc_map = false;}
    OBD9141_delay(INBETWEEN_DELAY_MS);

    // Engine Coolant Temperature [°C]
    res = OBD9141_get_current_pid(0x05, 1);
    data.attempt_cntr++;
    if (res){
        data.coolant_temp = OBD9141_read_uint8() - 40;
        data.success_cntr++;
    }
    OBD9141_delay(INBETWEEN_DELAY_MS);

    // RPM
    res = OBD9141_get_current_pid(0x0C, 2);
    data.attempt_cntr++;
    if (res){
        data.rpm = OBD9141_read_uint16() / 4;
        data.success_cntr++;
    }
    else{data.can_calc_map = false;}
    OBD9141_delay(INBETWEEN_DELAY_MS);

    // Vehicle Speed [km/h]
    res = OBD9141_get_current_pid(0x0D, 1);
    data.attempt_cntr++;
    if (res){
        data.speed = OBD9141_read_uint8();
        data.success_cntr++;
    } else{data.speed = 0;} // Do not leave old speed data so you don't assume distance travelled but only record fuel consumed
    OBD9141_delay(INBETWEEN_DELAY_MS);

    // Intake Air Temperature [°C]
    res = OBD9141_get_current_pid(0x0F, 1);
    data.attempt_cntr++;
    if (res){
        data.intake_temp = OBD9141_read_uint8() - 40;
        data.success_cntr++;
        data.read_iat = true; // differentiate no response from actual 0 degrees Celsius
    }
    // else{data.can_calc_map = false;}
    OBD9141_delay(INBETWEEN_DELAY_MS);

    // Mass Air Flow [g/s]
    res = OBD9141_get_current_pid(0x10, 2);
    data.attempt_cntr++;
    if (res){
        data.maf = OBD9141_read_uint16() / 100.0f;
        data.success_cntr++;
    }
    // else{data.can_calc_map = false;}
    OBD9141_delay(INBETWEEN_DELAY_MS);

    // Throttle [%]
    res = OBD9141_get_current_pid(0x11, 1);
    data.attempt_cntr++;
    if (res){
        data.throttle = OBD9141_read_uint8() * 100 / 255;
        data.success_cntr++;
    }
    // else{data.can_calc_map = false;}

    if(data.success_cntr != data.attempt_cntr){
        ESP_LOGW(TAG, "success_cntr != attempt_cntr: %d/%d", data.success_cntr, data.attempt_cntr);
    }
    return data;
}

// // Get Volumetric Efficiency (for MAP, from VE LUT)
// static float get_ve(uint16_t load, uint16_t rpm) {
//     const int nx = sizeof(load_bp) / sizeof(load_bp[0]);
//     const int ny = sizeof(rpm_bp) / sizeof(rpm_bp[0]);

//     // Clamp rpm at table range
//     if (rpm <= rpm_bp[0]) rpm = rpm_bp[0];     // clamp to 500 rpm
//     if (rpm >= rpm_bp[ny -1 ]) rpm = rpm_bp[ny - 1]; // clamp to 7000 rpm

//     // Clamp load at table range
//     if (load <= load_bp[0]) load = load_bp[0];
//     if (load >= load_bp[nx - 1]) load = load_bp[nx - 1];

//     // Find load indices
//     int ix = 0;
//     while (ix < nx - 2 && load > load_bp[ix+1]) ix++;
//     int ix1 = ix, ix2 = ix + 1;

//     uint16_t x1 = load_bp[ix1];
//     uint16_t x2 = load_bp[ix2];
//     uint16_t dx = x2 - x1;
//     uint16_t fx = (dx ? ((load - x1) * 1000) / dx : 0);

//     // Find rpm indices
//     int iy = 0;
//     while (iy < ny - 2 && rpm > rpm_bp[iy + 1]) iy++;
//     int iy1 = iy, iy2 = iy + 1;

//     uint16_t y1 = rpm_bp[iy1];
//     uint16_t y2 = rpm_bp[iy2];
//     uint16_t dy = y2 - y1;
//     uint16_t fy = (dy ? ((rpm - y1) * 1000) / dy : 0);

//     // Get 4 surrounding VE values (x1000)
//     uint32_t v11 = ve_table[iy1][ix1];
//     uint32_t v12 = ve_table[iy1][ix2];
//     uint32_t v21 = ve_table[iy2][ix1];
//     uint32_t v22 = ve_table[iy2][ix2];

//     // Bilinear interpolation
//     uint32_t v1 = v11 + ((v12 - v11) * fx) / 1000;
//     uint32_t v2 = v21 + ((v22 - v21) * fx) / 1000;
//     uint32_t v  = v1  + ((v2  - v1 ) * fy) / 1000;

//     return (float)v / 1000.0f;
// }

// Get Manifold Absolute Pressure (for fuel injector rate)
// static uint32_t get_map(void) {
//     uint32_t map = 0;
//     if(!car_data.can_calc_map){ // We missed some data, must work around this
//         // TODO: Calculate MAP if some data is missing
//         //...
//         return 65000; // Return 65kPa (4.35 bar delta across injector, +-4% error in injector rate)
//     }
//     else{ // All data gathered and valid
//         /* Convert to appropriate SI units */
//         float maf = car_data.maf * 0.001f;                                          // Mass air flow          [kg/s]
//         uint16_t intake_temp = TO_KELVIN(car_data.intake_temp);                     // Intake air temp        [K]
//         uint16_t N_intakes_s = (car_data.rpm * N_CYL) / 120;                        // Air intakes per second [-]
//         if(!N_intakes_s){N_intakes_s++;}                                            // avoid dividing by 0
//         float VE = get_ve(car_data.throttle, car_data.rpm);                         // Volumetric efficiency  [-]
//         float V_cyl = V_CYL * 0.000001f;                                            // Cylinder volume        [m^3]

//         map = (maf * R_AIR * intake_temp) / (VE * V_cyl * N_intakes_s);             // Manifold absolute pressure [Pa]
//     }
//     return map;
// }

static uint32_t get_map(uint16_t load, uint16_t rpm) {
    // Clamp rpm/load
    if (rpm <= rpm_bp[0]) rpm = rpm_bp[0];
    if (rpm >= rpm_bp[N_RPM_BINS - 1]) rpm = rpm_bp[N_RPM_BINS - 1];
    if (load <= load_bp[0]) load = load_bp[0];
    if (load >= load_bp[N_LOAD_BINS - 1]) load = load_bp[N_LOAD_BINS - 1];

    // Find load indices
    int ix = 0;
    while (ix < N_LOAD_BINS - 2 && load > load_bp[ix + 1]) ix++;
    int ix1 = ix, ix2 = ix + 1;

    uint16_t x1 = load_bp[ix1];
    uint16_t x2 = load_bp[ix2];
    uint16_t dx = x2 - x1;
    uint16_t fx = dx ? ((load - x1) * 1000) / dx : 0;

    // Find rpm indices
    int iy = 0;
    while (iy < N_RPM_BINS - 2 && rpm > rpm_bp[iy + 1]) iy++;
    int iy1 = iy, iy2 = iy + 1;

    uint16_t y1 = rpm_bp[iy1];
    uint16_t y2 = rpm_bp[iy2];
    uint16_t dy = y2 - y1;
    uint16_t fy = dy ? ((rpm - y1) * 1000) / dy : 0;

    // 4 surrounding MAP values
    uint32_t m11 = map_table[iy1][ix1];
    uint32_t m12 = map_table[iy1][ix2];
    uint32_t m21 = map_table[iy2][ix1];
    uint32_t m22 = map_table[iy2][ix2];

    // Bilinear interpolation
    uint32_t m1 = m11 + ((m12 - m11) * fx) / 1000;
    uint32_t m2 = m21 + ((m22 - m21) * fx) / 1000;
    uint32_t map = m1 + ((m2 - m1) * fy) / 1000;

    return map; // in Pa
}

static double get_fuel_coeff(uint32_t map) {
    uint32_t p_barometric = P_BAROMETRIC_BASELINE;    // TODO: Take into account barometric pressure when I install BMP280 sensor
    double p_ratio = p_barometric / P_BAROMETRIC_BASELINE;

    uint32_t p_across_inj = FUEL_RAIL_PRESSURE + (p_barometric - map);  // Current pressure across injector, can vary between 4.0 bar @ WOT and 4.7 bar @ idle
    double deltap_coeff = sqrt(p_across_inj / STATIC_FLOW_PRESSURE);    // Coeffcient for static fuel flow rate @ measured pressure across injector, based on delta-P square law

    double coeff = STATIC_FLOW_RATE * deltap_coeff    * p_ratio;
    // [uL/ms]   = [uL/ms]          * (1.00 ~ 1.08)   * (0.52 ~ 1.05)
    //  coeff    = flow @ 4.0 bar   * (4.0 ~ 4.7 bar) * (0.52 ~ 1.05 bar)
    //                        pressure across injector  atmospheric pressure
    return coeff;
}

static double get_pulse_fuel(uint32_t pulse_width, double fuel_coeff) {
    double pulse_fuel = 0;
    if(pulse_width >= INJECTOR_FULL_OPENING_TIME) { // Pulse has full ramp-up and ramp-down
        uint32_t static_flow_width = pulse_width - INJECTOR_FULL_OPENING_TIME;
        pulse_fuel = ((INJECTOR_RAMP_UP_TIME * 0.5) + static_flow_width + (INJECTOR_RAMP_DOWN_TIME * 0.5)) * 0.001 * fuel_coeff; // [uL]
        //           (          [us]              +       [us]        +            [us]              )     * 0.001 * [uL/ms] = [uL]
        //                                          [us] * 0.001 = [ms]

        // Ramp-up/ramp-down fuel amount is calculated as area of right triangle formed by injector ramp-up/ramp-down time on x-axis, 
        // static flow rate coefficient on y-axis, and the change of flow rate over ramp time as a linear function
        /*

        Injector pulse, [ms]
        /\ 
        |  start                 end
    HIGH|_____                    __________________
        |     |                  |
        |     |                  |
     LOW|     |__________________|

        Fuel flow rate, [uL/ms]
        /\
        |start of pulse__________v--- end of pulse
        |     |       /|         |\
        |     |      / |coeff    | \
        |_____v_____/__|_________|__\___________> time, [ms]
              dead  ramp-up      ramp-down
              time  time         time      

        Ramp-up time + Static flow time + Ramp-down time
        
        Area of trapezoid is total fuel injected! 

        */

    }
    else{ // Pulse has partial ramp-up and ramp-down
        double full_ramp_up_fuel = ((INJECTOR_RAMP_UP_TIME * 0.5) * fuel_coeff) * 0.001;
        double full_ramp_down_fuel = ((INJECTOR_RAMP_DOWN_TIME * 0.5) * fuel_coeff) * 0.001;

        uint32_t ramp_up_width = pulse_width - INJECTOR_DEADTIME;
        double partial_coeff = pow((ramp_up_width / INJECTOR_RAMP_UP_TIME), 2);

        double partial_ramp_up_fuel = partial_coeff * full_ramp_up_fuel;
        double partial_ramp_down_fuel = partial_coeff * full_ramp_down_fuel;
        pulse_fuel = partial_ramp_up_fuel + partial_ramp_down_fuel;

        // Partial ramp-up/ramp-down pulses end up being triangles proportional to the 100% triangle ramps from above
        // their sides are in a ratio of (partial/full), which is in the range (0; 1), and their areas (fuel injected) are proportional to
        // the full ramp triangles in a ratio of (partial/full)^2.
        // Example: if full ramp-up time is 0.650 ms, partial ramp-up time is 0.325 ms, their ratio is 0.325/0.650 = 1/2;
        // so their areas (and the injected fuel) are in the ratio (1/2)^2 = 1/4.

        /*

        Injector pulse, [ms]
        /\ 
        |   start         end
    HIGH|_______           __________________
        |       |         |
        |       |         |
     LOW|       |_________|

        Fuel flow rate, [uL/ms]
        /\
        |start of pulse   v--- end of pulse
        |       |        /|\
        |       |       / | \
        |_______v______/__|__\___________> time, [ms]
                 dead ramp ramp
                 time up   down
                      time time      
        Partial ramp-up time + partial ramp-down time

        Area of triangles is total fuel injected! 

        */
    }
    return pulse_fuel;
}

/* Get current page's data pack */

static comms_data_pack_t get_comms_data_pack(void) {
    return car_data;
}

static debug_fuel_data_pack_t get_debug_fuel_data_pack(void) {
    debug_fuel_data_pack_t data_pack = {0};
    fuel_stats_t local_stats = {0};
    comms_data_pack_t local_car_data = {0};
    uint16_t local_local_pulse_count = 0;
    uint64_t local_avg_pulse_width = 0;
    // Copy locally to prevent overwrites
    if(xSemaphoreTake(fuel_data_mutex, pdMS_TO_TICKS(100))){
        local_stats = stats;
        local_car_data = car_data;
        local_local_pulse_count = local_pulse_count;
        local_avg_pulse_width = avg_pulse_width;
        xSemaphoreGive(fuel_data_mutex);
    }
    data_pack.inst_fuel = local_stats.fuel_cons_inst;
    data_pack.avg_fuel = local_stats.fuel_cons_avg;
    data_pack.coolant_temp = local_car_data.coolant_temp;
    data_pack.cons_fuel = local_stats.fuel_consumed * 0.000001;       // [uL] to [L]
    data_pack.rpm = local_car_data.rpm;
    data_pack.speed = local_car_data.speed;
    data_pack.pcnt_isr = local_local_pulse_count;
    // Injections per 600 ms, as expected from RPM
    // revs/min / 60 s = revs/sec; revs/sec / 2 (because every other rotation has an injection) and * 0.6 because revs/0.6 sec
    data_pack.pcnt_rpm = (int16_t)lround(local_car_data.rpm / 60 / 2 * 0.6);
    data_pack.pdelta = data_pack.pcnt_rpm - data_pack.pcnt_isr;
    if(data_pack.pcnt_rpm == 0){data_pack.pcnt_rpm = 1;}
    data_pack.avg_pwidth = local_avg_pulse_width * 0.001; // [us] to [ms]
    return data_pack;
}

static fuel_data_pack_t get_fuel_data_pack(void) {
    fuel_data_pack_t data_pack = {0};
    fuel_stats_t local_stats = {0};
    comms_data_pack_t local_car_data = {0};
    // Copy locally to prevent overwrites
    if(xSemaphoreTake(fuel_data_mutex, pdMS_TO_TICKS(100))){
        local_stats = stats;
        local_car_data = car_data;
        xSemaphoreGive(fuel_data_mutex);
    }
    data_pack.inst_fuel = local_stats.fuel_cons_inst;
    data_pack.avg_fuel = local_stats.fuel_cons_avg;
    data_pack.coolant_temp = local_car_data.coolant_temp;
    data_pack.cons_fuel = local_stats.fuel_consumed * 0.000001;       // [uL] to [L]
    data_pack.fuel_last_6 = local_stats.fuel_cons_last_6 * 0.001;     // [uL] to [mL]
    data_pack.fuel_last_60 = local_stats.fuel_cons_last_60 * 0.001;   // [uL] to [mL]

    return data_pack;
}


/* Page handlers */

static void comms_page_handler(void) {
    comms_data_pack_t data = get_comms_data_pack();
    send_comms_data_pack(data);
}

static void debug_fuel_page_handler(void) {
    debug_fuel_data_pack_t data = get_debug_fuel_data_pack();
    send_debug_fuel_data_pack(data);
}

static void fuel_page_handler(void) {
    fuel_data_pack_t data = get_fuel_data_pack();
    send_fuel_data_pack(data);
}


/* FreeRTOS tasks */

void fuel_meter_task(void *pvParameters) {
    fuel_data_mutex = xSemaphoreCreateMutex();
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(600));
        if(xSemaphoreTake(fuel_data_mutex, pdMS_TO_TICKS(100))){
            
        
/* ---------------------------------- Gather data ----------------------------------------------- */

            // Get data from Corsa over KWP
            car_data = get_car_data();

            // Snapshot data locally for safe calculations 
            uint32_t local_pulse_buffer[MAX_PULSES];
            taskENTER_CRITICAL(&pulse_spinlock);
            local_pulse_count = pulse_count_isr;
            pulse_count_isr = 0;
            memcpy(local_pulse_buffer, pulse_buffer, local_pulse_count * sizeof(uint32_t));
            taskEXIT_CRITICAL(&pulse_spinlock);

            // Get MAP for fuel injected calculations
            uint32_t map = MAP_DEFAULT;
            if(car_data.can_calc_map){
                map = get_map(car_data.load, car_data.rpm);
            }

            double fuel_coeff = get_fuel_coeff(map);

            // Fuel consumed during this 600 ms period
            double period_fuel_cons = 0; // in [uL] (microlitres)
            avg_pulse_width = 0; // Reset the avg every 600 ms
            for(size_t i = 0; i < local_pulse_count; i++){
                double pulse_fuel = get_pulse_fuel(local_pulse_buffer[i], fuel_coeff); // [uL]
                period_fuel_cons += pulse_fuel * N_CYL; // For all 4 cylinders, we assume the same pulse width across all cylinders in a given 4-stroke cycle
                avg_pulse_width += local_pulse_buffer[i];
            }
            avg_pulse_width /= local_pulse_count;

            // Distance travelled during this 600 ms period
            double speed_m_s = car_data.speed / 3.6; // [m/s]
            double dist_tr_m = speed_m_s * 0.600;    // [m/s * 0.6 s]

/* ---------------------------------- Update stats ----------------------------------------------- */

            /* Total fuel consumed and distance travelled since boot */
            stats.fuel_consumed += period_fuel_cons;
            stats.dist_tr += dist_tr_m;
            
            /* Instantaneous and average fuel consumption */ 
            if ((dist_tr_m < 0.1) || (stats.dist_tr < 0.1)) { // Car is stationary (0 m travelled in this 600 ms period)
                stats.fuel_cons_inst = -1; // Avoid division by 0 or nonsensical values
            } else { // Car is moving so we can calculate an actual instantaneous fuel consumption
                stats.fuel_cons_inst = period_fuel_cons / dist_tr_m * 0.1; // [L/100 km]
                // 1 [uL/m] = 1 [mL/km] = 100 [mL/100 km] = 0.1 [L/100 km]
            }
            if(stats.dist_tr < 0.1){ // Car hasn't moved yet (0 m since boot)
                stats.fuel_cons_avg = -1; // Avoid division by 0 or nonsensical values
            } else{ // Car has travelled non-zero distance so we can 
                stats.fuel_cons_avg = stats.fuel_consumed / stats.dist_tr * 0.1; // [L/100 km]
                // 1 [uL/m] = 1 [mL/km] = 100 [mL/100 km] = 0.1 [L/100 km]
            }

            /* Running sum of fuel consumed last 6 and 60 seconds */
            static double fuel_last_6[10] = {0};    // Stores last 6 seconds'  fuel amounts (in 600 ms intervals)
            static double fuel_last_60[100] = {0};  // Stores last 60 seconds' fuel amounts (in 600 ms intervals)
            static size_t index_6 = 0;
            static size_t index_60 = 0;

            // Remove oldest val
            stats.fuel_cons_last_6  -= fuel_last_6[index_6];
            stats.fuel_cons_last_60 -= fuel_last_60[index_60];
            // Possible rounding error fix
            if(stats.fuel_cons_last_6 < 0) {stats.fuel_cons_last_6 =  0;} 
            if(stats.fuel_cons_last_60 < 0){stats.fuel_cons_last_60 = 0;}
            // Store newest val
            fuel_last_6[index_6] = period_fuel_cons;
            fuel_last_60[index_60] = period_fuel_cons;
            // Add newest to sum
            stats.fuel_cons_last_6 += period_fuel_cons;
            stats.fuel_cons_last_60 += period_fuel_cons;
            // Wrap around array
            index_6 = (index_6 + 1) % 6;
            index_60 = (index_60 + 1) % 60;
/* ----------------------------------Fuel Meter data done ----------------------------------------------- */
            xSemaphoreGive(fuel_data_mutex);
        }
        xTaskNotifyGive(current_page_task_handle);
    }
}

void current_page_task(void *pvParameters) {
    while (1) {
        // Wait for data to be ready, up to 600 ms
        uint32_t notifyCount = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(600));

        if (!notifyCount) {
            ESP_LOGW(TAG, "fuel_meter_task timeout");
            continue;
        }
        if (strcmp(currently_open_page, "comms.html") == 0) {
            comms_page_handler();
        }
        else if (strcmp(currently_open_page, "debugfuel.html") == 0) {
            debug_fuel_page_handler();
        }
        else if (strcmp(currently_open_page, "fuel.html") == 0) {
            fuel_page_handler();
        }
        else {
            // Page not relevant, ignore notification
        }
    }
}
