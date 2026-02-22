#include "mbed.h"
#include "arm_book_lib.h"
#include <string.h>
#include <chrono>

using namespace std::chrono;

// ===================== Switches (D2–D7) =====================
DigitalIn sw1(D2);  // Switch 1: Toggle GAS alarm simulation
DigitalIn sw2(D3);  // Switch 2: Request GAS state
DigitalIn sw3(D4);  // Switch 3: Toggle TEMP alarm simulation
DigitalIn sw4(D5);  // Switch 3: Request TEMP state
DigitalIn sw5(D6);  // Switch 5: Reset both alarms
DigitalIn sw6(D7);  // Switch 6: Monitoring mode ON/OFF

// Optional LED (helps you see periodic activity on a scope/LED)
DigitalOut activityLed(LED1);

// UART to PC (115200 baud as in the lab sheet) :contentReference[oaicite:1]{index=1}
UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

// ===================== System state =====================
bool gasAlarmActive  = false;
bool tempAlarmActive = false;
bool monitoringMode  = false;

// Timing
Timer t;
milliseconds lastStatusSent = 0ms;

// Debounce / edge-detect bookkeeping
static bool prevSw[6] = {false, false, false, false, false, false};
static milliseconds lastAccepted[6] = {0ms, 0ms, 0ms, 0ms, 0ms, 0ms};
const milliseconds DEBOUNCE_TIME = 200ms;

// --------------------- Small UART helpers ---------------------
void uartPrint(const char* msg)
{
    uartUsb.write(msg, strlen(msg));
}

void sendGasState()
{
    uartPrint(gasAlarmActive ? "GAS ALARM ACTIVE\r\n" : "GAS ALARM CLEAR\r\n");
}

void sendTempState()
{
    uartPrint(tempAlarmActive ? "TEMP ALARM ACTIVE\r\n" : "TEMP ALARM CLEAR\r\n");
}

void sendStatusLine()
{
    char buf[80];
    snprintf(buf, sizeof(buf),
             "STATUS | GAS:%s | TEMP:%s\r\n",
             gasAlarmActive ? "ACTIVE" : "CLEAR",
             tempAlarmActive ? "ACTIVE" : "CLEAR");
    uartPrint(buf);
    activityLed = !activityLed;
}

// Rising-edge detect + debounce: returns true once per real press
bool pressed(DigitalIn& sw, int idx)
{
    bool cur = sw.read();
    bool rising = (cur && !prevSw[idx]);
    prevSw[idx] = cur;

    if (!rising) return false;

    milliseconds now = duration_cast<milliseconds>(t.elapsed_time());
    if (now - lastAccepted[idx] < DEBOUNCE_TIME) return false;

    lastAccepted[idx] = now;
    return true;
}

// --------------------- Init ---------------------
void inputsInit()
{
    sw1.mode(PullDown);
    sw2.mode(PullDown);
    sw3.mode(PullDown);
    sw4.mode(PullDown);
    sw5.mode(PullDown);
    sw6.mode(PullDown);
}

int main()
{
    inputsInit();
    activityLed = OFF;

    t.start();

    uartPrint("Main Task 3 ready.\r\n");
    ThisThread::sleep_for(50ms);
    uartPrint("S1:D2 GAS toggle | S2:D4 GAS state | S3:D5 TEMP toggle\r\n");
    ThisThread::sleep_for(50ms);
    uartPrint("S4:D3 TEMP state | S5:D6 RESET | S6:D7 MONITOR mode\r\n\r\n");

    while (true) {

        // ---- Switch 1: Toggle gas alarm simulation ON/OFF ----
        if (pressed(sw1, 0)) {
            gasAlarmActive = !gasAlarmActive;

            sendGasState();
            if (gasAlarmActive) {
                uartPrint(" WARNING: GAS DETECTED\r\n");
            }
        }

        // ---- Switch 2: Request gas alarm state to PC ----
        if (pressed(sw2, 1)) {
            sendGasState();
        }        

        // ---- Switch 3: Toggle over-temperature alarm simulation ON/OFF ----
        if (pressed(sw3, 2)) {
            tempAlarmActive = !tempAlarmActive;

            sendTempState();
            if (tempAlarmActive) {
                uartPrint(" WARNING: TEMPERATURE TOO HIGH\r\n");
            }
        }
		
		// ---- Switch 4: Request temp alarm state to PC ----
        if (pressed(sw4, 3)) {
            sendTempState();
        }

        // ---- Switch 5: Reset both alarms ----
        if (pressed(sw5, 4)) {
            gasAlarmActive = false;
            tempAlarmActive = false;
            uartPrint("ALARMS RESET\r\n");
        }

        // ---- Switch 6: Monitoring mode ON/OFF ----
        if (pressed(sw6, 5)) {
            monitoringMode = !monitoringMode;
            uartPrint(monitoringMode ? "MONITORING MODE ON\r\n" : "MONITORING MODE OFF\r\n");
        }

        // ---- Periodic stream every ~2 seconds if monitoring is enabled ----
        if (monitoringMode) {
            milliseconds now = duration_cast<milliseconds>(t.elapsed_time());
            if (now - lastStatusSent >= 2000ms) {
                sendStatusLine();
                lastStatusSent = now;
            }
        }

        // Small delay to reduce CPU load
        ThisThread::sleep_for(10ms);
    }
}