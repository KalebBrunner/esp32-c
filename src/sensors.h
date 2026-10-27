#pragma once

#include "amt103.h"
#include "mt6701.h"

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
void updateSensors(Sensors* sensors);
AngleReading getSensorReading(const Sensors* sensors, enum SensorId id);
