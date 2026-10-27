#include <Arduino.h>
#include <esp_timer.h>
#include "amt103.h"
#include "mt6701.h"

// ************************************************
// ************* Globals  *************************
// ************************************************

// ESP32-S3 GPIO numbers, not physical header positions.
// Avoid GPIO0/3/45/46 for general wiring: boot strapping pins.
// Keep GPIO19/20 free for our native USB connection.
// Avoid GPIO26-32 (flash/PSRAM); GPIO33-37 may also be reserved on octal-memory modules.
// GPIO43/44 are UART0; GPIO39-42 are external JTAG pins. Reserve them if using those interfaces.
// Check the exact board schematic for LEDs and other onboard connections before adding pins.
enum Pin {
    Pin_BootStrap0 = 0,
    Pin_Unused1 = 1,
    Pin_Unused2 = 2,
    Pin_BootStrap3 = 3,
    Pin_LeftEncoder_ChannelB = 4,
    Pin_LeftEncoder_ChannelA = 5,
    Pin_RightEncoder_ChannelB = 6,
    Pin_RightEncoder_ChannelA = 7,
    Pin_Unused8 = 8,
    Pin_Unused9 = 9,
    Pin_Unused10 = 10,
    Pin_Unused11 = 11,
    Pin_Unused12 = 12,
    Pin_Unused13 = 13,
    Pin_Unused14 = 14,
    Pin_Magnetic_SDA = 15,
    Pin_Magnetic_SCL = 16,
    Pin_Unused17 = 17,
    Pin_Unused18 = 18,
    Pin_USB_DMinus = 19,
    Pin_USB_DPlus = 20,
    Pin_Unused21 = 21,
    // GPIO22-25 do not exist on ESP32-S3.
    Pin_Memory26 = 26,
    Pin_Memory27 = 27,
    Pin_Memory28 = 28,
    Pin_Memory29 = 29,
    Pin_Memory30 = 30,
    Pin_Memory31 = 31,
    Pin_Memory32 = 32,
    Pin_ModuleDependent33 = 33,
    Pin_ModuleDependent34 = 34,
    Pin_ModuleDependent35 = 35,
    Pin_ModuleDependent36 = 36,
    Pin_ModuleDependent37 = 37,
    Pin_BoardDependent38 = 38,
    Pin_JTAG_MTCK = 39,
    Pin_JTAG_MTDO = 40,
    Pin_JTAG_MTDI = 41,
    Pin_JTAG_MTMS = 42,
    Pin_UART0_TX = 43,
    Pin_UART0_RX = 44,
    Pin_BootStrap45 = 45,
    Pin_BootStrap46 = 46,
    Pin_Unused47 = 47,
    Pin_BoardDependent48 = 48
};

// GPIO33/34 may not be exposed; GPIO35-37 depend on module memory wiring.
// GPIO38/48 may drive the onboard RGB LED, depending on board revision.
// Unused means unassigned by this firmware, not guaranteed free on every board.

// ************************************************
// ************* Sensor Structs  ******************
// ************************************************

enum SensorId {
    Sensor_LeftAngle,
    Sensor_RightAngle,
    Sensor_MagneticAngle,
    Sensor_Count
};

enum SensorType {
    SensorType_Unconfigured,
    SensorType_AMT103,
    SensorType_MT6701
};

typedef struct {
    const char* name;
    enum SensorType type;
    size_t driverIndex;
} Sensor;

enum {
    LEFT_ENCODER = 0,
    RIGHT_ENCODER = 1,
    MAGNETIC_ENCODER = 0,
    MAGNETIC_SENSOR_CAPACITY = 1
};

typedef struct {
    Sensor items[Sensor_Count];
    AMT103Handle amtEncoders[AMT103_CAPACITY];
    size_t amtCount;
    MT6701Handle magneticEncoders[MAGNETIC_SENSOR_CAPACITY];
    size_t magneticCount;
} Sensors;

AngleReading getSensorReading(const Sensors* sensors, enum SensorId id) {
    AngleReading unavailable = {};
    if (sensors == NULL || id < 0 || id >= Sensor_Count) {
        return unavailable;
    }
    const Sensor* sensor = &sensors->items[id];
    switch (sensor->type) {
        case SensorType_AMT103: {
            if (sensor->driverIndex >= sensors->amtCount) {
                return unavailable;
            }
            AMT103State state = getEncoderState(sensors->amtEncoders[sensor->driverIndex]);
            AngleReading reading = {
                .degrees = state.degrees,
                .timestampUs = state.timestampUs,
                .valid = state.valid,
                .reference = ANGLE_STARTUP_RELATIVE
            };
            return reading;
        }
        case SensorType_MT6701:
            if (sensor->driverIndex >= sensors->magneticCount) {
                return unavailable;
            }
            return getMt6701State(sensors->magneticEncoders[sensor->driverIndex]).angle;
        default:
            return unavailable;
    }
}

void updateAmtSensors(Sensors* sensors) {
    for (size_t i = 0; i < sensors->amtCount; ++i) {
        updateEncoder(sensors->amtEncoders[i]);
    }
}

void updateMagneticSensors(Sensors* sensors) {
    for (size_t i = 0; i < sensors->magneticCount; ++i) {
        updateMt6701Encoder(sensors->magneticEncoders[i]);
    }
}

void updateSensors(Sensors* sensors) {
    updateAmtSensors(sensors);
    updateMagneticSensors(sensors);
}

// ************************************************
// ************* App Structs  **********************
// ************************************************


typedef struct {
    Sensors sensors;
    int64_t screenRefreshUs;
    uint32_t rowsPerHeader;
    uint32_t idleDelayMs;
    int64_t startUs;
    int64_t nextScreenRefreshUs;
    uint32_t displayedRows;
    bool ready;
} Application;

