#include "amt103.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <soc/gpio_struct.h>

const int QUADRATURE_COUNTS_PER_PULSE = 4;
const int BITS_PER_AB_STATE = 2;
const uint8_t BOTH_CHANNELS_CHANGED = 0b11;

static DRAM_ATTR const int8_t quadratureSteps[] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

static uint8_t IRAM_ATTR readAB(AMT103* encoder) {
    const uint32_t bitMask = 1U;
    const unsigned aStateBit = 1;
    uint32_t pins = GPIO.in;
    return (((pins >> encoder->config.pinA) & bitMask) << aStateBit) |
           ((pins >> encoder->config.pinB) & bitMask);
}

static void IRAM_ATTR amt103OnPinChange(void* argument) {
    AMT103* encoder = (AMT103*)argument;

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

void amt103Start(AMT103* encoder) {
    portMUX_INITIALIZE(&encoder->lock);
    pinMode(encoder->config.pinA, INPUT);
    pinMode(encoder->config.pinB, INPUT);
    encoder->previousAB = readAB(encoder);
    attachInterruptArg(encoder->config.pinA, amt103OnPinChange, encoder, CHANGE);
    attachInterruptArg(encoder->config.pinB, amt103OnPinChange, encoder, CHANGE);
}

void amt103CaptureSnapshot(AMT103* encoder) {
    const double degreesPerRevolution = 360.0;
    if (encoder == NULL) {
        return;
    }
    AMT103Sample newSample = {};
    portENTER_CRITICAL(&encoder->lock);
    newSample.count = encoder->count;
    newSample.invalidTransitions = encoder->invalidTransitions;
    newSample.timestampUs = esp_timer_get_time();
    portEXIT_CRITICAL(&encoder->lock);

    int64_t countsPerRevolution = (int64_t)encoder->config.pulsesPerRevolution *
                                 QUADRATURE_COUNTS_PER_PULSE;
    int64_t wrappedCount = (newSample.count % countsPerRevolution + countsPerRevolution) %
                           countsPerRevolution;
    newSample.degrees = wrappedCount * degreesPerRevolution / countsPerRevolution;
    newSample.valid = true;
    encoder->latestSample = newSample;
}
