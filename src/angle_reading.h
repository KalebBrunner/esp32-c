#pragma once

#include <stdint.h>

typedef enum {
    ANGLE_STARTUP_RELATIVE,
    ANGLE_SENSOR_ABSOLUTE
} AngleReference;

typedef struct {
    double degrees;
    int64_t timestampUs;
    bool valid;
    AngleReference reference;
} AngleReading;
