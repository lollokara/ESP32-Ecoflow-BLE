#ifndef CMD_UTILS_H
#define CMD_UTILS_H

#include <Arduino.h>
#include <vector>
#include "EcoflowESP32.h"
#include "DeviceManager.h"

class CmdUtils {
public:
    static void processInput(String input);

private:
    static void printHelp();
    static uint8_t parseHexByte(String s);
    static void handleWave2Command(String cmd, String args);
    static void handleDelta3Command(String cmd, String args);
    static void handleDeltaPro3Command(String cmd, String args);
    static void handleAltChargerCommand(String cmd, String args);

    // Read Handlers
    static void handleWave2Read(String cmd);
    static void handleDelta3Read(String cmd);
    static void handleDeltaPro3Read(String cmd);
    static void handleAltChargerRead(String cmd);

    // System Commands
    static void handleSysCommand(String cmd);
    static void handleConCommand(String cmd, String args);

    static float parseFloat(String s);
};

#endif // CMD_UTILS_H
