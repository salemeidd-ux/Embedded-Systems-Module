#include "mbed.h"
#include "arm_book_lib.h"

// Digits 0..5 on six buttons (active-high)
DigitalIn key0(D2), key1(D3), key2(D4), key3(D5), key4(D6), key5(D7);

// LEDs
DigitalOut ledA(LED1), ledB(LED2), ledC(LED3);

// Codes
static const int USER_CODE[4]  = {1, 2, 3, 4};
static const int ADMIN_CODE[4] = {5, 5, 5,5 };

// Timing (ms)
static const int LOOP_MS              = 20;
static const int WARNING_TIME_MS      = 30000;
static const int WARNING_BLINK_MS     = 500;
static const int LOCKDOWN_BLINK_MS    = 250;
static const int LOCKDOWN_BLINKTIME_MS= 60000;

enum State { NORMAL, WARNING, LOCKDOWN };

static bool match4(const int buf[4], const int code[4]) {
    for (int i = 0; i < 4; i++) if (buf[i] != code[i]) return false;
    return true;
}

static void showCounter3bit(uint8_t v) {
    ledA = (v & 0x01) ? ON : OFF;
    ledB = (v & 0x02) ? ON : OFF;
    ledC = (v & 0x04) ? ON : OFF;
}

// Rising-edge detector (no debounce)
struct Edge {
    bool last = false;
    bool rising(bool now) {
        bool r = (!last && now);
        last = now;
        return r;
    }
};

int main() {
    // Active-high => PullDown
    key0.mode(PullDown); key1.mode(PullDown); key2.mode(PullDown);
    key3.mode(PullDown); key4.mode(PullDown); key5.mode(PullDown);

    ledA = OFF; ledB = OFF; ledC = OFF;

    Edge e0, e1, e2, e3, e4, e5;

    State state = NORMAL;

    int entry[4] = {-1, -1, -1, -1};
    int entryLen = 0;

    int fails = 0;
    bool afterWarning = false;

    uint8_t lockdownCounter = 0;

    Timer t;
    t.start();

    int warningStart = 0, lockdownStart = 0, lastBlink = 0;

    while (true) {
        int now = (int)chrono::duration_cast<chrono::milliseconds>(t.elapsed_time()).count();

        // Detect a single pressed digit (rising edge only)
        int digit = -1;
        if (e0.rising(key0.read())) digit = 0;
        else if (e1.rising(key1.read())) digit = 1;
        else if (e2.rising(key2.read())) digit = 2;
        else if (e3.rising(key3.read())) digit = 3;
        else if (e4.rising(key4.read())) digit = 4;
        else if (e5.rising(key5.read())) digit = 5;

        if (state == NORMAL) {
            // Display lockdown event counter in NORMAL (3-bit)
            showCounter3bit(lockdownCounter);

            if (digit != -1) {
                entry[entryLen++] = digit;

                if (entryLen == 4) {
                    if (match4(entry, USER_CODE)) {
                        fails = 0;
                        afterWarning = false;
                        lockdownCounter=0;
                    } else {
                        if (afterWarning) {
                            // immediate lockdown
                            state = LOCKDOWN;
                            lockdownStart = now;
                            lastBlink = now;
                            
                        } else {
                            fails++;
                            lockdownCounter+=1;
                            if (fails >= 3) {
                                state = WARNING;
                                warningStart = now;
                                lastBlink = now;
                                ledA = OFF; ledB = OFF; ledC = OFF;
                            }
                        }
                    }
                    entryLen = 0;
                }
            }
        }
        else if (state == WARNING) {
            // Block inputs
            entryLen = 0;

            // Slow blink LED1
            ledB = OFF; ledC = OFF;
            if (now - lastBlink >= WARNING_BLINK_MS) {
                ledA = !ledA;
                lastBlink = now;
            }

            if (now - warningStart >= WARNING_TIME_MS) {
                state = NORMAL;
                afterWarning = true;
                ledA = OFF; // counter display resumes in NORMAL
            }
        }
        else { // LOCKDOWN
            // LED1 solid, LED2 blinks for 1 minute
            ledA = ON;
            ledC = OFF;

            if (now - lockdownStart < LOCKDOWN_BLINKTIME_MS) {
                if (now - lastBlink >= LOCKDOWN_BLINK_MS) {
                    ledB = !ledB;
                    lastBlink = now;
                }
            } else {
                ledB = OFF;
            }

            // Only admin code unlocks
            if (digit != -1) {
                entry[entryLen++] = digit;

                if (entryLen == 4) {
                    if (match4(entry, ADMIN_CODE)) {
                        state = NORMAL;
                        fails = 0;
                        lockdownCounter=0;
                        afterWarning = false;
                        entryLen = 0;
                        ledA = OFF; ledB = OFF; ledC = OFF;
                    } else {
                        entryLen = 0;
                    }
                }
            }
        }

        ThisThread::sleep_for(chrono::milliseconds(LOOP_MS));
    }
}
