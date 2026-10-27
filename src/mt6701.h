#pragma once

#include <Wire.h>
#include "angle_reading.h"

typedef struct {
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

typedef struct {
    MT6701Config config;
    MT6701State snapshot;
    int64_t lastAttemptUs;
} MT6701;

void startMt6701(MT6701* encoder);
bool updateMt6701(MT6701* encoder);
MT6701State getMt6701State(const MT6701* encoder);
