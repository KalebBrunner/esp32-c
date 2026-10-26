#include <Arduino.h>
#include <esp_timer.h>
#include <soc/gpio_struct.h>

constexpr uint8_t ENCODER_A_PIN = 4;
constexpr uint8_t ENCODER_B_PIN = 5;
constexpr uint8_t ENCODER_INDEX_PIN = 6;
constexpr bool INDEX_CONNECTED = false;
constexpr int32_t ENCODER_PPR = 2048;
constexpr int32_t COUNTS_PER_REVOLUTION = ENCODER_PPR * 4;
constexpr int DIRECTION_SIGN = 1;
constexpr uint32_t DISPLAY_INTERVAL_US = 100000;
constexpr size_t VELOCITY_WINDOW_SAMPLES = 10;

portMUX_TYPE encoderLock = portMUX_INITIALIZER_UNLOCKED;
volatile int64_t encoderCount = 0;
volatile uint32_t invalidTransitions = 0;
volatile uint8_t previousAB = 0;

DRAM_ATTR const int8_t QUADRATURE_STEPS[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

uint8_t IRAM_ATTR readAB() {
    uint32_t pins = GPIO.in;
    return (((pins >> ENCODER_A_PIN) & 1U) << 1) |
           ((pins >> ENCODER_B_PIN) & 1U);
}

void IRAM_ATTR onEncoderChange() {
    portENTER_CRITICAL_ISR(&encoderLock);
    uint8_t currentAB = readAB();
    if ((previousAB ^ currentAB) == 3) {
        ++invalidTransitions;
    }
    encoderCount += DIRECTION_SIGN * QUADRATURE_STEPS[(previousAB << 2) | currentAB];
    previousAB = currentAB;
    portEXIT_CRITICAL_ISR(&encoderLock);
}

int64_t countDeltas[VELOCITY_WINDOW_SAMPLES] = {};
int64_t timeDeltas[VELOCITY_WINDOW_SAMPLES] = {};
int64_t windowCountDelta = 0;
int64_t windowTimeUs = 0;
size_t windowSlot = 0;
int64_t previousCount = 0;
int64_t previousSampleUs = 0;
int64_t startUs = 0;
uint32_t displayRows = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(ENCODER_A_PIN, INPUT);
    pinMode(ENCODER_B_PIN, INPUT);
    pinMode(ENCODER_INDEX_PIN, INPUT_PULLUP);

    previousAB = readAB();
    attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), onEncoderChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), onEncoderChange, CHANGE);

    startUs = esp_timer_get_time();
    previousSampleUs = startUs;
    Serial.printf("Encoder: %ld PPR | %ld counts/rev | zero = startup position\n",
                  static_cast<long>(ENCODER_PPR), static_cast<long>(COUNTS_PER_REVOLUTION));
    Serial.println("A/B/X are GPIO snapshots. Velocity is signed, averaged over up to 1 second.");
}

void loop() {
    int64_t nowUs = esp_timer_get_time();
    if (nowUs - previousSampleUs < DISPLAY_INTERVAL_US) {
        delay(1);
        return;
    }

    portENTER_CRITICAL(&encoderLock);
    int64_t count = encoderCount;
    uint32_t errors = invalidTransitions;
    uint8_t ab = readAB();
    portEXIT_CRITICAL(&encoderLock);

    windowCountDelta -= countDeltas[windowSlot];
    windowTimeUs -= timeDeltas[windowSlot];
    countDeltas[windowSlot] = count - previousCount;
    timeDeltas[windowSlot] = nowUs - previousSampleUs;
    windowCountDelta += countDeltas[windowSlot];
    windowTimeUs += timeDeltas[windowSlot];
    windowSlot = (windowSlot + 1) % VELOCITY_WINDOW_SAMPLES;
    previousCount = count;
    previousSampleUs = nowUs;

    double velocity = static_cast<double>(windowCountDelta) * 1000000.0 /
                      (COUNTS_PER_REVOLUTION * static_cast<double>(windowTimeUs));
    int32_t wrappedCount = (count % COUNTS_PER_REVOLUTION + COUNTS_PER_REVOLUTION) %
                           COUNTS_PER_REVOLUTION;
    double angle = wrappedCount * 360.0 / COUNTS_PER_REVOLUTION;
    uint64_t elapsedMs = (nowUs - startUs) / 1000;
    char indexLevel = INDEX_CONNECTED ? ('0' + digitalRead(ENCODER_INDEX_PIN)) : '-';

    if (displayRows++ % 30 == 0) {
        Serial.println("   Time (s) | A B X |      Count | Angle (deg) | Avg (rev/s) | Invalid");
    }
    Serial.printf("%8llu.%03u | %u %u %c | %10lld | %11.2f | %+11.4f | %lu\n",
                  static_cast<unsigned long long>(elapsedMs / 1000),
                  static_cast<unsigned>(elapsedMs % 1000),
                  static_cast<unsigned>(ab >> 1), static_cast<unsigned>(ab & 1), indexLevel,
                  static_cast<long long>(count), angle, velocity, static_cast<unsigned long>(errors));
}
