//=====[Libraries]=============================================================

#include "arm_book_lib.h"

#include "smart_home_system.h"

#include "user_interface.h"
#include "fire_alarm.h"
#include "pc_serial_com.h"
#include "event_log.h"
#include "motor.h"
#include "gate.h"

//=====[Implementations of public functions]===================================

void smartHomeSystemInit()
{
    userInterfaceInit();
    fireAlarmInit();
    pcSerialComInit();
    motorControlInit();
    gateInit();
}

void smartHomeSystemUpdate()
{
    userInterfaceUpdate();
    fireAlarmUpdate();
    pcSerialComUpdate();
    eventLogUpdate();
    gateUpdate();
    motorControlUpdate();
    delay( SYSTEM_TIME_INCREMENT_MS );
}
