/**
 * @file main.cpp
 * @author Lollokara
 * @brief Main application file for the ESP32 EcoFlow Controller.
 *
 * This file contains the primary application logic, including:
 * - Initialization of hardware (Serial, Display, Buttons).
 * - Management of the user interface, including button handling and display updates.
 * - Coordination of the DeviceManager to handle BLE connections and data.
 */

#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "Display.h"
#include "DeviceManager.h"

// Define GPIO pins for the buttons
#define BTN_UP_PIN    4
#define BTN_DOWN_PIN  5
#define BTN_ENTER_PIN 6

// Timings for button press detection (in milliseconds)
#define DEBOUNCE_DELAY 50
#define MEDIUM_PRESS_TIME 1000
#define LONG_PRESS_TIME 3000

/**
 * @class Button
 * @brief A simple class to handle button inputs with debouncing and multiple press types.
 *
 * This class detects short, medium (1s), and long (3s) presses for a single button.
 */
class Button {
    int pin;
    int state;
    int lastReading;
    unsigned long lastDebounceTime;
    unsigned long pressedTime;
    bool isPressedState;
    bool mediumPressHandled;
    bool longPressHandled;

public:
    Button(int p) : pin(p), state(HIGH), lastReading(HIGH), lastDebounceTime(0),
                    pressedTime(0), isPressedState(false), mediumPressHandled(false), longPressHandled(false) {}
    
    /**
     * @brief Checks the button state and returns the type of press event.
     * @return 0 for no event, 1 for a short press, 2 for a medium press, 3 for a long press.
     */
    int check() {
        int reading = digitalRead(pin);
        int event = 0;

        if (reading != lastReading) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (reading != state) {
                state = reading;
                if (state == LOW) { // Button pressed
                    pressedTime = millis();
                    isPressedState = true;
                    mediumPressHandled = false;
                    longPressHandled = false;
                } else { // Button released
                    if (isPressedState) {
                        isPressedState = false;
                        if (!longPressHandled && !mediumPressHandled) {
                             event = 1; // Short press
                        }
                    }
                }
            }
        }

        // Check for medium and long presses while the button is held down
        if (isPressedState) {
            unsigned long duration = millis() - pressedTime;
            if (!longPressHandled && duration >= LONG_PRESS_TIME) {
                event = 3; // Long press
                longPressHandled = true;
            } else if (!mediumPressHandled && duration >= MEDIUM_PRESS_TIME) {
                event = 2; // Medium press
                mediumPressHandled = true;
            }
        }

        lastReading = reading;
        return event;
    }
};

Button btnUp(BTN_UP_PIN);
Button btnDown(BTN_DOWN_PIN);
Button btnEnter(BTN_ENTER_PIN);

DeviceType currentViewDevice = DeviceType::DELTA_3;

void handleAction(DisplayAction action);

/**
 * @brief Standard Arduino setup function. Initializes all components.
 */
void setup() {
    Serial.begin(115200);
    Serial.println("Starting Ecoflow Controller...");

    setupDisplay();
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_ENTER_PIN, INPUT_PULLUP);
    DeviceManager::getInstance().initialize();
}

/**
 * @brief Standard Arduino loop function. Runs the main application logic.
 */
void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_data_refresh = 0;

    // 1. Update the Device Manager (handles all BLE communication)
    DeviceManager::getInstance().update();

    // 2. Check for button inputs and translate them to display actions
    DisplayAction action = DisplayAction::NONE;
    int upEvent = btnUp.check();
    int downEvent = btnDown.check();
    int enterEvent = btnEnter.check();

    if (upEvent == 1) action = handleDisplayInput(ButtonInput::BTN_UP);
    if (downEvent == 1) action = handleDisplayInput(ButtonInput::BTN_DOWN);
    if (enterEvent == 1) action = handleDisplayInput(ButtonInput::BTN_ENTER_SHORT);
    if (enterEvent == 2) action = handleDisplayInput(ButtonInput::BTN_ENTER_MEDIUM);
    if (enterEvent == 3) action = handleDisplayInput(ButtonInput::BTN_ENTER_LONG);

    // 3. Process the action from the user input
    handleAction(action);

    // 4. Process any pending action that might have been triggered by a timeout in the UI
    handleAction(getPendingAction());

    // 5. Periodically request fresh data from all connected devices
    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();
        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3->isAuthenticated()) d3->requestData();
        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2->isAuthenticated()) w2->requestData();
    }

    // 6. Update the display at a regular framerate
    if (millis() - last_display_update > 20) {
        last_display_update = millis();
        EcoflowESP32* activeDev = DeviceManager::getInstance().getDevice(currentViewDevice);
        DeviceSlot* activeSlot = DeviceManager::getInstance().getSlot(currentViewDevice);
        bool scanning = DeviceManager::getInstance().isScanning();
        updateDisplay(activeDev ? activeDev->_data : EcoflowData{}, activeSlot, scanning);
    }
}

/**
 * @brief Executes actions based on user input from the display.
 * @param action The action to be performed.
 */
void handleAction(DisplayAction action) {
    if (action == DisplayAction::NONE) return;

    // Handle device management actions first
    if (action == DisplayAction::CONNECT_DEVICE) {
        DeviceType target = getTargetDeviceType();
        DeviceManager::getInstance().scanAndConnect(target);
        currentViewDevice = target;
        return;
    }
    if (action == DisplayAction::DISCONNECT_DEVICE) {
        DeviceManager::getInstance().disconnect(getTargetDeviceType());
        return;
    }

    // For control actions, ensure the device is authenticated
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
    if (dev && dev->isAuthenticated()) {
        switch(action) {
            case DisplayAction::TOGGLE_AC:
                dev->setAC(!dev->isAcOn());
                break;
            case DisplayAction::TOGGLE_DC:
                dev->setDC(!dev->isDcOn());
                break;
            case DisplayAction::TOGGLE_USB:
                dev->setUSB(!dev->isUsbOn());
                break;
            case DisplayAction::SET_AC_LIMIT:
                dev->setAcChargingLimit(getSetAcLimit());
                break;
            case DisplayAction::SET_SOC_LIMITS:
                dev->setBatterySOCLimits(getSetMaxChgSoc(), getSetMinDsgSoc());
                break;
            default: break;
        }
    } else {
        ESP_LOGI("main", "Action ignored: Device not authenticated");
    }
}
