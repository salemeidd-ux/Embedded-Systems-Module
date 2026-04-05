//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "user_interface.h"

#include "code.h"
#include "siren.h"
#include "smart_home_system.h"
#include "fire_alarm.h"
#include "date_and_time.h"
#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "matrix_keypad.h"
#include "display.h"

//=====[Declaration of private defines]========================================

#define DISPLAY_REFRESH_TIME_MS       1000
#define ALARM_LCD_REFRESH_TIME_MS    60000
#define STATUS_MESSAGE_TIME_MS        3000

//=====[Declaration of private data types]=====================================

typedef enum {
    LCD_STATUS_NONE,
    LCD_STATUS_GAS,
    LCD_STATUS_TEMP,
} lcdStatusMessage_t;

//=====[Declaration and initialization of public global objects]===============

DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

//=====[Declaration of external public global variables]=======================

//=====[Declaration and initialization of public global variables]=============

char codeSequenceFromUserInterface[CODE_NUMBER_OF_KEYS];

//=====[Declaration and initialization of private global variables]============

static bool incorrectCodeState = OFF;
static bool systemBlockedState = OFF;

static bool codeComplete = false;
static int numberOfCodeChars = 0;
static lcdStatusMessage_t lcdStatusMessage = LCD_STATUS_NONE;
static int accumulatedStatusMessageTime = 0;

//=====[Declarations (prototypes) of private functions]========================

static void userInterfaceMatrixKeypadUpdate();
static void incorrectCodeIndicatorUpdate();
static void systemBlockedIndicatorUpdate();

static void userInterfaceDisplayInit();
static void userInterfaceDisplayUpdate();

//=====[Implementations of public functions]===================================

void userInterfaceInit()
{
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
    matrixKeypadInit( SYSTEM_TIME_INCREMENT_MS );
    userInterfaceDisplayInit();
}

void userInterfaceUpdate()
{
    userInterfaceMatrixKeypadUpdate();
    incorrectCodeIndicatorUpdate();
    systemBlockedIndicatorUpdate();
    userInterfaceDisplayUpdate();
}

bool incorrectCodeStateRead()
{
    return incorrectCodeState;
}

void incorrectCodeStateWrite( bool state )
{
    incorrectCodeState = state;
}

bool systemBlockedStateRead()
{
    return systemBlockedState;
}

void systemBlockedStateWrite( bool state )
{
    systemBlockedState = state;
}

bool userInterfaceCodeCompleteRead()
{
    return codeComplete;
}

void userInterfaceCodeCompleteWrite( bool state )
{
    codeComplete = state;
}

//=====[Implementations of private functions]==================================

static void userInterfaceMatrixKeypadUpdate()
{
    char keyReleased = matrixKeypadUpdate();

    if( keyReleased == '\0' ) {
        return;
    }

    if( !sirenStateRead() ) {
        if( keyReleased == '4' ) {
            lcdStatusMessage = LCD_STATUS_GAS;
            accumulatedStatusMessageTime = 0;
        } else if( keyReleased == '5' ) {
            lcdStatusMessage = LCD_STATUS_TEMP;
            accumulatedStatusMessageTime = 0;
        }
        return;
    }

    if( systemBlockedStateRead() ) {
        return;
    }

    if( keyReleased >= '0' && keyReleased <= '9' ) {
        incorrectCodeStateWrite(OFF);

        if ( numberOfCodeChars < CODE_NUMBER_OF_KEYS ) {
            codeSequenceFromUserInterface[numberOfCodeChars] = keyReleased;
            numberOfCodeChars++;
        }
    } else if( keyReleased == '*' ) {
        numberOfCodeChars = 0;
        codeComplete = false;
        incorrectCodeStateWrite(OFF);
    } else if( keyReleased == '#' ) {
        if ( numberOfCodeChars == CODE_NUMBER_OF_KEYS ) {
            codeComplete = true;
        } else {
            incorrectCodeStateWrite(ON);
        }
        numberOfCodeChars = 0;
    }
}

