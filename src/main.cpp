#include <Arduino.h>
#include <esp_timer.h>
#include <soc/gpio_struct.h>

enum {
    AMT103_CAPACITY = 2,
    LEFT_ENCODER = 0,
    RIGHT_ENCODER = 1,
    QUADRATURE_COUNTS_PER_PULSE = 4,
    BITS_PER_AB_STATE = 2,
    BOTH_CHANNELS_CHANGED = 3
};

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

struct AMT103Internal {
    bool allocated;
    AMT103Config config;
    portMUX_TYPE lock;
    volatile int64_t count;
    volatile uint32_t invalidTransitions;
    volatile uint8_t previousAB;
    AMT103State snapshot;
};

typedef struct {
    AMT103Handle encoders[AMT103_CAPACITY];
    size_t count;
} Sensors;

typedef struct {
    int64_t refreshIntervalUs;
    uint32_t rowsPerHeader;
} DisplaySettings;

typedef struct {
    DisplaySettings display;
    uint32_t idleDelayMs;
} AppConfig;

typedef struct {
    Sensors sensors;
    AppConfig config;
    int64_t startUs;
    int64_t currentTimeUs;
    int64_t nextScreenRefreshUs;
    uint32_t displayedRows;
    bool ready;
} Application;

static uint8_t IRAM_ATTR readAB(AMT103Handle encoder) {
    const uint32_t bitMask = 1U;
    const unsigned aStateBit = 1;
    uint32_t pins = GPIO.in;
    return (((pins >> encoder->config.pinA) & bitMask) << aStateBit) |
           ((pins >> encoder->config.pinB) & bitMask);
}

static void IRAM_ATTR onEncoderChange(void* argument) {
    static DRAM_ATTR const int8_t quadratureSteps[] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };
    AMT103Handle encoder = (AMT103Handle)argument;

    portENTER_CRITICAL_ISR(&encoder->lock);
    uint8_t currentAB = readAB(encoder);
    if ((encoder->previousAB ^ currentAB) == BOTH_CHANNELS_CHANGED) {
        ++encoder->invalidTransitions;
    }
    unsigned transition = (encoder->previousAB << BITS_PER_AB_STATE) | currentAB;
    encoder->count += encoder->config.directionSign * quadratureSteps[transition];
    encoder->previousAB = currentAB;
    portEXIT_CRITICAL_ISR(&encoder->lock);
}

bool createAmtEncoder(AMT103Handle* handle, const AMT103Config* config) {
    static struct AMT103Internal pool[AMT103_CAPACITY] = {};
    const uint8_t highestSupportedPin = 21;
    if (handle == NULL || config == NULL || *handle != NULL ||
        config->pinA > highestSupportedPin || config->pinB > highestSupportedPin ||
        config->pinA == config->pinB || config->pulsesPerRevolution <= 0 ||
        (config->directionSign != 1 && config->directionSign != -1)) {
        return false;
    }

    AMT103Handle available = NULL;
    for (size_t i = 0; i < AMT103_CAPACITY; ++i) {
        if (!pool[i].allocated) {
            if (available == NULL) {
                available = &pool[i];
            }
            continue;
        }
        const AMT103Config* existing = &pool[i].config;
        if (config->pinA == existing->pinA || config->pinA == existing->pinB ||
            config->pinB == existing->pinA || config->pinB == existing->pinB) {
            return false;
        }
    }
    if (available == NULL) {
        return false;
    }

    available->config = *config;
    portMUX_INITIALIZE(&available->lock);
    pinMode(config->pinA, INPUT);
    pinMode(config->pinB, INPUT);
    available->previousAB = readAB(available);
    available->allocated = true;
    attachInterruptArg(config->pinA, onEncoderChange, available, CHANGE);
    attachInterruptArg(config->pinB, onEncoderChange, available, CHANGE);
    *handle = available;
    return true;
}

