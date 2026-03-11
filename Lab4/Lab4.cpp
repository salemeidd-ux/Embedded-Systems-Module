//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"
#include <cstdio>
#include <cstring>

//=====[Defines]===============================================================

#define TIME_INCREMENT_MS 100
#define SERIAL_UPDATE_TIME_MS 500

#define TEMP_THRESHOLD_LOW_C   30.0f   // highest sensitivity
#define TEMP_THRESHOLD_HIGH_C  70.0f   // lowest sensitivity

#define GAS_THRESHOLD_LOW_PCT  30.0f   // highest sensitivity
#define GAS_THRESHOLD_HIGH_PCT 80.0f   // lowest sensitivity

#define NUMBER_OF_AVG_SAMPLES  10

//=====[Declaration and initialization of public global objects]===============

AnalogIn potentiometer(A0);
AnalogIn lm35(A1);
AnalogIn mq2Analog(A2);   // MQ-2 AO -> A2 (through 0-3.3V safe interface)

DigitalOut alarmLed(LED1);
DigitalInOut sirenPin(PE_10);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

//=====[Declaration and initialization of public global variables]=============

float temperatureC = 0.0f;
float gasLevelPct = 0.0f;
float sensitivityPct = 0.0f;

float temperatureThresholdC = 50.0f;
float gasThresholdPct = 50.0f;

bool temperatureWarning = false;
bool gasWarning = false;
bool buzzerState = false;

int serialAccumulatedTime = 0;

//=====[Declarations (prototypes) of public functions]=========================

void inputsInit();
void outputsInit();

float analogReadingScaledWithTheLM35Formula(float analogReading);
float mapFloat(float x, float inMin, float inMax, float outMin, float outMax);
float readAverage(AnalogIn& input);

void readSensors();
void updateThresholds();
void evaluateWarnings();
void updateOutputs();
void uartTask();

//=====[Main function, the program entry point after power on or reset]========

int main()
{
    inputsInit();
    outputsInit();

    while (true) {
        readSensors();
        updateThresholds();
        evaluateWarnings();
        updateOutputs();
        uartTask();

        delay(TIME_INCREMENT_MS);
    }
}

//=====[Implementations of public functions]===================================

void inputsInit()
{
    sirenPin.mode(OpenDrain);
    sirenPin.input();   // buzzer OFF at start
}

void outputsInit()
{
    alarmLed = OFF;
}

float analogReadingScaledWithTheLM35Formula(float analogReading)
{
    return (analogReading * 3.3f / 0.01f);
}

float mapFloat(float x, float inMin, float inMax, float outMin, float outMax)
{
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

float readAverage(AnalogIn& input)
{
    float sum = 0.0f;

    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        sum += input.read();
    }

    return sum / NUMBER_OF_AVG_SAMPLES;
}

void readSensors()
{
    float potentiometerReading = readAverage(potentiometer);
    float lm35Reading = readAverage(lm35);
    float mq2Reading = readAverage(mq2Analog);

    sensitivityPct = potentiometerReading * 100.0f;
    temperatureC = analogReadingScaledWithTheLM35Formula(lm35Reading);

    // Relative gas level, not calibrated ppm
    gasLevelPct = mq2Reading * 100.0f;
}

void updateThresholds()
{
    // Higher potentiometer value => higher sensitivity => lower thresholds
    temperatureThresholdC = mapFloat(
        sensitivityPct,
        0.0f, 100.0f,
        TEMP_THRESHOLD_HIGH_C, TEMP_THRESHOLD_LOW_C
    );

    gasThresholdPct = mapFloat(
        sensitivityPct,
        0.0f, 100.0f,
        GAS_THRESHOLD_HIGH_PCT, GAS_THRESHOLD_LOW_PCT
    );
}

void evaluateWarnings()
{
    temperatureWarning = (temperatureC > temperatureThresholdC);
    gasWarning = (gasLevelPct > gasThresholdPct);

    buzzerState = (temperatureWarning || gasWarning);
}

void updateOutputs()
{
    if (buzzerState) {
        sirenPin.output();
        sirenPin = LOW;      // active buzzer ON
        alarmLed = ON;
    } else {
        sirenPin.input();    // active buzzer OFF
        alarmLed = OFF;
    }
}

void uartTask()
{
    char str[220];
    int stringLength;

    serialAccumulatedTime += TIME_INCREMENT_MS;

    if (serialAccumulatedTime >= SERIAL_UPDATE_TIME_MS) {
        serialAccumulatedTime = 0;

        if (temperatureWarning && gasWarning) {
            sprintf(
                str,
                "Temp: %.2f C | Gas: %.2f %% | Sensitivity: %.1f %% | Temp Th: %.2f C | Gas Th: %.2f %% | Buzzer ON - Cause: Temperature and Gas\r\n",
                temperatureC,
                gasLevelPct,
                sensitivityPct,
                temperatureThresholdC,
                gasThresholdPct
            );
        } else if (temperatureWarning) {
            sprintf(
                str,
                "Temp: %.2f C | Gas: %.2f %% | Sensitivity: %.1f %% | Temp Th: %.2f C | Gas Th: %.2f %% | Buzzer ON - Cause: Temperature\r\n",
                temperatureC,
                gasLevelPct,
                sensitivityPct,
                temperatureThresholdC,
                gasThresholdPct
            );
        } else if (gasWarning) {
            sprintf(
                str,
                "Temp: %.2f C | Gas: %.2f %% | Sensitivity: %.1f %% | Temp Th: %.2f C | Gas Th: %.2f %% | Buzzer ON - Cause: Gas\r\n",
                temperatureC,
                gasLevelPct,
                sensitivityPct,
                temperatureThresholdC,
                gasThresholdPct
            );
        } else {
            sprintf(
                str,
                "Temp: %.2f C | Gas: %.2f %% | Sensitivity: %.1f %% | Temp Th: %.2f C | Gas Th: %.2f %% | System Normal\r\n",
                temperatureC,
                gasLevelPct,
                sensitivityPct,
                temperatureThresholdC,
                gasThresholdPct
            );
        }

        stringLength = strlen(str);
        uartUsb.write(str, stringLength);
    }
}