static void userInterfaceDisplayInit()
{
    displayInit( DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER );

    displayCharPositionWrite ( 0,0 );
    displayStringWrite( "Temperature:        " );

    displayCharPositionWrite ( 0,1 );
    displayStringWrite( "Status:             " );

    displayCharPositionWrite ( 0,2 );
    displayStringWrite( "Alarm: OFF          " );

    displayCharPositionWrite ( 0,3 );
    displayStringWrite( "Enter 5 digits + #  " );
}

static void userInterfaceDisplayUpdate()
{
    static int accumulatedDisplayTime = 0;
    static int accumulatedAlarmDisplayTime = ALARM_LCD_REFRESH_TIME_MS;
    static bool lastAlarmState = OFF;

    char lineBuffer[21] = "";

    if( lcdStatusMessage != LCD_STATUS_NONE ) {
        accumulatedStatusMessageTime =
            accumulatedStatusMessageTime + SYSTEM_TIME_INCREMENT_MS;

        if ( accumulatedStatusMessageTime >= STATUS_MESSAGE_TIME_MS ) {
            lcdStatusMessage = LCD_STATUS_NONE;
            accumulatedStatusMessageTime = 0;
        }
    }

    if( accumulatedDisplayTime >= DISPLAY_REFRESH_TIME_MS ) {
        accumulatedDisplayTime = 0;

        sprintf( lineBuffer, "Temp: %4.1f C       ",
                 temperatureSensorReadCelsius() );
        displayCharPositionWrite ( 0,0 );
        displayStringWrite( lineBuffer );

        displayCharPositionWrite ( 0,1 );
        if ( overTemperatureDetectorStateRead() && gasDetectorStateRead() ) {
            displayStringWrite( "WARNING: TEMP+GAS   " );
        } else if ( overTemperatureDetectorStateRead() ) {
            displayStringWrite( "WARNING: HIGH TEMP  " );
        } else if ( gasDetectorStateRead() ) {
            displayStringWrite( "WARNING: GAS        " );
        } else if ( lcdStatusMessage == LCD_STATUS_GAS ) {
            if ( gasDetectorStateRead() ) {
                displayStringWrite( "Gas state: DETECTED " );
            } else {
                displayStringWrite( "Gas state: SAFE     " );
            }
        } else if ( lcdStatusMessage == LCD_STATUS_TEMP ) {
            if ( overTemperatureDetectorStateRead() ) {
                displayStringWrite( "Temp state: HIGH    " );
            } else {
                displayStringWrite( "Temp state: SAFE    " );
            }
        } else {
            displayStringWrite( "Press 4:G  5:T      " );
        }

        displayCharPositionWrite ( 0,3 );
        if ( systemBlockedStateRead() ) {
            displayStringWrite( "System blocked      " );
        } else if ( incorrectCodeStateRead() ) {
            displayStringWrite( "Wrong code, retry   " );
        } else {
            displayStringWrite( "Enter 5 digits + #  " );
        }

    } else {
        accumulatedDisplayTime =
            accumulatedDisplayTime + SYSTEM_TIME_INCREMENT_MS;
    }

    if( ( accumulatedAlarmDisplayTime >= ALARM_LCD_REFRESH_TIME_MS ) ||
        ( lastAlarmState != sirenStateRead() ) ) {

        accumulatedAlarmDisplayTime = 0;
        lastAlarmState = sirenStateRead();
        displayCharPositionWrite ( 0,2 );

        if ( sirenStateRead() ) {
            displayStringWrite( "Alarm: ON           " );
        } else {
            displayStringWrite( "Alarm: OFF          " );
        }
    } else {
        accumulatedAlarmDisplayTime =
            accumulatedAlarmDisplayTime + SYSTEM_TIME_INCREMENT_MS;
    }
}

static void incorrectCodeIndicatorUpdate()
{
    incorrectCodeLed = incorrectCodeStateRead();
}

static void systemBlockedIndicatorUpdate()
{
    systemBlockedLed = systemBlockedState;
}
