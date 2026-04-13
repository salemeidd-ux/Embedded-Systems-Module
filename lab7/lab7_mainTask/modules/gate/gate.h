//=====[#include guards - begin]===============================================

#ifndef _GATE_H_
#define _GATE_H_

//=====[Declaration of public data types]======================================

typedef enum {
    GATE_CLOSED,
    GATE_OPEN,
    GATE_OPENING,
    GATE_CLOSING,
    GATE_STOPPED
} gateStatus_t;

typedef enum {
    GATE_MODE_MANUAL,
    GATE_MODE_AUTO_CLOSE
} gateMode_t;

//=====[Declarations (prototypes) of public functions]=========================

void gateInit();
void gateUpdate();

void gateOpen();
void gateClose();
void gateStop();
void gateToggleMode();

void gateHandlePirInterrupt();

bool gatePirPendingRead();

gateStatus_t gateStatusRead();
gateMode_t gateModeRead();

const char* gateStatusString();
const char* gateModeString();
const char* gateLastEventString();

//=====[#include guards - end]=================================================

#endif // _GATE_H_
