#pragma once

#include <Wire.h>

bool tca9548aSelectChannel(TwoWire* bus, uint8_t address, uint8_t channel);
