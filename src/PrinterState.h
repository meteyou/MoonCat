#pragma once

// Global printer status (switch between views)
enum class PrinterStatus { Standby, Printing, Paused, Complete, Error, Offline };

// Shared printer states
struct PrinterState {
    char          printerName[32]   = "MoonCat";
    bool          klippyReady       = false;
    PrinterStatus status            = PrinterStatus::Offline;
    float         progress          = 0.0f;
    float         printDuration     = 0.0f;
    float         extruderTemp      = 0.0f;
    float         extruderTarget    = 0.0f;
    float         bedTemp           = 0.0f;
    float         bedTarget         = 0.0f;
    bool          hasHeaterBed      = false;
};

extern PrinterState printerState;

// Moonraker-String -> Enum
PrinterStatus stateToStatus(const char* state);
