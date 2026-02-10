/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"


int main() {
    DigitalOut led1(LED1), led2(LED2), led3(LED3);

    const auto base = 250ms; // base time unit
    int tick = 0;

    while (true) {
        // LED2 toggles every 250ms -> period 500ms
        led2 = !led2;

        // LED1 toggles every 500ms -> period 1s
        if (tick % 2 == 0) {
            led1 = !led1;
        }

        // LED3 toggles every 1000ms -> period 2s
        if (tick % 4 == 0) {
            led3 = !led3;
        }

        tick++;
        ThisThread::sleep_for(base);
    }
}