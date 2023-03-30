#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define HIGH 1
#define LOW 0
#define INPUT GPIO_MODE_INPUT
#define OUTPUT GPIO_MODE_OUTPUT

// JTAG pins are from the perspective of the MSP430
#define RST GPIO_NUM_16 // MSP430 reset
#define TMS GPIO_NUM_17 // JTAG state machine control
#define TCK GPIO_NUM_18 // JTAG clock input
#define TDI GPIO_NUM_19 // JTAG data input and TCLK input
#define TDO GPIO_NUM_21 // JTAG data output
#define TEN GPIO_NUM_22 // JTAG enable

#define LOCATION 0x00
#define CLK_DELAY 0.01 // in milliseconds

const uint8_t IR_ADDR_16BIT = 0x83;
const uint8_t IR_ADDR_CAPTURE = 0x84;
const uint8_t IR_DATA_TO_ADDR = 0x85;
const uint8_t IR_DATA_16BIT = 0x41;
const uint8_t IR_DATA_QUICK = 0x43;
const uint8_t IR_BYPASS = 0xFF;
const uint8_t IR_CNTRL_SIG_16BIT = 0x13;
const uint8_t IR_CNTRL_SIG_CAPTURE = 0x14;
const uint8_t IR_CNTRL_SIG_RELEASE = 0x15;
const uint8_t IR_DATA_PSA = 0x44;
const uint8_t IR_SHIFT_OUT_PSA = 0x46;
const uint8_t IR_Prepare_Blow = 0x22;
const uint8_t IR_Ex_Blow = 0x24;
const uint8_t IR_JMB_EXCHANGE = 0x61;

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
    uint8_t ret = 0;
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
    uint8_t bit;
    for (int i = 7; i > 0; i--) {
        bit = input_data >> i;
        bit &= 0x01; // send the selected bit
        gpio_set_level(TCK, LOW);
        gpio_set_level(TDI, bit);
        gpio_set_level(TCK, HIGH);
        ret |= gpio_get_level(TDO) << i;
    }

    // Send LSB and return to Run/Idle
    bit = input_data;
    bit &= 0x01;
    gpio_set_level(TMS, HIGH);
    gpio_set_level(TDI, LOW);
    gpio_set_level(TDI, bit);
    gpio_set_level(TCK, HIGH); // 1
    ret |= gpio_get_level(TDO);

    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1

    gpio_set_level(TMS, LOW);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    gpio_set_level(TDI, prevTDI);
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
    printf("%X = 0b", input_data);
    for (int i = 15; i > 0; i--) {
        bit = input_data >> i;
        bit &= 0x0001; // send the selected bit
        printf("%d", bit);
        gpio_set_level(TCK, LOW);
        gpio_set_level(TDI, bit);
        gpio_set_level(TCK, HIGH);
        ret |= gpio_get_level(TDO) << i;
    }
    printf("\n");

    // Send LSB and return to Run/Idle
    bit = input_data;
    bit &= 0x0001;
    gpio_set_level(TMS, HIGH);
    gpio_set_level(TDI, LOW);
    gpio_set_level(TDI, bit);
    gpio_set_level(TCK, HIGH); // 1
    ret |= gpio_get_level(TDO);

    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 1

    gpio_set_level(TMS, LOW);
    gpio_set_level(TCK, LOW);
    gpio_set_level(TCK, HIGH); // 0

    gpio_set_level(TDI, prevTDI);
    printf("%X\n", ret);
    return ret;
}

/*
    Sets TCLK to LOW, which acts as the falling edge of
    the CPU clock. Executed in the Run/Idle state. Note
    that the MSP430 is not pipelined, so a full TCLK cycle
    executes the CPU instruction located at the PC.
*/
void ClrTCLK() {
    gpio_set_level(TDI, LOW);
}

/*
    Sets TCLK to HIGH, which acts as the rising edge of
    the CPU clock. Executed in the Run/Idle state. Note
    that the MSP430 is not pipelined, so a full TCLK cycle
    executes the CPU instruction located at the PC.
*/
void SetTCLK() {
    gpio_set_level(TDI, HIGH);
}

/*
    Takes the CPU under JTAG Control.
*/
void GetDevice() {
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    DR_SHIFT((uint16_t) 0x2401);
    IR_SHIFT(IR_CNTRL_SIG_CAPTURE);
    while (true) {
        uint16_t TDOword = DR_SHIFT((uint16_t) 0x0000);
        if ((TDOword & 0x0100) != 0) return;
    }   
}

