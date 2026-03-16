//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

//=====[Defines]===============================================================

#define TIME_INCREMENT_MS             10
#define DEBOUNCE_KEY_TIME_MS          40
#define KEYPAD_NUMBER_OF_ROWS         4
#define KEYPAD_NUMBER_OF_COLS         4
#define NUMBER_OF_KEYS                4
#define NUMBER_OF_AVG_SAMPLES         100

#define STATUS_PRINT_TIME_MS          1000
#define PROMPT_PRINT_TIME_MS          2000

#define TEMP_THRESHOLD_MIN_C          25.0f
#define TEMP_THRESHOLD_MAX_C          37.0f
#define GAS_THRESHOLD_MIN_PPM         0.0f
#define GAS_THRESHOLD_MAX_PPM         800.0f

#define EVENT_MAX_STORAGE             5
#define EVENT_CAUSE_MAX_LENGTH        10

//=====[Declaration of public data types]======================================

typedef enum {
    MATRIX_KEYPAD_SCANNING,
    MATRIX_KEYPAD_DEBOUNCE,
    MATRIX_KEYPAD_KEY_HOLD_PRESSED
} matrixKeypadState_t;

typedef struct systemEvent {
    time_t seconds;
    float temperatureC;
    float gasPpm;
    char cause[EVENT_CAUSE_MAX_LENGTH];
} systemEvent_t;

//=====[Declaration and initialization of public global objects]===============

DigitalIn alarmTestButton(BUTTON1);

DigitalOut alarmLed(LED1);
DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

DigitalInOut sirenPin(PE_10);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn potentiometer(A0);
AnalogIn lm35(A1);
AnalogIn mq2(A2);   // Gas sensor analog input as in your Lab 4 setup

DigitalOut keypadRowPins[KEYPAD_NUMBER_OF_ROWS] = {PB_3, PB_5, PC_7, PA_15};
DigitalIn keypadColPins[KEYPAD_NUMBER_OF_COLS]  = {PB_12, PB_13, PB_15, PC_6};

//=====[Declaration and initialization of public global variables]=============

bool alarmState = OFF;

float lm35ReadingsAverage = 0.0f;
float lm35ReadingsSum = 0.0f;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float lm35TempC = 0.0f;

float gasPpm = 0.0f;
float potentiometerReading = 0.0f;
float temperatureThresholdC = TEMP_THRESHOLD_MIN_C;
float gasThresholdPpm = GAS_THRESHOLD_MIN_PPM;

bool temperatureExceeded = false;
bool gasExceeded = false;
bool previousTemperatureExceeded = false;
bool previousGasExceeded = false;

char codeSequence[NUMBER_OF_KEYS] = {'1', '8', '0', '5'};
char enteredCode[NUMBER_OF_KEYS]  = {'0', '0', '0', '0'};
int enteredCodeIndex = 0;

int accumulatedStatusPrintTime = 0;
int accumulatedPromptPrintTime = 0;

int accumulatedDebounceMatrixKeypadTime = 0;
char matrixKeypadLastKeyPressed = '\0';
matrixKeypadState_t matrixKeypadState;

char matrixKeypadIndexToCharArray[] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D',
};

systemEvent_t eventLog[EVENT_MAX_STORAGE];
int eventWriteIndex = 0;
int storedEvents = 0;

//=====[Declarations (prototypes) of public functions]=========================

void inputsInit();
void outputsInit();

void lm35ReadingsArrayInit();

float analogReadingScaledWithTheLM35Formula(float analogReading);
float mapFloat(float value, float inMin, float inMax, float outMin, float outMax);
float readAnalogAverage(AnalogIn& input, int samples);

void sensorsUpdate();
void thresholdsUpdate();
void alarmUpdate();
void alarmDeactivationUpdate();
void outputsUpdate();
void serialStatusUpdate();
void uartTask();
void availableCommands();

bool isNumericKey(char key);
bool enteredCodeIsCorrect();

void storeEvent(const char* cause);
void printEventLog();

void matrixKeypadInit();
char matrixKeypadScan();
char matrixKeypadUpdate();

//=====[Main function, the program entry point after power on or reset]========

int main()
{
    inputsInit();
    outputsInit();

    while (true) {
        sensorsUpdate();
        thresholdsUpdate();
        alarmUpdate();
        alarmDeactivationUpdate();
        outputsUpdate();
        serialStatusUpdate();
        uartTask();
        delay(TIME_INCREMENT_MS);
    }
}

//=====[Implementations of public functions]===================================

