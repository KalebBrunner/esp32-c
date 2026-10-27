#include "mt6701.h"
#include <esp_timer.h>
#include <math.h>

struct MT6701Internal {
    bool allocated;
    MT6701Config config;
    MT6701State snapshot;
    int64_t lastAttemptUs;
};

static struct MT6701Internal magneticEncoder = {};

static bool readRegister(MT6701Handle encoder, uint8_t address, uint8_t* value) {
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

bool createMt6701Encoder(MT6701Handle* handle, const MT6701Config* config) {
    if (handle == NULL || *handle != NULL || config == NULL || config->bus == NULL ||
        magneticEncoder.allocated || (config->address != 0x06 && config->address != 0x46) ||
        (config->directionSign != 1 && config->directionSign != -1) ||
        !isfinite(config->zeroOffsetDegrees) || config->sampleIntervalUs <= 0) {
        return false;
    }
    magneticEncoder.config = *config;
    magneticEncoder.snapshot.angle.reference = ANGLE_SENSOR_ABSOLUTE;
    magneticEncoder.allocated = true;
    magneticEncoder.lastAttemptUs = esp_timer_get_time() - config->sampleIntervalUs;
    *handle = &magneticEncoder;
    return true;
}

bool updateMt6701Encoder(MT6701Handle encoder) {
    const uint8_t angleHighRegister = 0x03;
    const uint8_t angleLowRegister = 0x04;
    const unsigned highAngleShift = 6;
    const unsigned lowAngleShift = 2;
    const double angleStepsPerRevolution = 16384.0;
    const double degreesPerRevolution = 360.0;
    if (encoder == NULL) {
        return false;
    }
    int64_t nowUs = esp_timer_get_time();
    if (nowUs - encoder->lastAttemptUs < encoder->config.sampleIntervalUs) {
        return encoder->snapshot.angle.valid;
    }
    encoder->lastAttemptUs = nowUs;

    uint8_t highByte;
    uint8_t lowByte;
    if (!readRegister(encoder, angleHighRegister, &highByte) ||
        !readRegister(encoder, angleLowRegister, &lowByte)) {
        encoder->snapshot.angle.valid = false;
        ++encoder->snapshot.failedReads;
        return false;
    }

    uint16_t rawAngle = ((uint16_t)highByte << highAngleShift) | (lowByte >> lowAngleShift);
    double sensorDegrees = rawAngle * degreesPerRevolution / angleStepsPerRevolution;
    double degrees = encoder->config.directionSign *
                     (sensorDegrees - encoder->config.zeroOffsetDegrees);
    degrees = fmod(degrees, degreesPerRevolution);
    if (degrees < 0) {
        degrees += degreesPerRevolution;
    }
    encoder->snapshot.rawAngle = rawAngle;
    encoder->snapshot.angle.degrees = degrees;
    encoder->snapshot.angle.timestampUs = esp_timer_get_time();
    encoder->snapshot.angle.valid = true;
    return true;
}

MT6701State getMt6701State(MT6701Handle encoder) {
    MT6701State unavailable = {};
    return encoder != NULL ? encoder->snapshot : unavailable;
}
