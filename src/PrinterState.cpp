#include <string.h>

#include "PrinterState.h"

PrinterState printerState;

PrinterStatus stateToStatus(const char* state) {
    if (strcmp(state, "printing") == 0)  return PrinterStatus::Printing;
    if (strcmp(state, "paused") == 0)    return PrinterStatus::Paused;
    if (strcmp(state, "complete") == 0)  return PrinterStatus::Complete;
    if (strcmp(state, "cancelled") == 0) return PrinterStatus::Error;
    if (strcmp(state, "error") == 0)     return PrinterStatus::Error;
    if (strcmp(state, "standby") == 0)   return PrinterStatus::Standby;

    return PrinterStatus::Offline;
}
