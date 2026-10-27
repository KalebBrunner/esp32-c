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
        .zeroOffsetDegrees = 0.0
    };
}

AngleReading getSensorReading(Sensors* sensors, enum SensorId id) {
    AMT103Sample sample = {};
    switch (id) {
        case Sensor_LeftAngle:
            sample = amt103CaptureSnapshot(&sensors->left);
            break;
        case Sensor_RightAngle:
            sample = amt103CaptureSnapshot(&sensors->right);
            break;
        case Sensor_MagneticAngle: {
            MT6701Sample magneticSample = mt6701CaptureSnapshot(&sensors->magnetic);
            AngleReading reading = {
                .degrees = magneticSample.degrees,
                .timestampUs = magneticSample.timestampUs,
                .valid = magneticSample.valid,
                .reference = ANGLE_SENSOR_ABSOLUTE
            };
            return reading;
        }
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
