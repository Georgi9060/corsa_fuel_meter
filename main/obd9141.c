 /*
 * OBD9141_C_Core - C translation of the OBD9141 Arduino library
 *
 * Original library: OBD9141 by Ivor Wanders https://github.com/iwanders/OBD9141
 * Original library licensed under MIT License.
 *
 * Translation and adaptation to C / ESP-IDF by Georgi Georgiev, 2025.
 * Framework-agnostic template functions included for portability to other frameworks.
 * 
 * Copyright (c) 2015, Ivor Wanders
 * Copyright (c) 2025, Georgi Georgiev
 *
 * MIT License, see the LICENSE.md file in the root folder.
 */

#include "obd9141.h"

static OBD9141_t obd9141;

// Static functions (private methods of OBD9141 class in original library)

static void OBD9141_kline(bool enabled){
    OBD9141_set_pin_level(K_LINE, enabled);
}

// writes a byte and removes the echo.
static void OBD9141_write_byte(uint8_t b){
#ifdef OBD9141_DEBUG
        printf("w: 0x%02X\n", b);
#endif
    OBD9141_uart_write_bytes(obd9141.serial_port, &b, 1); // writes 1 byte

    uint8_t tmp[1]; // temporary variable to read into
    size_t timeout_ms = OBD9141_REQUEST_ECHO_MS_PER_BYTE * 1 + OBD9141_WAIT_FOR_ECHO_TIMEOUT;
    OBD9141_uart_read_bytes(obd9141.serial_port, tmp, 1, timeout_ms);
}

// writes an array and removes the echo.
static void OBD9141_write_arr(void *b, uint8_t len){
    uint8_t *bytes = (uint8_t*) b;
#ifdef OBD9141_DEBUG
    printf("w: ");
#endif
    for (uint8_t i = 0; i < len; i++) {
#ifdef OBD9141_DEBUG
        printf("0x%02X ", bytes[i]);
#endif
        OBD9141_uart_write_bytes(obd9141.serial_port, &bytes[i], 1); // writes 1 byte at a time
        OBD9141_delay(OBD9141_INTERSYMBOL_WAIT);
    }
#ifdef OBD9141_DEBUG
    printf("\n");
#endif

    uint8_t tmp[len]; // temporary variable to read into
    size_t timeout_ms = OBD9141_REQUEST_ECHO_MS_PER_BYTE * len + OBD9141_WAIT_FOR_ECHO_TIMEOUT;
    OBD9141_uart_read_bytes(obd9141.serial_port, tmp, len, timeout_ms);
}

