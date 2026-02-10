/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mbed.h"


static void all_off(DigitalOut &a, DigitalOut &b, DigitalOut &c) {
    a = 0; b = 0; c = 0;
}

int main() {
    DigitalOut led1(LED1), led2(LED2), led3(LED3);

    // Order: LED1, LED2, LED3, LED2, LED1 (repeat)
    const int seq[] = {1, 2, 3, 2, 1};
    const int seq_len = 5;

    while (true) {
        for (int i = 0; i < seq_len; i++) {
            all_off(led1, led2, led3);

            // Turn one LED on
            if (seq[i] == 1) led1 = 1;
            if (seq[i] == 2) led2 = 1;
            if (seq[i] == 3) led3 = 1;

            ThisThread::sleep_for(200ms); // ON time

            all_off(led1, led2, led3);
            ThisThread::sleep_for(200ms); // OFF gap before next
        }
    }
}