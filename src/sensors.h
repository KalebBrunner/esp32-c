#pragma once

#include "amt103.h"
#include "mt6701.h"
#include "angle_reading.h"

enum SensorId {
    Sensor_LeftAngle,
    Sensor_RightAngle,
    Sensor_MagneticAngle
};

typedef struct {
    AMT103 left;
    AMT103 right;
    MT6701 magnetic;
} Sensors;

void startSensors(Sensors* sensors);
AngleReading getSensorReading(Sensors* sensors, enum SensorId id);
