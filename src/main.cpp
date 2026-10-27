#include <Arduino.h>
#include <esp_timer.h>
#include "amt103.h"
#include "mt6701.h"
#include "board_pins.h"
#include <math.h>

enum AngleId {
    Angle_Turret,
    Angle_Shoulder,
    Angle_LittleNolan,
    Angle_BigNolan,
    Angle_Count
};

typedef struct {
    MT6701 turret;
    MT6701 shoulder;
    MT6701 littleNolan;
    AMT103 bigNolan;
} Sensors;

typedef struct {
    Sensors sensors;
    int64_t screenRefreshUs;
    uint32_t rowsPerHeader;
    uint32_t idleDelayMs;
    int64_t startUs;
    int64_t nextScreenRefreshUs;
    uint32_t displayedRows;
} Application;


void initSensors(Application* app){
    /*
    app->sensors.bigNolan.config = {
        .pinA = Pin_BigNolan_ChannelA,
        .pinB = Pin_BigNolan_ChannelB,
        .pulsesPerRevolution = 2048,
        .directionSign = 1
    };
    amt103Start(&app->sensors.bigNolan);
    */

    const uint32_t busFrequencyHz = 100000;
    const uint16_t busTimeoutMs = 5;
    Wire.begin(Pin_SDA, Pin_SCL, busFrequencyHz);
    Wire.setTimeOut(busTimeoutMs);

    const uint8_t multiplexerAddress = 0x70;
    const uint8_t turretChannel = 0;
    Wire.beginTransmission(multiplexerAddress);
    Wire.write((uint8_t)(1U << turretChannel));
    Wire.endTransmission();

    app->sensors.turret.config = {
        .bus = &Wire,
        .address = 0x06,
        .directionSign = 1,
        .zeroOffsetDegrees = 0.0
    };
    app->sensors.shoulder.config = {
        .bus = NULL,
        .address = 0x06,
        .directionSign = 1,
        .zeroOffsetDegrees = 0.0
    };
    app->sensors.littleNolan.config = {
        .bus = NULL,
        .address = 0x06,
        .directionSign = 1,
        .zeroOffsetDegrees = 0.0
    };
}


Application applicationState = {};

void setup() {
    Serial.begin(115200);
    delay(1000);

    Application* app = &applicationState;
    app->screenRefreshUs = 100000;
    app->rowsPerHeader = 30;
    app->idleDelayMs = 1;

    initSensors(app);

    app->startUs = esp_timer_get_time();
    app->nextScreenRefreshUs = app->startUs + app->screenRefreshUs;
}

void updateAngles(Sensors* sensors, double angles[Angle_Count]) {
    angles[Angle_Turret] = captureMt6701Angle(&sensors->turret);
    // angles[Angle_Shoulder] = captureMt6701Angle(&sensors->shoulder);
    // angles[Angle_LittleNolan] = captureMt6701Angle(&sensors->littleNolan);

    // angles[Angle_BigNolan] = captureAmt103Angle(&sensors->bigNolan);
}

static void formatAngle(char* text, size_t capacity, double angle) {
    if (isfinite(angle)) {
        snprintf(text, capacity, "%.2f", angle);
    } else {
        snprintf(text, capacity, "--");
    }
}

void printScreen(const double angles[Angle_Count], int64_t elapsedUs, bool showHeader) {
    const int64_t microsecondsPerMillisecond = 1000;
    const uint64_t millisecondsPerSecond = 1000;
    const size_t angleTextCapacity = 16;
    char angleText[Angle_Count][angleTextCapacity];
    formatAngle(angleText[Angle_Turret], sizeof(angleText[Angle_Turret]), angles[Angle_Turret]);
    // formatAngle(angleText[Angle_Shoulder], sizeof(angleText[Angle_Shoulder]), angles[Angle_Shoulder]);
    // formatAngle(angleText[Angle_LittleNolan], sizeof(angleText[Angle_LittleNolan]), angles[Angle_LittleNolan]);
    // formatAngle(angleText[Angle_BigNolan], sizeof(angleText[Angle_BigNolan]), angles[Angle_BigNolan]);
    uint64_t elapsedMs = elapsedUs / microsecondsPerMillisecond;

    if (showHeader) {
        Serial.println("   Time (s) | Turret (deg)");
        // Serial.println("   Time (s) | Turret (deg) | Shoulder (deg) | Little Nolan (deg) | Big Nolan (deg)");
    }
    Serial.printf("%8llu.%03u | %12s\n",
                  (unsigned long long)(elapsedMs / millisecondsPerSecond),
                  (unsigned)(elapsedMs % millisecondsPerSecond),
                  angleText[Angle_Turret]);
    /*
    Serial.printf("%8llu.%03u | %12s | %14s | %18s | %15s\n",
                  (unsigned long long)(elapsedMs / millisecondsPerSecond),
                  (unsigned)(elapsedMs % millisecondsPerSecond),
                  angleText[Angle_Turret], angleText[Angle_Shoulder],
                  angleText[Angle_LittleNolan], angleText[Angle_BigNolan]);
    */
}

void loop() {
    Application* app = &applicationState;
    int64_t currentTimeUs = esp_timer_get_time();
    if (currentTimeUs < app->nextScreenRefreshUs) {
        delay(app->idleDelayMs);
        return;
    }

    double angles[Angle_Count] = {};
    updateAngles(&app->sensors, angles);

    currentTimeUs = esp_timer_get_time();
    bool showHeader = app->displayedRows++ % app->rowsPerHeader == 0;
    printScreen(angles, currentTimeUs - app->startUs, showHeader);
    app->nextScreenRefreshUs = currentTimeUs + app->screenRefreshUs;
}
