#include "tca9548a.h"

bool tca9548aSelectChannel(TwoWire* bus, uint8_t address, uint8_t channel) {
    const uint8_t channelCount = 8;
    if (bus == NULL || channel >= channelCount) {
        return false;
    }
    bus->beginTransmission(address);
    bus->write((uint8_t)(1U << channel));
    return bus->endTransmission() == 0;
}
