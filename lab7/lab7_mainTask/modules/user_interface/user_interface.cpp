//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "user_interface.h"

#include "code.h"
#include "siren.h"
#include "smart_home_system.h"
#include "fire_alarm.h"
#include "temperature_sensor.h"
#include "matrix_keypad.h"
#include "display.h"
#include "gate.h"
#include "pc_serial_com.h"

//=====[Declaration of private defines]========================================

#define DISPLAY_REFRESH_TIME_MS       250
#define ALARM_LCD_REFRESH_TIME_MS   60000
#define STATUS_MESSAGE_TIME_MS       3000
#define LCD_LINE_LENGTH               20

//=====[Declaration and initialization of public global objects]===============

DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);
InterruptIn pirMotionSensor(PG_0);

//=====[Declaration and initialization of public global variables]=============

char codeSequenceFromUserInterface[CODE_NUMBER_OF_KEYS];

//=====[Declaration and initialization of private global variables]============

static bool incorrectCodeState = OFF;
static bool systemBlockedState = OFF;
static bool codeComplete = false;
static int numberOfCodeChars = 0;

static char transientMessage[LCD_LINE_LENGTH + 1] = "System ready";
static int transientMessageTimeMs = STATUS_MESSAGE_TIME_MS;
static int alarmStateReportTimeMs = 0;

//=====[Declarations (prototypes) of private functions]========================

static void userInterfaceMatrixKeypadUpdate();
static void incorrectCodeIndicatorUpdate();
static void systemBlockedIndicatorUpdate();

static void userInterfaceDisplayInit();
static void userInterfaceDisplayUpdate();
static void displayWriteLine( uint8_t row, const char* text );
static void setTransientMessage( const char* text );
static void buildCodeEntryLine( char* buffer );

static void pirMotionCallback();

//=====[Implementations of public functions]===================================

void userInterfaceInit()
{
    pirMotionSensor.mode( PullDown );
    pirMotionSensor.rise( &pirMotionCallback );

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

    if ( transientMessageTimeMs > 0 ) {
        transientMessageTimeMs -= SYSTEM_TIME_INCREMENT_MS;
        if ( transientMessageTimeMs < 0 ) {
            transientMessageTimeMs = 0;
        }
    }

    alarmStateReportTimeMs += SYSTEM_TIME_INCREMENT_MS;
    if ( alarmStateReportTimeMs >= ALARM_LCD_REFRESH_TIME_MS ) {
        if ( sirenStateRead() ) {
            setTransientMessage( "Alarm state: ON" );
        } else {
            setTransientMessage( "Alarm state: OFF" );
        }
        alarmStateReportTimeMs = 0;
    }
}

bool incorrectCodeStateRead()
{
    return incorrectCodeState;
}

void incorrectCodeStateWrite( bool state )
{
    incorrectCodeState = state;
    if ( state ) {
        setTransientMessage( "Wrong code" );
    }
}

bool systemBlockedStateRead()
{
    return systemBlockedState;
}

void systemBlockedStateWrite( bool state )
{
    systemBlockedState = state;
    if ( state ) {
        setTransientMessage( "System blocked" );
    }
}

bool userInterfaceCodeCompleteRead()
{
    return codeComplete;
}

void userInterfaceCodeCompleteWrite( bool state )
{
    codeComplete = state;
    if ( !state ) {
        numberOfCodeChars = 0;
    }
}

//=====[Implementations of private functions]==================================

static void userInterfaceMatrixKeypadUpdate()
{
    char keyReleased = matrixKeypadUpdate();

    if ( keyReleased == '\0' ) {
        return;
    }

    if ( sirenStateRead() ) {
        if ( systemBlockedStateRead() ) {
            return;
        }

        if ( keyReleased >= '0' && keyReleased <= '9' ) {
            incorrectCodeStateWrite( OFF );
            if ( numberOfCodeChars < CODE_NUMBER_OF_KEYS ) {
                codeSequenceFromUserInterface[numberOfCodeChars] = keyReleased;
                numberOfCodeChars++;
                if ( numberOfCodeChars >= CODE_NUMBER_OF_KEYS ) {
                    codeComplete = true;
                }
            }
        } else if ( keyReleased == '*' ) {
            numberOfCodeChars = 0;
            codeComplete = false;
            incorrectCodeStateWrite( OFF );
            setTransientMessage( "Code cleared" );
        }
        return;
    }

    switch ( keyReleased ) {
        case '1':
            gateOpen();
        break;

        case '2':
            gateClose();
        break;

        case '3':
            gateStop();
            setTransientMessage( "KEYPAD -> STOP" );
            pcSerialComStringWrite( "[Gate] KEYPAD -> STOP\r\n" );
        break;

        case '4':
            if ( gasDetectorStateRead() ) {
                setTransientMessage( "Gas state: WARN" );
                pcSerialComStringWrite( "[LCD] Gas state: WARN\r\n" );
            } else {
                setTransientMessage( "Gas state: SAFE" );
                pcSerialComStringWrite( "[LCD] Gas state: SAFE\r\n" );
            }
        break;

        case '5':
            if ( overTemperatureDetectorStateRead() ) {
                setTransientMessage( "Temp state: WARN" );
                pcSerialComStringWrite( "[LCD] Temp state: WARN\r\n" );
            } else {
                setTransientMessage( "Temp state: SAFE" );
                pcSerialComStringWrite( "[LCD] Temp state: SAFE\r\n" );
            }
        break;

        case '6':
            gateToggleMode();
        break;

        default:
        break;
    }
}