void inputsInit()
{
    lm35ReadingsArrayInit();

    alarmTestButton.mode(PullDown);

    sirenPin.mode(OpenDrain);
    sirenPin.input();

    matrixKeypadInit();
}

void outputsInit()
{
    alarmLed = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
}

void lm35ReadingsArrayInit()
{
    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsArray[i] = 0.0f;
    }
}

float analogReadingScaledWithTheLM35Formula(float analogReading)
{
    return (analogReading * 3.3f / 0.01f);
}

float mapFloat(float value, float inMin, float inMax, float outMin, float outMax)
{
    return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

float readAnalogAverage(AnalogIn& input, int samples)
{
    float sum = 0.0f;

    for (int i = 0; i < samples; i++) {
        sum += input.read();
    }

    return sum / samples;
}

void sensorsUpdate()
{
    static int lm35SampleIndex = 0;

    lm35ReadingsArray[lm35SampleIndex] = lm35.read();
    lm35SampleIndex++;

    if (lm35SampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        lm35SampleIndex = 0;
    }

    lm35ReadingsSum = 0.0f;
    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsSum += lm35ReadingsArray[i];
    }

    lm35ReadingsAverage = lm35ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    lm35TempC = analogReadingScaledWithTheLM35Formula(lm35ReadingsAverage);

    potentiometerReading = readAnalogAverage(potentiometer, 20);

    // MQ-2 analog value scaled to 0-800 ppm for task display and threshold compare
    // This is a lab-style scaled reading, not a fully calibrated ppm measurement.
    gasPpm = readAnalogAverage(mq2, 20) * 800.0f;
}

void thresholdsUpdate()
{
    temperatureThresholdC = mapFloat(
        potentiometerReading, 0.0f, 1.0f,
        TEMP_THRESHOLD_MIN_C, TEMP_THRESHOLD_MAX_C
    );

    gasThresholdPpm = mapFloat(
        potentiometerReading, 0.0f, 1.0f,
        GAS_THRESHOLD_MIN_PPM, GAS_THRESHOLD_MAX_PPM
    );
}

void alarmUpdate()
{
    temperatureExceeded = (lm35TempC > temperatureThresholdC);
    gasExceeded = (gasPpm > gasThresholdPpm);

    if (alarmTestButton) {
        temperatureExceeded = true;
        gasExceeded = true;
    }

    if (temperatureExceeded && gasExceeded &&
        (!previousTemperatureExceeded || !previousGasExceeded)) {
        storeEvent("TEMP+GAS");
        alarmState = ON;
    } else if (temperatureExceeded && !previousTemperatureExceeded) {
        storeEvent("TEMP");
        alarmState = ON;
    } else if (gasExceeded && !previousGasExceeded) {
        storeEvent("GAS");
        alarmState = ON;
    }

    previousTemperatureExceeded = temperatureExceeded;
    previousGasExceeded = gasExceeded;

    if (alarmState) {
        accumulatedPromptPrintTime += TIME_INCREMENT_MS;

        if (accumulatedPromptPrintTime >= PROMPT_PRINT_TIME_MS) {
            const char* prompt = "Enter 4-Digit Code to Deactivate\r\n";
            uartUsb.write(prompt, strlen(prompt));
            accumulatedPromptPrintTime = 0;
        }
    } else {
        accumulatedPromptPrintTime = 0;
    }
}

void alarmDeactivationUpdate()
{
    char keyReleased = matrixKeypadUpdate();

    if (keyReleased == '\0') {
        return;
    }

    if (alarmState) {
        if (isNumericKey(keyReleased)) {
            incorrectCodeLed = OFF;

            if (enteredCodeIndex < NUMBER_OF_KEYS) {
                enteredCode[enteredCodeIndex] = keyReleased;
                enteredCodeIndex++;
                uartUsb.write("*", 1);
            }
        } else if (keyReleased == '*') {
            enteredCodeIndex = 0;
            incorrectCodeLed = OFF;
            uartUsb.write("\r\nCode cleared\r\n", 16);
        } else if (keyReleased == '#') {
            uartUsb.write("\r\n", 2);

            if (enteredCodeIndex == NUMBER_OF_KEYS && enteredCodeIsCorrect()) {
                uartUsb.write("Correct code. Alarm deactivated\r\n", 33);

                alarmState = OFF;
                incorrectCodeLed = OFF;
                enteredCodeIndex = 0;

                // reset edge memory so a still-active condition can retrigger cleanly
                previousTemperatureExceeded = false;
                previousGasExceeded = false;
            } else {
                uartUsb.write("Incorrect code\r\n", 16);
                incorrectCodeLed = ON;
                enteredCodeIndex = 0;
            }
        }
    } else {
        if (keyReleased == '#') {
            printEventLog();
        }
    }
}

