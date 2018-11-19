#include "led.h"

void server_init_led(void)
{
    write_hps_reg("led1", 0x1); // Solid green
    write_hps_reg("led0", 0x00070003); // Flashing green
}

void server_ready_led(void)
{
    write_hps_reg("led1", 0x1);
    write_hps_reg("led0", 0x1);
}
