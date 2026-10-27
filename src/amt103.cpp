#include "amt103.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <soc/gpio_struct.h>

enum {
    QUADRATURE_COUNTS_PER_PULSE = 4,
    BITS_PER_AB_STATE = 2,
    BOTH_CHANNELS_CHANGED = 3
};

struct AMT103Internal {
    bool allocated;
    AMT103Config config;
    portMUX_TYPE lock;
    volatile int64_t count;
    volatile uint32_t invalidTransitions;
    volatile uint8_t previousAB;
    AMT103State snapshot;
};

static struct AMT103Internal encoderPool[AMT103_CAPACITY] = {};

static DRAM_ATTR const int8_t quadratureSteps[] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t IRAM_ATTR readAB(AMT103Handle encoder) {
    const uint32_t bitMask = 1U;
    const unsigned aStateBit = 1;
    uint32_t pins = GPIO.in;
    return (((pins >> encoder->config.pinA) & bitMask) << aStateBit) |
           ((pins >> encoder->config.pinB) & bitMask);
}

static void IRAM_ATTR onEncoderChange(void* argument) {
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
    const uint8_t highestSupportedPin = 21;
    if (handle == NULL || config == NULL || *handle != NULL ||
        config->pinA > highestSupportedPin || config->pinB > highestSupportedPin ||
        config->pinA == config->pinB || config->pulsesPerRevolution <= 0 ||
        (config->directionSign != 1 && config->directionSign != -1)) {
        return false;
    }

    AMT103Handle available = NULL;
    for (size_t i = 0; i < AMT103_CAPACITY; ++i) {
        if (!encoderPool[i].allocated) {
            if (available == NULL) {
                available = &encoderPool[i];
            }
            continue;
        }
        const AMT103Config* existing = &encoderPool[i].config;
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

void updateEncoder(AMT103Handle encoder) {
    const double degreesPerRevolution = 360.0;
    if (encoder == NULL) {
        return;
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

AMT103State getEncoderState(AMT103Handle encoder) {
    AMT103State unavailable = {};
    return encoder != NULL ? encoder->snapshot : unavailable;
}

