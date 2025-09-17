/*
 *  Copyright (c) 2015, Ivor Wanders
 *  MIT License, see the LICENSE.md file in the root folder.
*/

#ifndef OBD9141_H
#define OBD9141_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "driver/uart.h"                        // Change this to your framework's equivalent header file
#include "driver/gpio.h"                        // Change this to your framework's equivalent header file
#include "esp_log.h"                            // Change this to your framework's equivalent header file

#define RX_PIN GPIO_NUM_16                      // Change this to your board's UART RX pin
#define TX_PIN GPIO_NUM_17                      // Change this to your board's UART TX pin
#define UART_NUM UART_NUM_2                     // Change this to your UART of choice
#define OBD_SERIAL_DATA_TYPE uart_port_t        // Change this to your framework's equivalent type

// Change this function's contents to your framework's millisecond-precision delay function (preferrably non-blocking)
void OBD9141_delay(uint32_t ms);
// Change this function's contents to your framework's equivalent UART init
void OBD9141_uart_init(void);
// Change this function's contents to your framework's equivalent UART deinit
void OBD9141_uart_deinit(void);
// Change this function's contents to your framework's equivalent UART check
// bool OBD9141_uart_is_driver_installed(OBD_SERIAL_DATA_TYPE serial_port);
// Change this function's contents to your framework's equivalent UART write function
int  OBD9141_uart_write_bytes(OBD_SERIAL_DATA_TYPE serial_port, void *b, size_t len);
// Change this function's contents to your framework's equivalent UART read function
int  OBD9141_uart_read_bytes(OBD_SERIAL_DATA_TYPE serial_port, void *b, size_t len, size_t timeout_ms);
// Change this function's contents to your framework's equivalent GPIO mode function
void OBD9141_set_pin_mode(int pin, int mode);
// Change this function's contents to your framework's equivalent GPIO level function
void OBD9141_set_pin_level(int pin, int level);
// Change this function's contents to your framework's equivalent UART driver check function
bool OBD9141_uart_is_driver_installed(OBD_SERIAL_DATA_TYPE serial_port);


// Use to enable debug print logs.
// #define OBD9141_DEBUG

#define LOW 0  // GPIO state
#define HIGH 1 // GPIO state

#define K_LINE TX_PIN

#define OBD9141_KLINE_BAUD 10400 
// as per spec.

#define OBD9141_BUFFER_SIZE 16
// maximum possible as per protocol is 256 payload, the buffer also contains
// request and checksum, add 5 + 1 for those on top of the max desired length.
// User needs to guarantee that the ret_len never exceeds the buffer size.

#define OBD9141_INTERSYMBOL_WAIT 5
// Milliseconds delay between writing of subsequent bytes on the bus.
// Is 5ms according to the specification.


// When data is sent over the serial port to the K-line transceiver, an echo of
// this data is heard on the Rx pin; this determines the timeout of readBytes
// used after sending data and trying to read the same number of bytes from the
// serial port to discard the echo.
// The total timeout for reading the echo is:
// (OBD9141_REQUEST_ECHO_TIMEOUT_MS * sent_len + 
//                      OBD9141_WAIT_FOR_ECHO_TIMEOUT) milliseconds.
#define OBD9141_WAIT_FOR_ECHO_TIMEOUT 5
// Time added to the timeout to read the echo.
#define OBD9141_REQUEST_ECHO_MS_PER_BYTE 3
// Time added per byte sent to wait for the echo.


// When a request is to be made, request bytes are pushed on the bus with
// OBD9141_INTERSYMBOL_WAIT delay between them. After the transmission of the
// request has been completed, there is a delay of OBD9141_AFTER_REQUEST_DELAY
// milliseconds. Then the readBytes function is used  with a timeout of
// (OBD9141_REQUEST_ANSWER_MS_PER_BYTE * ret_len + 
//                      OBD9141_WAIT_FOR_REQUEST_ANSWER_TIMEOUT) milliseconds.

#define OBD9141_REQUEST_ANSWER_MS_PER_BYTE 3
// The ECU might not push all bytes on the bus immediately, but wait several ms
// between the bytes, this is the time allowed per byte for the answer

#define OBD9141_WAIT_FOR_REQUEST_ANSWER_TIMEOUT (30 + 20)
// Time added to the read timeout when reading the response to a request. 
// This should incorporate the 30 ms that's between the request and answer
// according to the specification.



#define OBD9141_INIT_IDLE_BUS_BEFORE 3000
// Before the init sequence; the bus is kept idle for this duration in ms.

#define OBD9141_INIT_POST_INIT_DELAY 50
// This is a delay after the initialisation has been completed successfully.
// It is not present in the spec, but prevents a request immediately after the
// init has succeeded when the other side might not yet be ready.

