#pragma once

#include <Wire.h>

typedef struct {
    TwoWire* bus;
    uint8_t address;
    int8_t directionSign;
    double zeroOffsetDegrees;
} MT6701Config;

typedef struct {
    uint16_t rawAngle;
    double degrees;
    int64_t timestampUs;
    bool valid;
} MT6701Sample;

typedef struct {
    MT6701Config config;
} MT6701;

MT6701Sample mt6701CaptureSnapshot(const MT6701* encoder);
