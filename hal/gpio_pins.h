//
// Copyright 2014 - 2019 Per Vices Corporation
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//                    
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef GPIO_PINS_H_
#define GPIO_PINS_H_

/* clang-format off */

#include "array-utils.h"

// https://en.wikipedia.org/wiki/X_Macro.

#if defined(TATE)
    #define GPIO_PINS \
        X(000, tx) \
        X(001, tx) \
        X(002, tx) \
        X(003, tx) \
        X(004, tx) \
        X(005, tx) \
        X(006, tx) \
        X(007, tx) \
        X(008, tx) \
        X(009, tx) \
        X(010, tx) \
        X(011, tx) \
        X(012, tx) \
        X(013, tx) \
        X(014, tx) \
        X(015, tx) \
        X(016, tx) \
        X(017, tx) \
        X(018, tx) \
        X(019, tx) \
        X(020, tx) \
        X(021, tx) \
        X(022, tx) \
        X(023, tx) \
        X(024, tx) \
        X(025, tx) \
        X(026, tx) \
        X(027, tx) \
        X(028, tx) \
        X(029, tx) \
        X(030, tx) \
        X(031, tx) \
        X(032, tx) \
        X(033, tx) \
        X(034, tx) \
        X(035, tx) \
        X(036, tx) \
        X(037, tx) \
        X(038, tx) \
        X(039, tx) \
        X(040, tx) \
        X(041, tx) \
        X(042, tx) \
        X(043, tx) \
        X(044, tx) \
        X(045, tx) \
        X(046, tx) \
        X(047, tx) \
        X(048, tx) \
        X(049, tx) \
        X(050, tx) \
        X(051, tx) \
        X(052, tx) \
        X(053, tx) \
        X(054, tx) \
        X(055, tx) \
        X(056, tx) \
        X(057, tx) \
        X(058, tx) \
        X(059, tx) \
        X(060, tx) \
        X(061, tx) \
        X(062, tx) \
        X(063, tx) \
        X(064, tx) \
        X(065, tx) \
        X(066, tx) \
        X(067, tx) \
        X(068, tx) \
        X(069, tx) \
        X(070, tx) \
        X(071, tx) \
        X(072, tx) \
        X(073, tx) \
        X(074, tx) \
        X(075, tx) \
        X(076, tx) \
        X(077, tx) \
        X(078, tx) \
        X(079, tx)
#elif defined(TATE_4R4T)
    #define GPIO_PINS \
        X(000, tx) \
        X(001, tx) \
        X(002, tx) \
        X(003, tx) \
        X(004, tx) \
        X(005, tx) \
        X(006, tx) \
        X(007, tx) \
        X(008, tx) \
        X(009, tx) \
        X(010, tx) \
        X(011, tx) \
        X(012, tx) \
        X(013, tx) \
        X(014, tx) \
        X(015, tx) \
        X(016, tx) \
        X(017, tx) \
        X(018, tx) \
        X(019, tx) \
        X(020, tx) \
        X(021, tx) \
        X(022, tx) \
        X(023, tx) \
        X(024, tx) \
        X(025, tx) \
        X(026, tx) \
        X(027, tx) \
        X(028, tx) \
        X(029, tx) \
        X(030, tx) \
        X(031, tx) \
        X(032, tx) \
        X(033, tx) \
        X(034, tx) \
        X(035, tx) \
        X(036, tx) \
        X(037, tx) \
        X(038, tx) \
        X(039, tx) \
        X(040, tx) \
        X(041, tx) \
        X(042, tx) \
        X(043, tx) \
        X(044, tx) \
        X(045, tx) \
        X(046, tx) \
        X(047, tx) \
        X(048, tx) \
        X(049, tx) \
        X(050, tx) \
        X(051, tx) \
        X(052, tx) \
        X(053, tx) \
        X(054, tx) \
        X(055, tx) \
        X(056, tx) \
        X(057, tx) \
        X(058, tx) \
        X(059, tx) \
        X(060, tx) \
        X(061, tx) \
        X(062, tx) \
        X(063, tx) \
        X(064, tx) \
        X(065, tx) \
        X(066, tx) \
        X(067, tx) \
        X(068, tx) \
        X(069, tx) \
        X(070, tx) \
        X(071, tx) \
        X(072, tx) \
        X(073, tx) \
        X(074, tx) \
        X(075, tx) \
        X(076, tx) \
        X(077, tx) \
        X(078, tx) \
        X(079, tx)
