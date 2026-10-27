#include "sensors.h"
#include "board_pins.h"

void startSensors(Sensors* sensors) {
    sensors->left.config = {
        .pinA = Pin_LeftEncoder_ChannelA,
        .pinB = Pin_LeftEncoder_ChannelB,
        .pulsesPerRevolution = 2048,
        .directionSign = 1
    };
    amt103Start(&sensors->left);

    sensors->right.config = {
        .pinA = Pin_RightEncoder_ChannelA,
        .pinB = Pin_RightEncoder_ChannelB,
        .pulsesPerRevolution = 2048,
        .directionSign = 1
    };
    amt103Start(&sensors->right);

    const uint32_t busFrequencyHz = 100000;
    const uint16_t busTimeoutMs = 5;
    Wire.begin(Pin_Magnetic_SDA, Pin_Magnetic_SCL, busFrequencyHz);
    Wire.setTimeOut(busTimeoutMs);
    sensors->magnetic.config = {
        .bus = &Wire,
        .address = 0x06,
        .directionSign = 1,
        .zeroOffsetDegrees = 0.0,
        .sampleIntervalUs = 10000
    };
    startMt6701(&sensors->magnetic);
}

void updateSensors(Sensors* sensors) {
    amt103CaptureSnapshot(&sensors->left);
    amt103CaptureSnapshot(&sensors->right);
    updateMt6701(&sensors->magnetic);
}

AngleReading getSensorReading(const Sensors* sensors, enum SensorId id) {
    AMT103Sample sample = {};
    switch (id) {
        case Sensor_LeftAngle:
            sample = sensors->left.latestSample;
            break;
        case Sensor_RightAngle:
            sample = sensors->right.latestSample;
            break;
        case Sensor_MagneticAngle:
            return getMt6701State(&sensors->magnetic).angle;
        default: {
            AngleReading unavailable = {};
            return unavailable;
        }
    }
    AngleReading reading = {
        .degrees = sample.degrees,
        .timestampUs = sample.timestampUs,
        .valid = sample.valid,
        .reference = ANGLE_STARTUP_RELATIVE
    };
    return reading;
}