void outputsUpdate()
{
    if (alarmState) {
        sirenPin.output();
        sirenPin = LOW;
        alarmLed = ON;
    } else {
        sirenPin.input();
        alarmLed = OFF;
    }
}

void serialStatusUpdate()
{
    char str[180];

    accumulatedStatusPrintTime += TIME_INCREMENT_MS;

    if (accumulatedStatusPrintTime >= STATUS_PRINT_TIME_MS) {
        accumulatedStatusPrintTime = 0;

        sprintf(
            str,
            "Temp: %.2f C | Temp Th: %.2f C | Gas: %.1f ppm | Gas Th: %.1f ppm\r\n",
            lm35TempC,
            temperatureThresholdC,
            gasPpm,
            gasThresholdPpm
        );

        uartUsb.write(str, strlen(str));
    }
}

void uartTask()
{
    char receivedChar = '\0';
    char str[100];

    if (uartUsb.readable()) {
        uartUsb.read(&receivedChar, 1);

        switch (receivedChar) {

        case 's':
        case 'S': {
            struct tm rtcTime;
            int strIndex;

            uartUsb.write("\r\nType four digits for the current year (YYYY): ", 48);
            for (strIndex = 0; strIndex < 4; strIndex++) {
                uartUsb.read(&str[strIndex], 1);
                uartUsb.write(&str[strIndex], 1);
            }
            str[4] = '\0';
            rtcTime.tm_year = atoi(str) - 1900;
            uartUsb.write("\r\n", 2);

            uartUsb.write("Type two digits for the current month (01-12): ", 47);
            for (strIndex = 0; strIndex < 2; strIndex++) {
                uartUsb.read(&str[strIndex], 1);
                uartUsb.write(&str[strIndex], 1);
            }
            str[2] = '\0';
            rtcTime.tm_mon = atoi(str) - 1;
            uartUsb.write("\r\n", 2);

            uartUsb.write("Type two digits for the current day (01-31): ", 45);
            for (strIndex = 0; strIndex < 2; strIndex++) {
                uartUsb.read(&str[strIndex], 1);
                uartUsb.write(&str[strIndex], 1);
            }
            str[2] = '\0';
            rtcTime.tm_mday = atoi(str);
            uartUsb.write("\r\n", 2);

            uartUsb.write("Type two digits for the current hour (00-23): ", 46);
            for (strIndex = 0; strIndex < 2; strIndex++) {
                uartUsb.read(&str[strIndex], 1);
                uartUsb.write(&str[strIndex], 1);
            }
            str[2] = '\0';
            rtcTime.tm_hour = atoi(str);
            uartUsb.write("\r\n", 2);

            uartUsb.write("Type two digits for the current minutes (00-59): ", 49);
            for (strIndex = 0; strIndex < 2; strIndex++) {
                uartUsb.read(&str[strIndex], 1);
                uartUsb.write(&str[strIndex], 1);
            }
            str[2] = '\0';
            rtcTime.tm_min = atoi(str);
            uartUsb.write("\r\n", 2);

            uartUsb.write("Type two digits for the current seconds (00-59): ", 49);
            for (strIndex = 0; strIndex < 2; strIndex++) {
                uartUsb.read(&str[strIndex], 1);
                uartUsb.write(&str[strIndex], 1);
            }
            str[2] = '\0';
            rtcTime.tm_sec = atoi(str);
            uartUsb.write("\r\n", 2);

            rtcTime.tm_isdst = -1;
            set_time(mktime(&rtcTime));
            uartUsb.write("Date and time has been set\r\n", 28);
            break;
        }

        case 't':
        case 'T': {
            time_t epochSeconds = time(NULL);
            sprintf(str, "Date and Time = %s\r\n", ctime(&epochSeconds));
            uartUsb.write(str, strlen(str));
            break;
        }

        case 'e':
        case 'E':
            printEventLog();
            break;

        default:
            availableCommands();
            break;
        }
    }
}

void availableCommands()
{
    uartUsb.write("Available commands:\r\n", 21);
    uartUsb.write("Press 's' or 'S' to set the date and time\r\n", 43);
    uartUsb.write("Press 't' or 'T' to get the date and time\r\n", 43);
    uartUsb.write("Press 'e' or 'E' to print the event log\r\n", 40);
    uartUsb.write("Use keypad digits + # to enter the alarm code\r\n", 45);
    uartUsb.write("Press keypad # while alarm is OFF to show event log\r\n\r\n", 52);
}