static bool OBD9141_init_impl(bool check_v1_v2){
    obd9141.use_kwp = false;
    // this function performs the ISO9141 5-baud 'slow' init.
    OBD9141_set_port(false); // disable the port.

    OBD9141_kline(HIGH);
    OBD9141_delay(OBD9141_INIT_IDLE_BUS_BEFORE); // no traffic on bus for 3 seconds.
#ifdef OBD9141_DEBUG
    printf("Before magic 5 baud.\n");
#endif
    // next, send the startup 5 baud init..
    OBD9141_kline(LOW);     OBD9141_delay(200);   // start
    OBD9141_kline(HIGH);    OBD9141_delay(400);   // first two bits
    OBD9141_kline(LOW);     OBD9141_delay(400);   // second pair
    OBD9141_kline(HIGH);    OBD9141_delay(400);   // third pair
    OBD9141_kline(LOW);     OBD9141_delay(400);   // last pair
    OBD9141_kline(HIGH);    OBD9141_delay(200);   // stop bit
    // this last 200 ms delay could also be put in the setTimeout below.
    // But the spec says we have a stop bit.

    // done, from now on it the bus can be treated ad a 10400 baud serial port.
#ifdef OBD9141_DEBUG
    printf("Before setting port.\n");
#endif
    OBD9141_set_port(true);
#ifdef OBD9141_DEBUG
    printf("After setting port.\n");
#endif

    int ret;
    uint8_t buffer[1];
    size_t timeout_ms = 300 + 200; // wait should be between 20 and 300 ms long
    ret = OBD9141_uart_read_bytes(obd9141.serial_port, buffer, 1, timeout_ms); // read first value into buffer, should be 0x55
    if (ret > 0){
#ifdef OBD9141_DEBUG
        printf("ret: %d\n", ret);
        printf("First read is: %d\n", buffer[0]);
#endif
        if (buffer[0] != 0x55){
            return false;
        }
    }
    else {
#ifdef OBD9141_DEBUG
        printf("Timeout on read 0x55.\n");
#endif
        return false;
    }
    // we get here after we have passed receiving the first 0x55 from ecu.

    uint8_t v1 = 0, v2 = 0; // sent by car:  (either 0x08 or 0x94)

    // read v1
    timeout_ms = 20; // w2 and w3 are pauses between 5 and 20 ms
    ret = OBD9141_uart_read_bytes(obd9141.serial_port, buffer, 1, timeout_ms);
    if (ret <= 0){
#ifdef OBD9141_DEBUG
        printf("Timeout on read v1.\n");
#endif
        return false;
    }
    else {
        v1 = buffer[0];
#ifdef OBD9141_DEBUG
        printf("ret: %d\n", ret);
        printf("read v1: %d\n", v1);
#endif
    }

    // read v2
    ret = OBD9141_uart_read_bytes(obd9141.serial_port, buffer, 1, timeout_ms);
    if (ret <= 0){
#ifdef OBD9141_DEBUG
        printf("Timeout on read v2.\n");
#endif
        return false;
    }
    else {
        v2 = buffer[0];
#ifdef OBD9141_DEBUG
        printf("ret: %d\n", ret);
        printf("read v2: %d\n", v2);
#endif
    }
#ifdef OBD9141_DEBUG
    printf("v1: %d\n", v1);
    printf("v2: %d\n", v2);
#endif

    // these two should be identical according to the spec.
    if (check_v1_v2) {
        if (v1 != v2){
            return false;
        }
    }

    // we obtained w1 and w2, now invert and send it back.
    // tester waits w4 between 25 and 50 ms:
    OBD9141_delay(30);
    OBD9141_write_byte(~v2);



    timeout_ms = 50; // w5 is same as w4...  max 50 ms
    // finally, attempt to read 0xCC from the ECU, indicating successful init.
    ret = OBD9141_uart_read_bytes(obd9141.serial_port, buffer, 1, timeout_ms);
    if (ret <= 0){
#ifdef OBD9141_DEBUG
        printf("Timeout on 0xCC read.\n");
#endif
        return false;
    }
    else {
#ifdef OBD9141_DEBUG
        printf("ret: %d\n", ret);
        printf("read 0xCC?: 0x%02X\n", buffer[0]);
#endif
        if ((buffer[0] == 0xCC)){ // done if this is inverse of 0x33
           OBD9141_delay(OBD9141_INIT_POST_INIT_DELAY);
            // this delay is not in the spec, but prevents requests immediately
            // after the finishing of the init sequency.

            return true; // yay! we are initialised.
        }
        else {
            return false;
        }
    }
}




// Change these functions to your framework's equivalents

void OBD9141_delay(uint32_t ms){
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void OBD9141_uart_init(void){
    // Setup UART buffered IO with event queue
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;

    uart_config_t uart_config = {
        .baud_rate = OBD9141_KLINE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));

    // Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
}

void OBD9141_uart_deinit(void){
    ESP_ERROR_CHECK(uart_driver_delete(obd9141.serial_port));
    ESP_ERROR_CHECK(gpio_reset_pin(TX_PIN));
}

bool OBD9141_uart_is_driver_installed(OBD_SERIAL_DATA_TYPE serial_port){
    return uart_is_driver_installed(serial_port);
}

int OBD9141_uart_write_bytes(OBD_SERIAL_DATA_TYPE serial_port, void *b, size_t len){
    return uart_write_bytes(serial_port, b, len);
}

int OBD9141_uart_read_bytes(OBD_SERIAL_DATA_TYPE serial_port, void *b, size_t len, size_t timeout_ms){
    return uart_read_bytes(serial_port, b, len, pdMS_TO_TICKS(timeout_ms));
}

void OBD9141_set_pin_mode(int pin, int mode){
    ESP_ERROR_CHECK(gpio_set_direction(pin, mode));
}