typedef struct OBD9141_t{
    OBD_SERIAL_DATA_TYPE serial_port;
    bool use_kwp;
    uint8_t buffer[OBD9141_BUFFER_SIZE];
} OBD9141_t;

void OBD9141_begin(void);
// begin function which allows setting the serial port and pins.

bool OBD9141_get_current_pid(uint8_t pid, uint8_t return_length);
// The standard PID request on the current state, this is what you
// probably want to use.
// actually calls: OBD9141_get_pid(pid, 0x01, return_length)

bool OBD9141_get_pid(uint8_t pid, uint8_t mode, uint8_t return_length);
// Sends a request containing {0x68, 0x6A, 0xF1, mode, pid}
// Returns whether the request was answered with a correct answer
// (correct PID and checksum)

/**
 * @brief Send a request to the ECU, includes header bytes. For KWP the
 *        first two header bytes will be corrected before transmission.
 * @param request Pointer to the request bytes.
 * @param request_len The number of bytes that make up the request.
 * @param ret_len The expected return length.
 *
 * Sends buffer at request, up to request_len, adds a checksum.
 * Needs to know the returned number of bytes, checks if the appropiate
 * length was returned and if the checksum matches.
 * User needs to ensure that the ret_len never exceeds the buffer size.
 * if initKWP has been called, the requestKWP will be called.
 */
bool OBD9141_request(void* request, uint8_t request_len, uint8_t ret_len);
bool OBD9141_request_9141(void* request, uint8_t request_len, uint8_t ret_len);

/**
 * @brief Send a request with a variable number of return bytes.
 * @param request The pointer to read the address from.
 * @param request_len the length of the request.
 * @return the number of bytes read if checksum matches.
 * @note If checksum doesn't match return will be zero, but bytes will
 *       still be written to the internal buffer.
 */
uint8_t OBD9141_request_var_ret_len(void* request, uint8_t request_len);

/**
 * @brief Send a request and read return bytes according to KWP protocol
 * @param request The pointer to read the address from.
 * @param request_len the length of the request.
 * @return the number of bytes read if checksum matches.
 * @note If checksum doesn't match return will be zero, but bytes will
 *       still be written to the internal buffer.
 */
uint8_t OBD9141_request_kwp(void* request, uint8_t request_len);

// The following functions only work to read values from PID mode 0x01
uint8_t OBD9141_read_uint8(void); // returns right part from the buffer as uint8_t
uint16_t OBD9141_read_uint16(void); // idem...
uint32_t OBD9141_read_uint32(void);
uint8_t OBD9141_read_uint8_idx(uint8_t index); // returns byte on index.

/**
 * @brief This function allows raw access to the buffer, the return header
 *        is 4 bytes, so data starts on index 4.
 */
uint8_t OBD9141_read_buffer(uint8_t index);

/**
 * @brief Obtain the two bytes representing the trouble code from the
 *        buffer.
 * @param index The index of the trouble code.
 * @return Two byte data to be used by decodeDTC.
 */
uint16_t OBD9141_get_trouble_code(uint8_t index);

void OBD9141_set_port(bool enabled);
// need to disable the port before init.
// need to enable the port if we want to skip the init.

bool OBD9141_init(void); // attempts 'slow' ISO9141 5 baud init.
bool OBD9141_init_kwp(void);  // attempts kwp2000 fast init.
bool OBD9141_init_kwp_slow(void); // attempts 'slow' 5 baud kwp init, v1 = 233, v2 = 143.
// returns whether the procedure was finished correctly.
// The struct keeps no track of whether this was successful or not.
// It is up to the user to ensure that the initialisation is called.

bool OBD9141_clear_trouble_codes(void);
// Attempts to Clear trouble codes / Malfunction indicator lamp (MIL)
// Check engine light.
// Returns whether the request was successful.

/**
 * @brief Attempts to read the diagnostic trouble codes using the
 *        variable read method.
 * @return The number of trouble codes read.
 */
uint8_t OBD9141_read_trouble_codes(void);   // mode 0x03, stored codes
uint8_t OBD9141_read_pending_trouble_codes(void);  // mode 0x07, pending codes

uint8_t OBD9141_checksum(void* b, uint8_t len); // public for sim. (?)

/**
 *  Decodes the two bytes at input_bytes into the diagnostic trouble
 *  code, written in printable format to output_string.
 * @param input_bytes Two input bytes that represent the trouble code.
 * @param output_string Writes 5 bytes to this pointer representing the
 *        human readable DTC string.
 */
void OBD9141_decode_dtc(uint16_t input_bytes, uint8_t* output_string);
#endif