/*
    Releases CPU from JTAG control. The target CPU
    starts program execution with the address stored
    at location 0x0FFFE (reset vector).
    
    This function is very distinct from ReleaseCPU!
*/
void ReleaseDevice() {
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    DR_SHIFT((uint16_t) 0x2c01);
    DR_SHIFT((uint16_t) 0x2401);
    IR_SHIFT(IR_CNTRL_SIG_RELEASE);
}

/*
    Sets the CPU to instruction-fetch state. This is used
    to execute an instruction presented by a host over the
    JTAG port.
*/
void SetInstrFetch() {
    IR_SHIFT(IR_CNTRL_SIG_CAPTURE);
    uint16_t data = DR_SHIFT((uint16_t) 0x0000);
    if ((data & 0x0040) == 0) {
        ClrTCLK();
        SetTCLK();
    }
}

/*
    Loads the target device CPU's program counter 
    with the desired 16-bit address.
*/
void SetPC(uint16_t addr) {
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    DR_SHIFT((uint16_t) 0x3401);
    IR_SHIFT(IR_DATA_16BIT);
    DR_SHIFT((uint16_t) 0x4030);
    ClrTCLK();
    SetTCLK();
    DR_SHIFT(addr);
    ClrTCLK();
    SetTCLK();
    IR_SHIFT(IR_ADDR_CAPTURE);
    ClrTCLK();
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    DR_SHIFT((uint16_t) 0x2401);
}

/*
    Force a power-up reset of CPU
*/
void ExecutePOR() {
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    DR_SHIFT((uint16_t) 0x2c01);
    DR_SHIFT((uint16_t) 0x2401);
    ClrTCLK();
    SetTCLK();
    ClrTCLK();
    SetTCLK();
    ClrTCLK();
    IR_SHIFT(IR_ADDR_CAPTURE);
    SetTCLK();
}

/*
    Stopping of the CPU via the HALT_JTAG bit of the JTAG
    control signal register, which is set to 1 here.
*/
void HaltCPU() {
    IR_SHIFT(IR_DATA_16BIT);
    DR_SHIFT((uint16_t) 0x3FFF);
    ClrTCLK();
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    DR_SHIFT((uint16_t) 0x2409);
    SetTCLK();
}

/*
    Starting of the CPU via the HALT_JTAG bit of the JTAG
    control signal register, which is set to 0 here.
*/
void ReleaseCPU() {
    ClrTCLK();
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    DR_SHIFT((uint16_t) 0x2401);
    IR_SHIFT(IR_ADDR_CAPTURE);
    SetTCLK();
}

/*
    Reads one word (2 bytes) of memory at addr.
*/
uint16_t ReadMem(uint16_t addr) {
    SetInstrFetch();
    HaltCPU();
    ClrTCLK();
    IR_SHIFT(IR_CNTRL_SIG_16BIT);
    // read one word from mem
    uint16_t data = DR_SHIFT((uint16_t) 0x2409);
    IR_SHIFT(IR_ADDR_16BIT);
    DR_SHIFT(addr);
    IR_SHIFT(IR_DATA_TO_ADDR);
    SetTCLK();
    ClrTCLK();
    DR_SHIFT((uint16_t) 0x0000);
    ReleaseCPU();
    return data;
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

    // enable JTAG access: case 2b, Fig.2-13
    // RST held low for JTAG, high for SBW
    gpio_set_level(RST, LOW);
    gpio_set_level(TEN, HIGH);
        // I tested it, there is a roughly 28
        // microsecond delay between cycles here.
    gpio_set_level(TEN, LOW);
    gpio_set_level(TEN, HIGH);

    DR_SHIFT(0x0000);
    DR_SHIFT(0x1523);
    DR_SHIFT(0x5176);

    // GetDevice();

    // // This section of the code is executed only
    // // at the run state of the TAP machine,
    // // other states are accessed in the low-level
    // // functions: IR_SHIFT16, DR_SHIFT16.
    // // Get data at location in memory
    // uint16_t mem = ReadMem((uint16_t) 0xFF00);

    // // relinquish JTAG access
    // ReleaseDevice(); // idk if this is necessary
    gpio_set_level(TEN, LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    // print results
    // printf("%X\n", mem);
}
