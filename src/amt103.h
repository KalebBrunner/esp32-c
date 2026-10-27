#pragma once

#include <stdint.h>

enum { AMT103_CAPACITY = 2 };

typedef struct {
    const char* description;
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
} AMT103State;

typedef struct AMT103Internal* AMT103Handle;

bool createAmtEncoder(AMT103Handle* handle, const AMT103Config* config);
void updateEncoder(AMT103Handle encoder);
AMT103State getEncoderState(AMT103Handle encoder);
