#include <Arduino.h>
#include <esp_timer.h>
#include <soc/gpio_struct.h>

typedef struct {
    const char* description;
    uint8_t pinA;
    uint8_t pinB;
    int32_t pulsesPerRevolution;
    int8_t directionSign;
} AmtEncoderSettings;

typedef struct {
    int64_t count;
    double degrees;
    uint32_t invalidTransitions;
} AmtEncoderState;

typedef struct {
    AmtEncoderSettings settings;
    portMUX_TYPE lock;
    volatile int64_t count;
    volatile uint32_t invalidTransitions;
    volatile uint8_t previousAB;
} AmtEncoder;

uint8_t IRAM_ATTR readAB(const AmtEncoder* encoder) {
    const uint32_t bitMask = 1U;
    const unsigned aStateBit = 1;
    uint32_t pins = GPIO.in;
    return (((pins >> encoder->settings.pinA) & bitMask) << aStateBit) |
           ((pins >> encoder->settings.pinB) & bitMask);
}

void IRAM_ATTR onEncoderChange(void* argument) {
    static DRAM_ATTR const int8_t quadratureSteps[] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };
    const unsigned bitsPerState = 2;
    const uint8_t bothChannelsChanged = 0x03;
    AmtEncoder* encoder = (AmtEncoder*)argument;

    portENTER_CRITICAL_ISR(&encoder->lock);
    uint8_t currentAB = readAB(encoder);
    if ((encoder->previousAB ^ currentAB) == bothChannelsChanged) {
        ++encoder->invalidTransitions;
    }
    unsigned transition = (encoder->previousAB << bitsPerState) | currentAB;
    encoder->count += encoder->settings.directionSign * quadratureSteps[transition];
    encoder->previousAB = currentAB;
    portEXIT_CRITICAL_ISR(&encoder->lock);
}

void beginEncoder(AmtEncoder* encoder) {
    pinMode(encoder->settings.pinA, INPUT);
    pinMode(encoder->settings.pinB, INPUT);
    encoder->previousAB = readAB(encoder);
    attachInterruptArg(encoder->settings.pinA, onEncoderChange, encoder, CHANGE);
    attachInterruptArg(encoder->settings.pinB, onEncoderChange, encoder, CHANGE);
}

AmtEncoderState getEncoderState(AmtEncoder* encoder) {
    const int32_t quadratureCountsPerPulse = 4;
    const double degreesPerRevolution = 360.0;

    portENTER_CRITICAL(&encoder->lock);
    int64_t count = encoder->count;
    uint32_t invalidTransitions = encoder->invalidTransitions;
    portEXIT_CRITICAL(&encoder->lock);

    int64_t countsPerRevolution = (int64_t)encoder->settings.pulsesPerRevolution *
                                 quadratureCountsPerPulse;
    int64_t wrappedCount = (count % countsPerRevolution + countsPerRevolution) % countsPerRevolution;
    double degrees = wrappedCount * degreesPerRevolution / countsPerRevolution;
    AmtEncoderState state = {count, degrees, invalidTransitions};
    return state;
}

typedef struct {
    uint32_t serialBaud;
    uint32_t startupDelayMs;
    int64_t refreshIntervalUs;
    uint32_t idleDelayMs;
    uint32_t rowsPerHeader;
} DisplaySettings;

typedef struct {
    AmtEncoder left;
    AmtEncoder right;
    DisplaySettings display;
    int64_t startUs;
    int64_t previousDisplayUs;
    uint32_t displayedRows;
} Application;

Application* getApplication(void) {
    static Application instance = {
        {
            {"Left", 5, 4, 2048, 1},
            portMUX_INITIALIZER_UNLOCKED,
            0, 0, 0
        },
        {
            {"Right", 7, 6, 2048, 1},
            portMUX_INITIALIZER_UNLOCKED,
            0, 0, 0
        },
        {115200, 1000, 100000, 1, 30},
        0, 0, 0
    };
    return &instance;
}

void setup() {
    Application* app = getApplication();
    Serial.begin(app->display.serialBaud);
    delay(app->display.startupDelayMs);
    beginEncoder(&app->left);
    beginEncoder(&app->right);
    app->startUs = esp_timer_get_time();
    app->previousDisplayUs = app->startUs;
}

void loop() {
    const int64_t microsecondsPerMillisecond = 1000;
    const uint64_t millisecondsPerSecond = 1000;
    Application* app = getApplication();
    int64_t nowUs = esp_timer_get_time();
    if (nowUs - app->previousDisplayUs < app->display.refreshIntervalUs) {
        delay(app->display.idleDelayMs);
        return;
    }
    app->previousDisplayUs = nowUs;

    AmtEncoderState left = getEncoderState(&app->left);
    AmtEncoderState right = getEncoderState(&app->right);
    uint64_t elapsedMs = (nowUs - app->startUs) / microsecondsPerMillisecond;

    if (app->displayedRows++ % app->display.rowsPerHeader == 0) {
        Serial.printf("   Time (s) | %10s (deg) | %10s (deg)\n",
                      app->left.settings.description, app->right.settings.description);
    }
    Serial.printf("%8llu.%03u | %16.2f | %16.2f\n",
                  (unsigned long long)(elapsedMs / millisecondsPerSecond),
                  (unsigned)(elapsedMs % millisecondsPerSecond),
                  left.degrees, right.degrees);
}
