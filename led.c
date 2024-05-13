#include "led.h"

// led control codes can be found in led.h

uint8_t get_led_state(void) {
    uint32_t read_val;
    read_hps_reg("led0",&read_val);
    switch(read_val)
    {
        case 0x00070003:
            return LED_STATE_INIT;
        case 0x1:
            return LED_STATE_READY;
        case 0x8000a:
            return LED_STATE_ERROR;
        default:
            PRINT(ERROR,"LED_STATE_UNKNOWN\n");
            return LED_STATE_UNKNOWN;
    }
}

void set_led_state(uint8_t state) {
    switch(state)
    {
        case LED_STATE_INIT:
            write_hps_reg("led1", 0x1);        // Solid green
            write_hps_reg("led0", 0x00070003); // Flashing green
            break;
        case LED_STATE_READY:
            write_hps_reg("led1", 0x1);
            write_hps_reg("led0", 0x1);
            break;
        case LED_STATE_ERROR:
            write_hps_reg("led1", 0x1);
            write_hps_reg("led0", 0x8000a);
            break;
        default:
            PRINT(ERROR,"invalid arg for set_led_state()\n");
    }
}
