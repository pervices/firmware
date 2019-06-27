#!/usr/bin/tclsh

source /home/root/gpio_control


set ::SCL_GPIO_BIT_OFFSET 7
set ::SDA_GPIO_BIT_OFFSET 6

set ::scl_gpio_handle ""
set ::sda_gpio_handle ""

proc initialize_i2c_gpio { } {
    set max_gpio_chips 1 
    set max_gpio_pins 24
    set_gpio_variables $max_gpio_chips $max_gpio_pins

    set gpio_chip_label "ffc03300.gpio"
    set gpio_chip_number 0
    set ::scl_gpio_handle [get_gpio_file $gpio_chip_label $gpio_chip_number $::SCL_GPIO_BIT_OFFSET]
    set ::sda_gpio_handle [get_gpio_file $gpio_chip_label $gpio_chip_number $::SDA_GPIO_BIT_OFFSET]
}


proc write_gpio_pin_value { gpio_handle value } {
    gpio_out $gpio_handle $value
}

proc read_gpio_pin_value { gpio_handle } {
    return [gpio_sense $gpio_handle]
}

# XXX NOTE: From gpio_control library:
#    Acceptable values for "dir" are:
#          1) "in":   input
#          2) "out":  output and defaults the pin to GND.
#          3) "high": output and defaults the pin to VCC.
#          3) "low":  output and defaults the pin to GND.
proc set_scl_dir { dir } {
    set_gpio_direction $::scl_gpio_handle $dir
}

# XXX NOTE: From gpio_control library:
#    Acceptable values for "dir" are:
#          1) "in":   input
#          2) "out":  output and defaults the pin to GND.
#          3) "high": output and defaults the pin to VCC.
#          3) "low":  output and defaults the pin to GND.
proc set_sda_dir { dir } {
    set_gpio_direction $::sda_gpio_handle $dir
}

# read/write functions for I2C pins
proc scl_high { } {
    write_gpio_pin_value $::scl_gpio_handle 1
    after 1
}
proc scl_low { } {
    write_gpio_pin_value $::scl_gpio_handle 0
    after 1
}
proc sda_high { } {
    write_gpio_pin_value $::sda_gpio_handle 1
    after 1
}
proc sda_low { } {
    write_gpio_pin_value $::sda_gpio_handle 0
    after 1
}

proc read_scl { } {
    return [read_gpio_pin_value $::scl_gpio_handle]
}

proc read_sda { } {
    return [read_gpio_pin_value $::sda_gpio_handle]
}

