/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"

int main() {
    DigitalOut led1(LED1), led2(LED2), led3(LED3);

    // Phase A: 5 synchronous blinks
    for (int i = 0; i < 5; i++) {
        led1 = 1; led2 = 1; led3 = 1;
        ThisThread::sleep_for(200ms);

        led1 = 0; led2 = 0; led3 = 0;
        ThisThread::sleep_for(200ms);
    }

    // Phase B: final steady state
    led1 = 1;
    led2 = 0;
    led3 = 0;

    while (true) {
        // hold final state forever
        ThisThread::sleep_for(1000ms);
    }
}