void OBD9141_set_pin_level(int pin, int level){
    ESP_ERROR_CHECK(gpio_set_level(pin, level));
}




// User functions (public methods of OBD9141 class in original library)

void OBD9141_begin(void){
    obd9141.serial_port = UART_NUM;
    memset(obd9141.buffer, 0, OBD9141_BUFFER_SIZE);
    OBD9141_set_pin_mode(RX_PIN, GPIO_MODE_INPUT);
    obd9141.use_kwp = false;
}

bool OBD9141_get_current_pid(uint8_t pid, uint8_t return_length){
    return OBD9141_get_pid(pid, 0x01, return_length);
}

/*
    No header description to be found on the internet?

    for 9141-2:
      raw request: {0x68, 0x6A, 0xF1, 0x01, 0x0D}
          returns:  0x48  0x6B  0x11  0x41  0x0D  0x00  0x12 
          returns 1 byte
          total of 7 bytes.

      raw request: {0x68, 0x6A, 0xF1, 0x01, 0x00}
          returns   0x48  0x6B  0x11  0x41  0x00  0xBE  0x3E  0xB8  0x11  0xCA
          returns 3 bytes
          total of 10 bytes

      Mode seems to be 0x40 + mode, unable to confirm this though.

    for ISO 14230 KWP:
      First byte lower 6 bits are length, first two bits always 0b11?

      raw request: {0xc2, 0x33, 0xf1, 0x01, 0x0d, 0xf4}
      returns       0x83  0xf1  0x11  0x41  0xd  0x0  0xd3

      raw request: {0xc2, 0x33, 0xf1, 0x01, 0x0c, 0xf3}
      returns       0x84, 0xf1, 0x11, 0x41, 0x0c, 0x0c, 0x4c, 0x2b, 0xf3
*/

bool OBD9141_get_pid(uint8_t pid, uint8_t mode, uint8_t return_length){
    uint8_t message[5] = {0x68, 0x6A, 0xF1, mode, pid};
    // header of request is 5 long, first three are always constant.

    bool res = OBD9141_request(&message, 5, return_length + 5);
    // checksum is already checked, verify the PID.

    if(obd9141.buffer[4] != pid){
        return false;
    }
    return res;
}

bool OBD9141_request(void *request, uint8_t request_len, uint8_t ret_len){
    if (obd9141.use_kwp){
        // have to modify the first bytes.
        uint8_t rbuf[request_len];
        memcpy(rbuf, request, request_len);
        // now we modify the header, the payload is the request_len - 3 header bytes
        rbuf[0] = (0b11 << 6) | (request_len - 3);
        rbuf[1] = 0x33;  // second byte should be 0x33
        return (OBD9141_request_kwp(&rbuf, request_len) == ret_len);
        }
        return OBD9141_request_9141(request, request_len, ret_len);
}

bool OBD9141_request_9141(void* request, uint8_t request_len, uint8_t ret_len){
    uint8_t buf[request_len + 1];
    memcpy(buf, request, request_len); // copy request

    buf[request_len] = OBD9141_checksum(&buf, request_len); // add the checksum

    OBD9141_write_arr(&buf, request_len + 1);

    // wait after the request, officially 30 ms, but we might as well wait
    // for the data in the readBytes function.
    
    int ret;
    memset(obd9141.buffer, 0, ret_len + 1);
    // set proper timeout
    size_t timeout_ms = OBD9141_REQUEST_ANSWER_MS_PER_BYTE * ret_len + OBD9141_WAIT_FOR_REQUEST_ANSWER_TIMEOUT;
    ret = OBD9141_uart_read_bytes(obd9141.serial_port, obd9141.buffer, ret_len + 1, timeout_ms);
    if (ret > 0){
#ifdef OBD9141_DEBUG
        printf("R: ");
        for (uint8_t i = 0; i < (ret_len + 1); i++){
            printf("0x%02X ", obd9141.buffer[i]);
        };
        printf("\n");
#endif
        return (OBD9141_checksum(&(obd9141.buffer[0]), ret_len) == obd9141.buffer[ret_len]); // have data; return whether it is valid.
    }
    else {
#ifdef OBD9141_DEBUG
        printf("Timeout on reading bytes.\n");
#endif
        return false; // failed getting data.
    } 
}