static void userInterfaceDisplayInit()
{
    displayInit( DISPLAY_TYPE_LCD_HD44780,
                 DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER );
    displayClear();
}

static void userInterfaceDisplayUpdate()
{
    static int accumulatedDisplayTime = 0;
    char line[LCD_LINE_LENGTH + 1];
    char codeLine[LCD_LINE_LENGTH + 1];

    if ( accumulatedDisplayTime < DISPLAY_REFRESH_TIME_MS ) {
        accumulatedDisplayTime += SYSTEM_TIME_INCREMENT_MS;
        return;
    }

    accumulatedDisplayTime = 0;

    snprintf( line, sizeof(line), "T:%2.0fC G:%s",
              temperatureSensorReadCelsius(),
              gasDetectorStateRead() ? "WARN" : "SAFE" );
    displayWriteLine( 0, line );

    snprintf( line, sizeof(line), "Motor:%s", gateStatusString() );
    displayWriteLine( 1, line );

    snprintf( line, sizeof(line), "Alarm:%s Mode:%s",
              sirenStateRead() ? "ON" : "OFF",
              gateModeString() );
    displayWriteLine( 2, line );

    if ( sirenStateRead() ) {
        if ( systemBlockedStateRead() ) {
            displayWriteLine( 3, "SYSTEM BLOCKED" );
        } else if ( incorrectCodeStateRead() ) {
            displayWriteLine( 3, "Wrong code (* clr)" );
        } else if ( numberOfCodeChars > 0 || codeComplete ) {
            buildCodeEntryLine( codeLine );
            displayWriteLine( 3, codeLine );
        } else {
            displayWriteLine( 3, "Enter 5-digit code" );
        }
    } else if ( gasDetectorStateRead() && overTemperatureDetectorStateRead() ) {
        displayWriteLine( 3, "WARNING: TEMP+GAS" );
    } else if ( gasDetectorStateRead() ) {
        displayWriteLine( 3, "WARNING: GAS" );
    } else if ( overTemperatureDetectorStateRead() ) {
        displayWriteLine( 3, "WARNING: HIGH TEMP" );
    } else if ( transientMessageTimeMs > 0 ) {
        displayWriteLine( 3, transientMessage );
    } else {
        displayWriteLine( 3, gateLastEventString() );
    }
}

static void displayWriteLine( uint8_t row, const char* text )
{
    char paddedLine[LCD_LINE_LENGTH + 1];
    snprintf( paddedLine, sizeof(paddedLine), "%-20.20s", text );
    displayCharPositionWrite( 0, row );
    displayStringWrite( paddedLine );
}

static void setTransientMessage( const char* text )
{
    snprintf( transientMessage, sizeof(transientMessage), "%s", text );
    transientMessageTimeMs = STATUS_MESSAGE_TIME_MS;
}

static void buildCodeEntryLine( char* buffer )
{
    int i = 0;
    strcpy( buffer, "Code: " );
    for ( i = 0; i < CODE_NUMBER_OF_KEYS; i++ ) {
        if ( i < numberOfCodeChars ) {
            buffer[6 + i] = codeSequenceFromUserInterface[i];
        } else {
            buffer[6 + i] = '_';
        }
    }
    buffer[6 + CODE_NUMBER_OF_KEYS] = '\0';
}

static void incorrectCodeIndicatorUpdate()
{
    incorrectCodeLed = incorrectCodeStateRead();
}

static void systemBlockedIndicatorUpdate()
{
    systemBlockedLed = systemBlockedStateRead();
}

static void pirMotionCallback()
{
    gateHandlePirInterrupt();
}
