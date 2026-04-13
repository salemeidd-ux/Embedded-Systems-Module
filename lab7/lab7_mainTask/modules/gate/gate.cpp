//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "gate.h"
#include "motor.h"
#include "pc_serial_com.h"
#include "smart_home_system.h"

//=====[Declaration of private defines]========================================

#define GATE_TRAVEL_TIME_MS      3000
#define AUTO_CLOSE_DELAY_MS      5000

//=====[Declaration and initialization of private global variables]============

static gateStatus_t gateStatus;
static gateMode_t gateMode;
static volatile bool pirInterruptPending = false;
static char gateLastEvent[21] = "System ready";

//=====[Declarations (prototypes) of private functions]========================

static void gateEventWrite( const char* eventText );

//=====[Implementations of public functions]===================================

void gateInit()
{
    gateStatus = GATE_CLOSED;
    gateMode = GATE_MODE_MANUAL;
    pirInterruptPending = false;
    gateEventWrite( "Gate closed" );
}

void gateUpdate()
{
    static int motionTimeMs = 0;
    static int openHoldTimeMs = 0;

    if ( pirInterruptPending ) {
        core_util_critical_section_enter();
        pirInterruptPending = false;
        core_util_critical_section_exit();

        gateStop();
        gateEventWrite( "INT:PIR -> STOP" );
    }

    switch ( gateStatus ) {
        case GATE_OPENING:
            motionTimeMs += SYSTEM_TIME_INCREMENT_MS;
            if ( motionTimeMs >= GATE_TRAVEL_TIME_MS ) {
                motorDirectionWrite( STOPPED );
                gateStatus = GATE_OPEN;
                motionTimeMs = 0;
                openHoldTimeMs = 0;
                gateEventWrite( "Gate open" );
            }
        break;

        case GATE_CLOSING:
            motionTimeMs += SYSTEM_TIME_INCREMENT_MS;
            if ( motionTimeMs >= GATE_TRAVEL_TIME_MS ) {
                motorDirectionWrite( STOPPED );
                gateStatus = GATE_CLOSED;
                motionTimeMs = 0;
                gateEventWrite( "Gate closed" );
            }
        break;

        case GATE_OPEN:
            if ( gateMode == GATE_MODE_AUTO_CLOSE ) {
                openHoldTimeMs += SYSTEM_TIME_INCREMENT_MS;
                if ( openHoldTimeMs >= AUTO_CLOSE_DELAY_MS ) {
                    openHoldTimeMs = 0;
                    gateClose();
                    gateEventWrite( "AUTO -> CLOSE" );
                }
            }
        break;

        case GATE_STOPPED:
        case GATE_CLOSED:
        default:
            motionTimeMs = 0;
            openHoldTimeMs = 0;
        break;
    }
}

void gateOpen()
{
    motorDirectionWrite( DIRECTION_1 );
    gateStatus = GATE_OPENING;
    gateEventWrite( "KEYPAD -> OPEN" );
}

void gateClose()
{
    motorDirectionWrite( DIRECTION_2 );
    gateStatus = GATE_CLOSING;
    gateEventWrite( "KEYPAD -> CLOSE" );
}

void gateStop()
{
    motorDirectionWrite( STOPPED );
    gateStatus = GATE_STOPPED;
    gateEventWrite( "Motor stopped" );
}

void gateToggleMode()
{
    if ( gateMode == GATE_MODE_MANUAL ) {
        gateMode = GATE_MODE_AUTO_CLOSE;
        gateEventWrite( "Mode: AUTO_CLOSE" );
    } else {
        gateMode = GATE_MODE_MANUAL;
        gateEventWrite( "Mode: MANUAL" );
    }
}

void gateHandlePirInterrupt()
{
    pirInterruptPending = true;
}

bool gatePirPendingRead()
{
    return pirInterruptPending;
}

gateStatus_t gateStatusRead()
{
    return gateStatus;
}

gateMode_t gateModeRead()
{
    return gateMode;
}

const char* gateStatusString()
{
    switch ( gateStatus ) {
        case GATE_CLOSED:  return "CLOSED";
        case GATE_OPEN:    return "OPEN";
        case GATE_OPENING: return "OPENING";
        case GATE_CLOSING: return "CLOSING";
        case GATE_STOPPED: return "STOPPED";
        default:           return "UNKNOWN";
    }
}

const char* gateModeString()
{
    switch ( gateMode ) {
        case GATE_MODE_MANUAL:     return "MANUAL";
        case GATE_MODE_AUTO_CLOSE: return "AUTO";
        default:                   return "?";
    }
}

const char* gateLastEventString()
{
    return gateLastEvent;
}

//=====[Implementations of private functions]==================================

static void gateEventWrite( const char* eventText )
{
    snprintf( gateLastEvent, sizeof(gateLastEvent), "%s", eventText );
    pcSerialComStringWrite( "[Gate] " );
    pcSerialComStringWrite( gateLastEvent );
    pcSerialComStringWrite( "\r\n" );
}