#elif defined(TATE_4R4T_3G)
    #define GPIO_PINS \
        X(000, tx) \
        X(001, tx) \
        X(002, tx) \
        X(003, tx) \
        X(004, tx) \
        X(005, tx) \
        X(006, tx) \
        X(007, tx) \
        X(008, tx) \
        X(009, tx) \
        X(010, tx) \
        X(011, tx) \
        X(012, tx) \
        X(013, tx) \
        X(014, tx) \
        X(015, tx) \
        X(016, tx) \
        X(017, tx) \
        X(018, tx) \
        X(019, tx) \
        X(020, tx) \
        X(021, tx) \
        X(022, tx) \
        X(023, tx) \
        X(024, tx) \
        X(025, tx) \
        X(026, tx) \
        X(027, tx) \
        X(028, tx) \
        X(029, tx) \
        X(030, tx) \
        X(031, tx) \
        X(032, tx) \
        X(033, tx) \
        X(034, tx) \
        X(035, tx) \
        X(036, tx) \
        X(037, tx) \
        X(038, tx) \
        X(039, tx) \
        X(040, tx) \
        X(041, tx) \
        X(042, tx) \
        X(043, tx) \
        X(044, tx) \
        X(045, tx) \
        X(046, tx) \
        X(047, tx) \
        X(048, tx) \
        X(049, tx) \
        X(050, tx) \
        X(051, tx) \
        X(052, tx) \
        X(053, tx) \
        X(054, tx) \
        X(055, tx) \
        X(056, tx) \
        X(057, tx) \
        X(058, tx) \
        X(059, tx) \
        X(060, tx) \
        X(061, tx) \
        X(062, tx) \
        X(063, tx) \
        X(064, tx) \
        X(065, tx) \
        X(066, tx) \
        X(067, tx) \
        X(068, tx) \
        X(069, tx) \
        X(070, tx) \
        X(071, tx) \
        X(072, tx) \
        X(073, tx) \
        X(074, tx) \
        X(075, tx) \
        X(076, tx) \
        X(077, tx) \
        X(078, tx) \
        X(079, tx)
#elif defined(TATE_8R)
    #define GPIO_PINS \
        X(000, tx) \
        X(001, tx) \
        X(002, tx) \
        X(003, tx) \
        X(004, tx) \
        X(005, tx) \
        X(006, tx) \
        X(007, tx) \
        X(008, tx) \
        X(009, tx) \
        X(010, tx) \
        X(011, tx) \
        X(012, tx) \
        X(013, tx) \
        X(014, tx) \
        X(015, tx) \
        X(016, tx) \
        X(017, tx) \
        X(018, tx) \
        X(019, tx) \
        X(020, tx) \
        X(021, tx) \
        X(022, tx) \
        X(023, tx) \
        X(024, tx) \
        X(025, tx) \
        X(026, tx) \
        X(027, tx) \
        X(028, tx) \
        X(029, tx) \
        X(030, tx) \
        X(031, tx) \
        X(032, tx) \
        X(033, tx) \
        X(034, tx) \
        X(035, tx) \
        X(036, tx) \
        X(037, tx) \
        X(038, tx) \
        X(039, tx) \
        X(040, tx) \
        X(041, tx) \
        X(042, tx) \
        X(043, tx) \
        X(044, tx) \
        X(045, tx) \
        X(046, tx) \
        X(047, tx) \
        X(048, tx) \
        X(049, tx) \
        X(050, tx) \
        X(051, tx) \
        X(052, tx) \
        X(053, tx) \
        X(054, tx) \
        X(055, tx) \
        X(056, tx) \
        X(057, tx) \
        X(058, tx) \
        X(059, tx) \
        X(060, tx) \
        X(061, tx) \
        X(062, tx) \
        X(063, tx) \
        X(064, tx) \
        X(065, tx) \
        X(066, tx) \
        X(067, tx) \
        X(068, tx) \
        X(069, tx) \
        X(070, tx) \
        X(071, tx) \
        X(072, tx) \
        X(073, tx) \
        X(074, tx) \
        X(075, tx) \
        X(076, tx) \
        X(077, tx) \
        X(078, tx) \
        X(079, tx)
#else
    #error "Project name (TATE | TATE_4R4T | TATE_4R4T_3G | TATE_8R) not specified or not recognized."
#endif

// Channel names as strings.
static const char* const pin_names[] = {
#define X(pin, io) "gpio_##pin",
    GPIO_PINS
#undef X
};

// Number of channels.
#define NUM_PINS ARRAY_SIZE(pin_names)

/* clang-format on */

#endif // GPIO_PINS_H_

