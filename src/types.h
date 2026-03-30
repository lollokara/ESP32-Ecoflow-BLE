#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

enum class DeviceType {
    DELTA_3,
    WAVE_2,
    DELTA_PRO_3,
    ALTERNATOR_CHARGER
};

enum class ButtonInput {
    NONE,
    BTN_UP,
    BTN_DOWN,
    BTN_ENTER_SHORT,
    BTN_ENTER_MEDIUM,
    BTN_ENTER_LONG
};

enum class DisplayAction {
    NONE,
    CONNECT_DEVICE,
    DISCONNECT_DEVICE,
    TOGGLE_AC,
    TOGGLE_DC,
    TOGGLE_USB,
    SET_AC_LIMIT,
    SET_SOC_LIMITS,
    // Add more as needed
};

#endif // TYPES_H