uint8_t OBD9141_request_var_ret_len(void* request, uint8_t request_len){
    if (obd9141.use_kwp)
    {
        // have to modify the first bytes.
        uint8_t rbuf[request_len];
        memcpy(rbuf, request, request_len);
        // now we modify the header, the payload is the request_len - 3 header bytes
        rbuf[0] = (0b11 << 6) | (request_len - 3);
        rbuf[1] = 0x33;  // second byte should be 0x33
        return OBD9141_request_kwp(rbuf, request_len);
    }
    bool success = true;
    // wipe the entire buffer to ensure we are in a clean slate.
    memset(obd9141.buffer, 0, OBD9141_BUFFER_SIZE);

    // create the request with checksum.
    uint8_t buf[request_len + 1];
    memcpy(buf, request, request_len); // copy request
    buf[request_len] = OBD9141_checksum(&buf, request_len); // add the checksum

    // manually write the bytes onto the serial port
    // this does NOT read the echoes.
#ifdef OBD9141_DEBUG
    printf("W: ");
    for (uint8_t i = 0; i < (request_len + 1); i++){
        printf("0x%02X ", buf[i]);
    }
    printf("\n");
#endif
    for (uint8_t i = 0; i < request_len + 1 ; i++){
        OBD9141_uart_write_bytes(obd9141.serial_port, &buf[i], 1); // writes 1 byte at a time
        OBD9141_delay(OBD9141_INTERSYMBOL_WAIT);
    }

    // next step, is to read the echo from the serial port.
    uint8_t tmp[request_len + 1]; // temporary variable to read into.
    size_t timeout_ms = OBD9141_REQUEST_ECHO_MS_PER_BYTE * 1 + OBD9141_WAIT_FOR_ECHO_TIMEOUT;
    OBD9141_uart_read_bytes(obd9141.serial_port, tmp, request_len + 1, timeout_ms);
#ifdef OBD9141_DEBUG
    printf("E: ");
#endif
    for (uint8_t i = 0; i < request_len + 1; i++)
    {
#ifdef OBD9141_DEBUG
        printf("0x%02X ", tmp[i]);
#endif
      // check if echo is what we wanted to send
      success &= (buf[i] == tmp[i]);
    }

    // so echo is dealt with now... next is listening to the reply, which is a variable number.

    uint8_t answer_length = 0;
    int ret;

    // set the timeout for the first read to include the wait for answer timeout
    timeout_ms = OBD9141_REQUEST_ANSWER_MS_PER_BYTE + OBD9141_WAIT_FOR_REQUEST_ANSWER_TIMEOUT;

    // while readBytes returns a byte, keep reading.
    while (answer_length < OBD9141_BUFFER_SIZE) {
        ret = OBD9141_uart_read_bytes(obd9141.serial_port, &obd9141.buffer[answer_length], 1, timeout_ms);
        if (ret <= 0) {break;}

        answer_length++;
        // subsequent reads: normal per-byte timeout
        timeout_ms = OBD9141_REQUEST_ANSWER_MS_PER_BYTE;
    }

#ifdef OBD9141_DEBUG
    printf("\nA (%d): ", answer_length);
    for (uint8_t i = 0; i < answer_length; i++){
       printf("0x%02X ", obd9141.buffer[i]);
    }
    printf("\n");
#endif

    // next, calculate the checksum
    bool checksum = (OBD9141_checksum(&(obd9141.buffer[0]), answer_length - 1) == obd9141.buffer[answer_length - 1]);
#ifdef OBD9141_DEBUG
    printf("C: %d\n", checksum);
    printf("S: %d\n", success);
    printf("R: %d\n", answer_length - 1);
#endif
    if (checksum && success)
    {
      return answer_length - 1;
    }
    return 0;
}

