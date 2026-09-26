#pragma once
#include <stdint.h>
void battery_init(void);
// Latest averaged battery voltage in mV (0 until first reading) and coarse 0-5 level.
int battery_mv(void);
int battery_level(void);
