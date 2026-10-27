#pragma once

#include <Arduino.h>

typedef struct {
    uint8_t pinA;
    uint8_t pinB;
    int32_t pulsesPerRevolution;
    int8_t directionSign;
} AMT103Config;

typedef struct {
    int64_t count;
    double degrees;
    int64_t timestampUs;
    uint32_t invalidTransitions;
    bool valid;
} AMT103Sample;

typedef struct {
    AMT103Config config;
    portMUX_TYPE lock;
    volatile int64_t count;
    volatile uint32_t invalidTransitions;
    volatile uint8_t previousAB;
} AMT103;

void amt103Start(AMT103* encoder);
AMT103Sample amt103CaptureSnapshot(AMT103* encoder);
double captureAmt103Angle(AMT103* encoder);
