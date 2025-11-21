#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include "EcoflowData.h"
#include "DeviceManager.h"

enum class DisplayAction {
    NONE,
    TOGGLE_AC,
    TOGGLE_DC,
    TOGGLE_USB,
    SET_AC_LIMIT,
    SET_SOC_LIMITS,
    CONNECT_DEVICE,
    DISCONNECT_DEVICE
};

enum class ButtonInput {
    BTN_UP,
    BTN_DOWN,
    BTN_ENTER_SHORT,
    BTN_ENTER_MEDIUM, // 1s (Back)
    BTN_ENTER_LONG    // 3s (Confirm/Action)
};

void setupDisplay();
void updateDisplay(const EcoflowData& currentData, DeviceSlot* activeSlot, bool isScanning);
DisplayAction handleDisplayInput(ButtonInput input);
DisplayAction getPendingAction();

// Accessor for the new set value
int getSetAcLimit();
int getSetMaxChgSoc();
int getSetMinDsgSoc();
DeviceType getTargetDeviceType();

#endif
