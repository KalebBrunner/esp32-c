#include <Arduino.h>
#include <esp_timer.h>
#include "sensors.h"
#include "display.h"

typedef struct {
    Sensors sensors;
    int64_t screenRefreshUs;
    uint32_t rowsPerHeader;
    uint32_t idleDelayMs;
    int64_t startUs;
    int64_t nextScreenRefreshUs;
    uint32_t displayedRows;
} Application;

Application applicationState = {};

void setup() {
    Serial.begin(115200);
    delay(1000);

    Application* app = &applicationState;
    app->screenRefreshUs = 100000;
    app->rowsPerHeader = 30;
    app->idleDelayMs = 1;
    app->startUs = esp_timer_get_time();
    app->nextScreenRefreshUs = app->startUs + app->screenRefreshUs;

    startSensors(&app->sensors);
}


static void formatAngle(char* text, size_t capacity, AngleReading reading) {
    if (reading.valid) {
        snprintf(text, capacity, "%.2f", reading.degrees);
    } else {
        snprintf(text, capacity, "ERR");
    }
}

void printScreen(const Sensors* sensors, int64_t elapsedUs, bool showHeader) {
    const int64_t microsecondsPerMillisecond = 1000;
    const uint64_t millisecondsPerSecond = 1000;
    const size_t angleTextCapacity = 16;
    char leftText[angleTextCapacity];
    char rightText[angleTextCapacity];
    char magneticText[angleTextCapacity];
    formatAngle(leftText, sizeof(leftText), getSensorReading(sensors, Sensor_LeftAngle));
    formatAngle(rightText, sizeof(rightText), getSensorReading(sensors, Sensor_RightAngle));
    formatAngle(magneticText, sizeof(magneticText), getSensorReading(sensors, Sensor_MagneticAngle));
    uint64_t elapsedMs = elapsedUs / microsecondsPerMillisecond;

    if (showHeader) {
        Serial.println("   Time (s) |     Left angle |    Right angle | Magnetic angle");
        Serial.println("            |      (deg)     |      (deg)     |      (deg)");
    }
    Serial.printf("%8llu.%03u | %14s | %14s | %14s\n",
                  (unsigned long long)(elapsedMs / millisecondsPerSecond),
                  (unsigned)(elapsedMs % millisecondsPerSecond),
                  leftText, rightText, magneticText);
}

void loop() {
    Application* app = &applicationState;
    updateSensors(&app->sensors);

    int64_t currentTimeUs = esp_timer_get_time();
    if (currentTimeUs < app->nextScreenRefreshUs) {
        delay(app->idleDelayMs);
        return;
    }

    bool showHeader = app->displayedRows++ % app->rowsPerHeader == 0;
    printScreen(&app->sensors, currentTimeUs - app->startUs, showHeader);
    app->nextScreenRefreshUs = currentTimeUs + app->screenRefreshUs;
}
