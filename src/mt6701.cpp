#include "mt6701.h"
#include <esp_timer.h>
#include <math.h>

static bool readRegister(const MT6701* encoder, uint8_t address, uint8_t* value) {
    TwoWire* bus = encoder->config.bus;
    bus->beginTransmission(encoder->config.address);
    bus->write(address);
    if (bus->endTransmission(false) != 0) {
        return false;
    }
    const uint8_t bytesToRead = 1;
    if (bus->requestFrom(encoder->config.address, bytesToRead) != bytesToRead) {
        return false;
    }
    *value = (uint8_t)bus->read();
    return true;
}

MT6701Sample mt6701CaptureSnapshot(const MT6701* encoder) {
    const uint8_t angleHighRegister = 0x03;
    const uint8_t angleLowRegister = 0x04;
    const unsigned highAngleShift = 6;
    const unsigned lowAngleShift = 2;
    const double angleStepsPerRevolution = 16384.0;
    const double degreesPerRevolution = 360.0;
    MT6701Sample newSample = {};
    if (encoder == NULL) {
        return newSample;
    }

    uint8_t highByte;
    uint8_t lowByte;
    if (!readRegister(encoder, angleHighRegister, &highByte) ||
        !readRegister(encoder, angleLowRegister, &lowByte)) {
        return newSample;
    }

    uint16_t rawAngle = ((uint16_t)highByte << highAngleShift) | (lowByte >> lowAngleShift);
    double sensorDegrees = rawAngle * degreesPerRevolution / angleStepsPerRevolution;
    double degrees = encoder->config.directionSign *
                     (sensorDegrees - encoder->config.zeroOffsetDegrees);
    degrees = fmod(degrees, degreesPerRevolution);
    if (degrees < 0) {
        degrees += degreesPerRevolution;
    }
    newSample.rawAngle = rawAngle;
    newSample.degrees = degrees;
    newSample.timestampUs = esp_timer_get_time();
    newSample.valid = true;
    return newSample;
}

double captureMt6701Angle(const MT6701* encoder) {
    if (encoder == NULL || encoder->config.bus == NULL) {
        return NAN;
    }
    MT6701Sample sample = mt6701CaptureSnapshot(encoder);
    return sample.valid ? sample.degrees : NAN;
}
