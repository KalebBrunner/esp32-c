#pragma once

#include <Wire.h>
#include "angle_reading.h"

typedef struct {
    const char* description;
    TwoWire* bus;
    uint8_t address;
    int8_t directionSign;
    double zeroOffsetDegrees;
    int64_t sampleIntervalUs;
} MT6701Config;

typedef struct {
    uint16_t rawAngle;
    AngleReading angle;
    uint32_t failedReads;
} MT6701State;

typedef struct MT6701Internal* MT6701Handle;

bool createMt6701Encoder(MT6701Handle* handle, const MT6701Config* config);
bool updateMt6701Encoder(MT6701Handle encoder);
MT6701State getMt6701State(MT6701Handle encoder);
