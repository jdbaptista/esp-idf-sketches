#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define HIGH 1
#define LOW 0
#define INPUT GPIO_MODE_INPUT
#define OUTPUT GPIO_MODE_OUTPUT

// JTAG pins are from the perspective of the MSP430
#define RST GPIO_NUM_16 // MSP430 reset                     (RX)  ->  (16) 
#define TMS GPIO_NUM_17 // JTAG state machine control       (TX)  ->  (7)
#define TCK GPIO_NUM_18 // JTAG clock input                 (MO)  ->  (6)
#define TDI GPIO_NUM_19 // JTAG data input and TCLK input   (MI)  ->  (14)
#define TDO GPIO_NUM_21 // JTAG data output                 (21)  ->  (15)
#define TEN GPIO_NUM_22 // JTAG enable                      (SCL) ->  (17)

/*
    Shifts an 8-bit JTAG instruction into the JTAG
    instruction register via the TDI. At the same time,
    the 8-bit JTAG ID is shifted out via the TDO. Each
    instruction bit is captured from TDI on the rising
    edge of the TCK. TCLK is maintained throughout this
    macro.

    Returns: 8-bit JTAG ID
*/
uint8_t IR_SHIFT(uint8_t input_data) {
    uint16_t ret = 0x00;
    int prevTDI = gpio_get_level(TDI);

    // set TAP machine to Shift-DR state
    // TMS set through falling and rising edge
    gpio_set_level(TMS, HIGH);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1

    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1

    gpio_set_level(TMS, LOW);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    // shift data into DR
    uint16_t bit;
    printf("Shifting IR: \nIn: ");
    for (int i = 7; i >= 0; i--) {
        bit = input_data >> i;
        bit &= 0x01; // send the selected bit
        gpio_set_level(TDI, bit);
        printf("%d", bit);
        gpio_set_level(TCK, LOW);
        gpio_set_level(TCK, HIGH);
        uint16_t output = gpio_get_level(TDO);
        ret |= output << i;
    }

    // Send LSB and return to Run/Idle
    bit = input_data;
    bit &= 0x01;
    gpio_set_level(TMS, HIGH);
    gpio_set_level(TDI, bit);
    printf("%d\n", bit);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1
    ret |= gpio_get_level(TDO);

    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1

    gpio_set_level(TMS, LOW);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    for (int i = 0; i < 4; i++) {
        gpio_set_level(TCK, LOW);
        gpio_set_level(TCK, HIGH);
    }

    gpio_set_level(TDI, prevTDI);
    printf("Out: 0x%X\n", ret);
    return ret;
}

/*
    Shifts a 16-bit word into the JTAG data register (DR).
    The word is shifted, MSB first, via the TDI. At the
    same time, the last captured and stored value in the
    addressed data register is shifted out via the TDO. A
    new bit is present at TDO with a falling edge of TCK.
    TCLK is maintained throughout this macro.

    Returns: Last captured and stored value in the
    addressed data register.
*/
uint16_t DR_SHIFT(uint16_t input_data) {
    uint16_t ret = 0x0000;
    int prevTDI = gpio_get_level(TDI);

    // set TAP machine to Shift-DR state
    // TMS set through falling and rising edge
    gpio_set_level(TMS, HIGH);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1

    gpio_set_level(TMS, LOW);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    // shift data into DR
    uint16_t bit;
    printf("Shifting DR: \nIn: ");
    for (int i = 15; i > 0; i--) {
        bit = input_data >> i;
        bit &= 0x0001; // send the selected bit
        gpio_set_level(TDI, bit);
        printf("%d", bit);
        gpio_set_level(TCK, LOW);
        gpio_set_level(TCK, HIGH);
        uint16_t output = gpio_get_level(TDO);
        ret |= output << i;
    }

    // Send LSB and return to Run/Idle
    bit = input_data;
    bit &= 0x0001;
    gpio_set_level(TMS, HIGH);
    gpio_set_level(TDI, bit);
    printf("%d\n", bit);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1
    ret |= gpio_get_level(TDO);

    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1

    gpio_set_level(TMS, LOW);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    gpio_set_level(TDI, prevTDI);
    printf("Out: 0x%X\n", ret);
    return ret;
}

/*
    Drives one JTAG command on the MSP430 via standard
    4-Wire JTAG signals. Specifically, it reads one byte
    of memory at a given location. Refer to documentation:
    https://www.ti.com/lit/ug/slau320aj/slau320aj.pdf
*/
void app_main(void)
{
    // configure pins
    gpio_reset_pin(RST);
    gpio_reset_pin(TMS);
    gpio_reset_pin(TCK);
    gpio_reset_pin(TDI);
    gpio_reset_pin(TDO);
    gpio_reset_pin(TEN);
    gpio_set_direction(RST, OUTPUT);
    gpio_set_direction(TMS, OUTPUT);
    gpio_set_direction(TCK, OUTPUT);
    gpio_set_direction(TDI, OUTPUT);
    gpio_set_direction(TDO, INPUT);
    gpio_set_direction(TEN, OUTPUT);
    gpio_set_pull_mode(TDO, GPIO_PULLDOWN_ONLY);

    // enable JTAG access: case 2a, Fig.2-13
    // RST held low for JTAG, high for SBW
    gpio_set_level(RST, HIGH);
    gpio_set_level(TEN, LOW);
        // there is a ~28 microsecond delay
    gpio_set_level(TEN, HIGH);
    gpio_set_level(RST, LOW);
    gpio_set_level(TEN, LOW);
    gpio_set_level(TEN, HIGH);
    gpio_set_level(RST, HIGH);

    // Move TAP FSM to Run/IDLE for fuse check
    gpio_set_level(TMS, HIGH);
    for (int i = 0; i < 6; i++) {
        gpio_set_level(TCK, LOW);
        gpio_set_level(TCK, HIGH);  // FSM: TLR
    }
    gpio_set_level(TMS, LOW);
    gpio_set_level(TDI, HIGH);  // FSM: IDLE
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH);


    // gpio_set_level(TCK, LOW);   ????
    // gpio_set_level(TCK, HIGH);

    // fuse check
    // gpio_set_level(TMS, HIGH);    WHY IS THIS
    // gpio_set_level(TMS, LOW);     UNNECESSARY?
    // gpio_set_level(TMS, HIGH);
    // gpio_set_level(TMS, LOW);
    // gpio_set_level(TMS, HIGH);
    // gpio_set_level(TMS, LOW);   ????

    // send data
    IR_SHIFT(0xFF);
    DR_SHIFT(0x3333);
    IR_SHIFT(0xFF);
    DR_SHIFT(0x3333);
    // // relinquish JTAG access
    // ReleaseDevice(); idk if this is necessary
    gpio_set_level(TEN, LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    // print results
    // printf("%X\n", mem);
}
