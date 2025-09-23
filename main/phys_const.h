#ifndef __PHYS_CONST_H
#define __PHYS_CONST_H

/* Physical constants */

#define R_AIR 287.05f // Air gas constant, [J/kg.K]
#define TO_KELVIN(celsius) ((celsius) + 273)
#define P_BAROMETRIC_BASELINE 100000 // [Pa], 1 bar, assuming we're at sea level


/* Car constants (2005 Opel Corsa, engine is Z12XEP) */ 

#define V_Z12XEP 1229 // cubic cm 
#define N_CYL 4
#define V_CYL (V_Z12XEP / N_CYL)
#define FUEL_RAIL_PRESSURE 400000 // [Pa], manometric pressure (5 bar absolute if ambient is 1 bar)

/* Fuel Injector Constants */

#define STATIC_FLOW_PRESSURE 400000     // [Pa], pressure across the injector for the static flow given below (fuel rail absolute pressure (5 bar) - ambient pressure (1 bar))
#define STATIC_FLOW_RATE_ML_MIN 154                     // [mL/min] @ 4.0 bar across the injector                                                                                            (measured on fuel injector testing stand)
#define STATIC_FLOW_RATE (STATIC_FLOW_RATE_ML_MIN / 60) // [mL/s] or [uL/ms] @ 4.0 bar across the injector (get_fuel_coeff() corrects for current pressure across injector (varies with MAP) (calculated)
#define INJECTOR_FULL_OPENING_TIME 1400 // [us], time taken for injector to fully open (hit top needle position)                                                                             (measured with oscilloscope and vibration sensor)
#define INJECTOR_FULL_CLOSING_TIME 600  // [us], time taken after pulse end for injector to fully close (hit bottom needle position)                                                         (measured with oscilloscope and vibration sensor)
#define INJECTOR_DEADTIME 750           // [us], time taken for current buildup and magnetic force to overcome spring force and cause lifting of needle and non-zero fuel flow               (estimated)
#define INJECTOR_RAMP_UP_TIME (INJECTOR_FULL_OPENING_TIME - INJECTOR_DEADTIME) // [us], time taken for fuel flow to ramp up from 0 to STATIC_FLOW_RATE                                       (estimated)
#define INJECTOR_RAMP_DOWN_TIME INJECTOR_FULL_CLOSING_TIME                     // [us], same thing, using for consistency with ramp-up time                                                  (estimated)


/* Manifold Absolute Pressure LUT */
#define MAP_DEFAULT 60000 // [Pa], default fallback value if we can't get data for MAP, minimises error (4.4 bar delta across injector, +-4% error in injector fuel flow)

#define N_RPM_BINS 8
#define N_LOAD_BINS 6

static const uint16_t rpm_bp[N_RPM_BINS]  = {500, 1000, 2000, 3000, 4000, 5000, 6000, 7000}; // RPM
static const uint8_t  load_bp[N_LOAD_BINS] = {0, 20, 40, 60, 80, 100};                      // Load, [%]

static const uint32_t map_table[N_RPM_BINS][N_LOAD_BINS] = {
    {30000, 35000, 40000, 45000, 50000, 55000}, // 500 RPM (idle/stall region)
    {30000, 35000, 45000, 50000, 55000, 60000}, // 1000 RPM
    {30000, 40000, 50000, 60000, 70000, 80000}, // 2000 RPM
    {30000, 45000, 55000, 65000, 80000, 90000}, // 3000 RPM
    {30000, 50000, 60000, 75000, 90000,100000}, // 4000 RPM
    {30000, 50000, 65000, 80000, 95000,100000}, // 5000 RPM
    {30000, 50000, 65000, 85000,100000,100000}, // 6000 RPM
    {30000, 50000, 70000, 90000,100000,100000}  // 7000 RPM
};

// /* Volumetric Efficiency LUT */

// // Breakpoints
// static const uint16_t rpm_bp[]   = {500, 1000, 2000, 3000, 4000, 5000, 6000, 7000};
// static const uint8_t  load_bp[]  = {0, 20, 40, 60, 80, 100};

// // VE table: [rpm][load], values = VE * 1000
// static const uint16_t ve_table[8][6] = {
//     {250, 300, 350, 420, 480, 520},  // 500 rpm 
//     {300, 380, 480, 580, 680, 730},  // 1000 rpm
//     {320, 450, 600, 700, 800, 850},  // 2000 rpm
//     {350, 500, 700, 800, 850, 890},  // 3000 rpm
//     {360, 540, 760, 860, 900, 920},  // 4000 rpm
//     {340, 500, 700, 820, 860, 880},  // 5000 rpm
//     {300, 450, 650, 780, 820, 850},  // 6000 rpm
//     {260, 420, 600, 720, 760, 780},  // 7000 rpm
// };

#endif