bool isNumericKey(char key)
{
    return (key >= '0' && key <= '9');
}

bool enteredCodeIsCorrect()
{
    for (int i = 0; i < NUMBER_OF_KEYS; i++) {
        if (enteredCode[i] != codeSequence[i]) {
            return false;
        }
    }

    return true;
}

void storeEvent(const char* cause)
{
    eventLog[eventWriteIndex].seconds = time(NULL);
    eventLog[eventWriteIndex].temperatureC = lm35TempC;
    eventLog[eventWriteIndex].gasPpm = gasPpm;

    strcpy(eventLog[eventWriteIndex].cause, cause);

    eventWriteIndex++;
    if (eventWriteIndex >= EVENT_MAX_STORAGE) {
        eventWriteIndex = 0;
    }

    if (storedEvents < EVENT_MAX_STORAGE) {
        storedEvents++;
    }
}

void printEventLog()
{
    char str[220];

    uartUsb.write("\r\n===== LAST 5 EVENTS =====\r\n", 29);

    if (storedEvents == 0) {
        uartUsb.write("No events stored\r\n\r\n", 19);
        return;
    }

    for (int i = 0; i < storedEvents; i++) {
        int index = eventWriteIndex - 1 - i;

        if (index < 0) {
            index += EVENT_MAX_STORAGE;
        }

        sprintf(
            str,
            "Event %d | Cause: %s | Temp: %.2f C | Gas: %.1f ppm\r\n",
            i + 1,
            eventLog[index].cause,
            eventLog[index].temperatureC,
            eventLog[index].gasPpm
        );
        uartUsb.write(str, strlen(str));

        sprintf(str, "Time: %s\r\n", ctime(&eventLog[index].seconds));
        uartUsb.write(str, strlen(str));
    }

    uartUsb.write("=========================\r\n\r\n", 29);
}

void matrixKeypadInit()
{
    matrixKeypadState = MATRIX_KEYPAD_SCANNING;

    for (int pinIndex = 0; pinIndex < KEYPAD_NUMBER_OF_COLS; pinIndex++) {
        keypadColPins[pinIndex].mode(PullUp);
    }
}

char matrixKeypadScan()
{
    for (int row = 0; row < KEYPAD_NUMBER_OF_ROWS; row++) {

        for (int i = 0; i < KEYPAD_NUMBER_OF_ROWS; i++) {
            keypadRowPins[i] = ON;
        }

        keypadRowPins[row] = OFF;

        for (int col = 0; col < KEYPAD_NUMBER_OF_COLS; col++) {
            if (keypadColPins[col] == OFF) {
                return matrixKeypadIndexToCharArray[
                    row * KEYPAD_NUMBER_OF_COLS + col
                ];
            }
        }
    }

    return '\0';
}

char matrixKeypadUpdate()
{
    char keyDetected = '\0';
    char keyReleased = '\0';

    switch (matrixKeypadState) {

    case MATRIX_KEYPAD_SCANNING:
        keyDetected = matrixKeypadScan();
        if (keyDetected != '\0') {
            matrixKeypadLastKeyPressed = keyDetected;
            accumulatedDebounceMatrixKeypadTime = 0;
            matrixKeypadState = MATRIX_KEYPAD_DEBOUNCE;
        }
        break;

    case MATRIX_KEYPAD_DEBOUNCE:
        if (accumulatedDebounceMatrixKeypadTime >= DEBOUNCE_KEY_TIME_MS) {
            keyDetected = matrixKeypadScan();

            if (keyDetected == matrixKeypadLastKeyPressed) {
                matrixKeypadState = MATRIX_KEYPAD_KEY_HOLD_PRESSED;
            } else {
                matrixKeypadState = MATRIX_KEYPAD_SCANNING;
            }
        }

        accumulatedDebounceMatrixKeypadTime += TIME_INCREMENT_MS;
        break;

    case MATRIX_KEYPAD_KEY_HOLD_PRESSED:
        keyDetected = matrixKeypadScan();

        if (keyDetected != matrixKeypadLastKeyPressed) {
            if (keyDetected == '\0') {
                keyReleased = matrixKeypadLastKeyPressed;
            }
            matrixKeypadState = MATRIX_KEYPAD_SCANNING;
        }
        break;

    default:
        matrixKeypadInit();
        break;
    }

    return keyReleased;
}