uint8_t OBD9141_request_kwp(void* request, uint8_t request_len){
    uint8_t buf[request_len + 1];
    memcpy(buf, request, request_len); // copy request

    buf[request_len] = OBD9141_checksum(&buf, request_len); // add the checksum

    OBD9141_write_arr(&buf, request_len + 1);

    // wait after the request, officially 30 ms, but we might as well wait
    // for the data in the readBytes function.

    
    // Example response: 131 241 17 193 239 143 196 0 
    int ret;
    memset(obd9141.buffer, 0, OBD9141_BUFFER_SIZE);
    // set proper timeout
    size_t timeout_ms = OBD9141_REQUEST_ANSWER_MS_PER_BYTE * 1 + OBD9141_WAIT_FOR_REQUEST_ANSWER_TIMEOUT;
    // Try to read the fmt byte.
    ret = OBD9141_uart_read_bytes(obd9141.serial_port, obd9141.buffer, 1, timeout_ms);
    if (ret <= 0)
    {
      return 0; // failed reading the response byte.
    }

    const uint8_t msg_len = (obd9141.buffer[0]) & 0b111111;
    // If length is zero, there is a length byte at the end of the header.
    // This is likely very rare, we do not handle this for now.

    // This means that we should now read the 2 addressing bytes, the payload
    // and the checksum byte.
    const uint8_t remainder = msg_len + 2 + 1;
#ifdef OBD9141_DEBUG
    printf("Rem: %d\n", remainder);
#endif
    const uint8_t ret_len = remainder + 1;
#ifdef OBD9141_DEBUG
    printf("ret_len: %d\n", ret_len);
#endif

    timeout_ms = OBD9141_REQUEST_ANSWER_MS_PER_BYTE * (remainder + 1);
    ret = OBD9141_uart_read_bytes(obd9141.serial_port, &(obd9141.buffer[1]), remainder, timeout_ms);
    if (ret > 0){
#ifdef OBD9141_DEBUG
        printf("R: ");
        for (uint8_t i = 0; i < (ret_len + 1); i++){
            printf("0x%02X ", obd9141.buffer[i]);
        };printf("\n");
#endif
  
        const uint8_t calc_checksum = OBD9141_checksum(&(obd9141.buffer[0]), ret_len - 1);
#ifdef OBD9141_DEBUG
    printf("calc cs: %d\n", calc_checksum);
    printf("buf cs: %d\n", obd9141.buffer[ret_len - 1]);
#endif
        if (calc_checksum == obd9141.buffer[ret_len - 1])
        {
          return ret_len - 1; // have data; return whether it is valid.
        }
    }
    else {
#ifdef OBD9141_DEBUG
        printf("Timeout on reading bytes.\n");
#endif
        return 0; // failed getting data.
    }
    return 0;
}

uint8_t OBD9141_read_uint8(void){
    return obd9141.buffer[5];
}

uint16_t OBD9141_read_uint16(void){
    return  obd9141.buffer[5] * 256 +  obd9141.buffer[6]; // need to reverse endianness    
}

uint32_t OBD9141_read_uint32(void){
    return ((uint32_t)obd9141.buffer[5] << 24) |
           ((uint32_t)obd9141.buffer[6] << 16) |
           ((uint32_t)obd9141.buffer[7] << 8)  |
            (uint32_t)obd9141.buffer[8]; // need to reverse endianness
}

uint8_t OBD9141_read_uint8_idx(uint8_t index){
    return obd9141.buffer[5 + index];
}

uint8_t OBD9141_read_buffer(uint8_t index){
    return obd9141.buffer[index];
}

uint16_t OBD9141_get_trouble_code(uint8_t index){
    return *(uint16_t*)&(obd9141.buffer[index * 2 + 4]);
}

void OBD9141_set_port(bool enabled){
    if(enabled){
        if(!OBD9141_uart_is_driver_installed(obd9141.serial_port)){
            OBD9141_uart_init();
        }
    }
    else{
        if(OBD9141_uart_is_driver_installed(obd9141.serial_port)){
            OBD9141_uart_deinit();
        }
        OBD9141_set_pin_mode(TX_PIN, GPIO_MODE_OUTPUT);
        OBD9141_set_pin_level(TX_PIN, HIGH);
    }
}

bool OBD9141_init(void){
    // Normal 9141-2 slow init, check v1 == v2.
    return OBD9141_init_impl(true);
}