void updateSensors(Sensors* sensors) {
    const double degreesPerRevolution = 360.0;
    for (size_t i = 0; i < sensors->count; ++i) {
        AMT103Handle encoder = sensors->encoders[i];
        if (encoder == NULL) {
            continue;
        }
        AMT103State state = {};
        portENTER_CRITICAL(&encoder->lock);
        state.count = encoder->count;
        state.invalidTransitions = encoder->invalidTransitions;
        state.timestampUs = esp_timer_get_time();
        portEXIT_CRITICAL(&encoder->lock);

        int64_t countsPerRevolution = (int64_t)encoder->config.pulsesPerRevolution *
                                     QUADRATURE_COUNTS_PER_PULSE;
        int64_t wrappedCount = (state.count % countsPerRevolution + countsPerRevolution) %
                               countsPerRevolution;
        state.degrees = wrappedCount * degreesPerRevolution / countsPerRevolution;
        state.valid = true;
        encoder->snapshot = state;
    }
}

AMT103State getEncoderState(AMT103Handle encoder) {
    AMT103State unavailable = {};
    return encoder != NULL ? encoder->snapshot : unavailable;
}

Application* getApplication(void) {
    static Application instance = {};
    return &instance;
}

Application* createApp(const AppConfig* config) {
    if (config == NULL || config->display.refreshIntervalUs <= 0 ||
        config->display.rowsPerHeader == 0 || config->idleDelayMs == 0) {
        return NULL;
    }
    Application* app = getApplication();
    app->config = *config;
    app->startUs = esp_timer_get_time();
    app->currentTimeUs = app->startUs;
    app->nextScreenRefreshUs = app->startUs + config->display.refreshIntervalUs;
    return app;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    const AppConfig config = {
        .display = {
            .refreshIntervalUs = 100000,
            .rowsPerHeader = 30
        },
        .idleDelayMs = 1
    };
    Application* app = createApp(&config);
    if (app == NULL) {
        Serial.println("Could not create application.");
        return;
    }

    const AMT103Config leftConfig = {
        .description = "Left",
        .pinA = 5,
        .pinB = 4,
        .pulsesPerRevolution = 2048,
        .directionSign = 1
    };
    if (!createAmtEncoder(&app->sensors.encoders[LEFT_ENCODER], &leftConfig)) {
        Serial.println("Could not create Left encoder.");
        return;
    }
    ++app->sensors.count;

    const AMT103Config rightConfig = {
        .description = "Right",
        .pinA = 7,
        .pinB = 6,
        .pulsesPerRevolution = 2048,
        .directionSign = 1
    };
    if (!createAmtEncoder(&app->sensors.encoders[RIGHT_ENCODER], &rightConfig)) {
        Serial.println("Could not create Right encoder.");
        return;
    }
    ++app->sensors.count;

    app->ready = true;
}

void printScreen(Application* app) {
    const int64_t microsecondsPerMillisecond = 1000;
    const uint64_t millisecondsPerSecond = 1000;
    AMT103State left = getEncoderState(app->sensors.encoders[LEFT_ENCODER]);
    AMT103State right = getEncoderState(app->sensors.encoders[RIGHT_ENCODER]);
    uint64_t elapsedMs = (app->currentTimeUs - app->startUs) / microsecondsPerMillisecond;

    if (app->displayedRows++ % app->config.display.rowsPerHeader == 0) {
        Serial.println("   Time (s) |       Left (deg) |      Right (deg)");
    }
    Serial.printf("%8llu.%03u | %16.2f | %16.2f\n",
                  (unsigned long long)(elapsedMs / millisecondsPerSecond),
                  (unsigned)(elapsedMs % millisecondsPerSecond),
                  left.degrees, right.degrees);
}

void loop() {
    Application* app = getApplication();
    if (!app->ready) {
        const uint32_t initializationFailureDelayMs = 100;
        delay(initializationFailureDelayMs);
        return;
    }

    updateSensors(&app->sensors);

    app->currentTimeUs = esp_timer_get_time();
    if (app->currentTimeUs < app->nextScreenRefreshUs) {
        delay(app->config.idleDelayMs);
        return;
    }
    printScreen(app);
    app->nextScreenRefreshUs = app->currentTimeUs + app->config.display.refreshIntervalUs;
}
