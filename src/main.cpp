#include "esp32-hal-gpio.h"
#include <Arduino.h>

constexpr uint8_t KEYPAD_TERMINAL_1_PIN = 15;
constexpr uint8_t KEYPAD_TERMINAL_2_PIN = 16;
constexpr uint8_t KEYPAD_TERMINAL_3_PIN = 17;
constexpr uint8_t KEYPAD_TERMINAL_4_PIN = 18;
constexpr uint8_t KEYPAD_TERMINAL_5_PIN = 8;
constexpr uint8_t KEYPAD_TERMINAL_6_PIN = 9;
constexpr uint8_t KEYPAD_TERMINAL_7_PIN = 10;
constexpr uint8_t KEYPAD_TERMINAL_8_PIN = 11;

constexpr int KEYPAD[8] = {KEYPAD_TERMINAL_1_PIN,
                     KEYPAD_TERMINAL_2_PIN,
                     KEYPAD_TERMINAL_3_PIN,
                     KEYPAD_TERMINAL_4_PIN,
                     KEYPAD_TERMINAL_5_PIN,
                     KEYPAD_TERMINAL_6_PIN,
                     KEYPAD_TERMINAL_7_PIN,
                     KEYPAD_TERMINAL_8_PIN};

void setup() {
    Serial.begin(115200);
    delay(1000);


    for (int i = 0; i<8; i++){
        pinMode(KEYPAD[i], INPUT_PULLUP);
    }

    digitalWrite(KEYPAD_TERMINAL_1_PIN, LOW);
    pinMode(KEYPAD_TERMINAL_1_PIN, OUTPUT);


}




void loop() {
    static unsigned long count = 0;
    Serial.print("t-");
    Serial.println(count++);


    uint8_t pinStates = 0;

    for (int i = 0; i < 8; i++) {
        pinStates |= digitalRead(KEYPAD[i]) << i;
    }
    for (int bit = 7; bit >= 0; bit--) {
        Serial.print((pinStates >> bit) & 1);
    }
    Serial.println();

    // Serial.println(pinStates, BIN);
    delay(1000);
}