bool  OBD9141_init_kwp(void){
    // this function performs the KWP2000 fast init.
    obd9141.use_kwp = true;
    OBD9141_set_port(false); // disable the port

    OBD9141_kline(HIGH); // set high
    OBD9141_delay(OBD9141_INIT_IDLE_BUS_BEFORE); // no traffic on bus for 3 seconds
#ifdef OBD9141_DEBUG
    printf("Before 25 ms / 25 ms startup.\n");
#endif
    OBD9141_kline(LOW);     // set low
    OBD9141_delay(25);      // start with 25 ms low
    OBD9141_kline(HIGH);    // set high
    TickType_t xLastWakeTime = xTaskGetTickCount();   
#ifdef OBD9141_DEBUG
    printf("Enable port\n");
#endif
    OBD9141_set_port(true);
    // If porting to another framework where FreeRTOS is not used, find a way to make the HIGH delay + UART init time be 25 ms total (check with scope/logic analyzer if you have to)
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(25)); // then 25 ms high - using vTaskDelayUntil because UART init takes ~6 ms and would otherwise prolong the delay to ~31 ms instead of 25 ms if we used regular vTaskDelay()

    // immediately follow this by a startCommunicationRequest
    // startCommunicationRequest message:
    uint8_t message[4] = {0xC1, 0x33, 0xF1, 0x81};
    // checksum (0x66) is calculated by request method.

    // Send this request and read the response
    if (OBD9141_request_kwp(&message, 4) == 6) {
        // check positive response service ID, should be 0xC1.
        if (obd9141.buffer[3] == 0xC1) {
            // Not necessary to do anything with this data?
            return true;
        }
        else {
            return false;
        }
    }
    return false;
}

bool OBD9141_init_kwp_slow(void){
    // KWP slow init, v1 == 233, v2 = 143, don't check v1 == v2.
    bool res = OBD9141_init_impl(false);
    // After the init, switch to kwp.
    obd9141.use_kwp = true;
    return res;
}

bool OBD9141_clear_trouble_codes(void){
    uint8_t message[4] = {0x68, 0x6A, 0xF1, 0x04};
    // 0x04 without PID value should clear the trouble codes or
    // malfunction indicator lamp.

    // No data is returned to this request, we expect the request itself
    // to be returned.
    bool res = OBD9141_request(&message, 4, 4);
    return res;
}

uint8_t OBD9141_read_trouble_codes(void){
    uint8_t message[4] = {0x68, 0x6A, 0xF1, 0x03};
    uint8_t response = OBD9141_request_var_ret_len(&message, 4);
    if (response >= 4){
#ifdef OBD9141_DEBUG
        printf("T: %d\n", (response - 4) / 2);
#endif
        return (response - 4) / 2;  // every DTC is 2 bytes, header was 4 bytes.
    }
    return 0;
}

uint8_t OBD9141_read_pending_trouble_codes(void){
    uint8_t message[4] = {0x68, 0x6A, 0xF1, 0x07};
    uint8_t response = OBD9141_request_var_ret_len(&message, 4);
    if (response >= 4){
#ifdef OBD9141_DEBUG
        printf("T: %d\n", (response - 4) / 2);
#endif
        return (response - 4) / 2;  // every DTC is 2 bytes, header was 4 bytes.
    }
    return 0;
}

uint8_t OBD9141_checksum(void* b, uint8_t len){
    uint8_t ret = 0;
    for (uint8_t i = 0; i < len; i++) {
        ret += ((uint8_t*)b)[i];
    }
    return ret;
}

void OBD9141_decode_dtc(uint16_t input_bytes, uint8_t* output_string){
    const uint8_t A = (input_bytes >> 8) & 0xFF;
    const uint8_t B = input_bytes & 0xFF;
    const static char type_lookup[4] = {'P', 'C', 'B', 'U'};
    const static char digit_lookup[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    // A7-A6 is first dtc character, error type:
    output_string[0] = type_lookup[A >> 6];
    // A5-A4 is second dtc character
    output_string[1] = digit_lookup[(A >> 4) & 0b11];
    // A3-A0 is third dtc character.
    output_string[2] = digit_lookup[A & 0b1111];
    // B7-B4 is fourth dtc character
    output_string[3] = digit_lookup[B >> 4];
    // B3-B0 is fifth dtc character
    output_string[4] = digit_lookup[B & 0b1111];
}