// ************************************************
// ************* Persistent State *****************
// ************************************************

Application applicationState = {};

// ************************************************
// ********************** Setup *******************
// ************************************************

void setup() {
    Serial.begin(115200);
    delay(1000);

    Application* app = &applicationState;
    app->screenRefreshUs = 100000;
    app->rowsPerHeader = 30;
    app->idleDelayMs = 1;
    app->startUs = esp_timer_get_time();
    app->nextScreenRefreshUs = app->startUs + app->screenRefreshUs;

    const AMT103Config leftConfig = {
        .description = "Left",
        .pinA = Pin_LeftEncoder_ChannelA,
        .pinB = Pin_LeftEncoder_ChannelB,
        .pulsesPerRevolution = 2048,
        .directionSign = 1
    };
    if (!createAmtEncoder(&app->sensors.amtEncoders[LEFT_ENCODER], &leftConfig)) {
        Serial.println("Could not create Left encoder.");
        return;
    }
    ++app->sensors.amtCount;
    app->sensors.items[Sensor_LeftAngle] = {
        .name = "Left angle",
        .type = SensorType_AMT103,
        .driverIndex = LEFT_ENCODER
    };

    const AMT103Config rightConfig = {
        .description = "Right",
        .pinA = Pin_RightEncoder_ChannelA,
        .pinB = Pin_RightEncoder_ChannelB,
        .pulsesPerRevolution = 2048,
        .directionSign = 1
    };
    if (!createAmtEncoder(&app->sensors.amtEncoders[RIGHT_ENCODER], &rightConfig)) {
        Serial.println("Could not create Right encoder.");
        return;
    }
    ++app->sensors.amtCount;
    app->sensors.items[Sensor_RightAngle] = {
        .name = "Right angle",
        .type = SensorType_AMT103,
        .driverIndex = RIGHT_ENCODER
    };

    const uint32_t magneticBusFrequencyHz = 100000;
    const uint16_t magneticBusTimeoutMs = 5;
    if (!Wire.begin(Pin_Magnetic_SDA, Pin_Magnetic_SCL, magneticBusFrequencyHz)) {
        Serial.println("Could not start magnetic encoder I2C bus.");
        return;
    }
    Wire.setTimeOut(magneticBusTimeoutMs);

    const MT6701Config magneticConfig = {
        .description = "Magnetic",
        .bus = &Wire,
        .address = 0x06,
        .directionSign = 1,
        .zeroOffsetDegrees = 0.0,
        .sampleIntervalUs = 10000
    };
    if (!createMt6701Encoder(&app->sensors.magneticEncoders[MAGNETIC_ENCODER], &magneticConfig)) {
        Serial.println("Could not create magnetic encoder.");
        return;
    }
    ++app->sensors.magneticCount;
    app->sensors.items[Sensor_MagneticAngle] = {
        .name = "Magnetic angle",
        .type = SensorType_MT6701,
        .driverIndex = MAGNETIC_ENCODER
    };
    app->ready = true;
}

// ************************************************
// ***************** Display Instructions **********
// ************************************************

void formatAngle(char* text, size_t capacity, double degrees, bool valid) {
    if (valid) {
        snprintf(text, capacity, "%.2f", degrees);
    } else {
        snprintf(text, capacity, "ERR");
    }
}

void printScreen(Application* app, int64_t currentTimeUs) {
    const int64_t microsecondsPerMillisecond = 1000;
    const uint64_t millisecondsPerSecond = 1000;
    const size_t angleTextCapacity = 16;
    AngleReading left = getSensorReading(&app->sensors, Sensor_LeftAngle);
    AngleReading right = getSensorReading(&app->sensors, Sensor_RightAngle);
    AngleReading magnetic = getSensorReading(&app->sensors, Sensor_MagneticAngle);
    char leftText[angleTextCapacity];
    char rightText[angleTextCapacity];
    char magneticText[angleTextCapacity];
    formatAngle(leftText, sizeof(leftText), left.degrees, left.valid);
    formatAngle(rightText, sizeof(rightText), right.degrees, right.valid);
    formatAngle(magneticText, sizeof(magneticText), magnetic.degrees, magnetic.valid);
    uint64_t elapsedMs = (currentTimeUs - app->startUs) / microsecondsPerMillisecond;

    if (app->displayedRows++ % app->rowsPerHeader == 0) {
        Serial.printf("   Time (s) | %14s | %14s | %14s\n",
                      app->sensors.items[Sensor_LeftAngle].name,
                      app->sensors.items[Sensor_RightAngle].name,
                      app->sensors.items[Sensor_MagneticAngle].name);
        Serial.println("            |      (deg)     |      (deg)     |      (deg)");
    }
    Serial.printf("%8llu.%03u | %14s | %14s | %14s\n",
                  (unsigned long long)(elapsedMs / millisecondsPerSecond),
                  (unsigned)(elapsedMs % millisecondsPerSecond),
                  leftText, rightText, magneticText);
}

// ************************************************
// ***************** Main Loop ********************
// ************************************************

void loop() {
    // ready the app
    Application* app = &applicationState;
    if (!app->ready) {
        const uint32_t initializationFailureDelayMs = 100;
        delay(initializationFailureDelayMs);
        return;
    }

    // update time
    int64_t currentTimeUs = esp_timer_get_time();

    //update sensors
    updateSensors(&app->sensors);


    // print screen
    if (currentTimeUs < app->nextScreenRefreshUs) {
        delay(app->idleDelayMs);
        return;
    }
    printScreen(app, currentTimeUs);
    app->nextScreenRefreshUs = currentTimeUs + app->screenRefreshUs;

    // etc
}
