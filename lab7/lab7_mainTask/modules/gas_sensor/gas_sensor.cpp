//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "gas_sensor.h"

//=====[Declaration of private defines]========================================

#define MQ2_NUMBER_OF_AVG_SAMPLES   20
#define GAS_LIMIT_PPM               400.0f

//=====[Declaration and initialization of public global objects]===============

AnalogIn mq2(A2);

//=====[Declaration and initialization of private global variables]============

static float gasPpm = 0.0f;
static bool gasDetected = OFF;

//=====[Declarations (prototypes) of private functions]========================

static float readAnalogAverage( AnalogIn& input, int samples );

//=====[Implementations of public functions]===================================

void gasSensorInit()
{
    gasPpm = 0.0f;
    gasDetected = OFF;
}

void gasSensorUpdate()
{
    gasPpm = readAnalogAverage( mq2, MQ2_NUMBER_OF_AVG_SAMPLES ) * 800.0f;
    gasDetected = ( gasPpm > GAS_LIMIT_PPM );
}

bool gasSensorRead()
{
    return gasDetected;
}

//=====[Implementations of private functions]==================================

static float readAnalogAverage( AnalogIn& input, int samples )
{
    float sum = 0.0f;

    for ( int i = 0; i < samples; i++ ) {
        sum += input.read();
    }

    return ( sum / samples );
}
