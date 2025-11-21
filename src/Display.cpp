#include "Display.h"
#include "Font.h"
#include <math.h>

#define DATAPIN    14
#define CLOCKPIN   13
#define NUM_ROWS   9
#define NUM_COLS   20
#define NUMPIXELS  (NUM_ROWS * NUM_COLS)

Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);

// --- Global State ---
EcoflowData currentData;

enum class MenuState {
    DASHBOARD,
    SELECTION,
    DETAIL,
    SETTINGS_SUBMENU,
    LIMITS_SUBMENU,
    EDIT_CHG,
    EDIT_SOC_UP,
    EDIT_SOC_DN,
    DEVICE_SELECT,
    DEVICE_ACTION
};

enum class DashboardView {
    D3_BATT,
    D3_SOLAR,
    W2_BATT,
    W2_TEMP
};

enum class SelectionPage {
    AC,
    DC,
    USB,
    SOL,
    IN,
    SET,
    DEV
};

enum class SettingsPage {
    CHG,
    LIM
};

enum class LimitsPage {
    UP,
    DN
};

enum class DevicePage {
    D3,
    W2
};

enum class DeviceActionPage {
    CON,
    DIS,
    RET
};

MenuState currentState = MenuState::DASHBOARD;
DashboardView currentDashboardView = DashboardView::D3_BATT;
SelectionPage currentSelection = SelectionPage::AC;
SettingsPage currentSettingsPage = SettingsPage::CHG;
LimitsPage currentLimitsPage = LimitsPage::UP;
DevicePage currentDevicePage = DevicePage::D3;
DeviceActionPage currentDeviceAction = DeviceActionPage::CON;

unsigned long lastInteractionTime = 0;
unsigned long lastScrollTime = 0;
int scrollOffset = 0;
bool needScroll = false;

// For slide animation
bool isAnimating = false;
int animationStep = 0;
SelectionPage prevSelection = SelectionPage::AC;
int slideDirection = 0; // 1 = Up, -1 = Down

// Pending Action for Timeout
DisplayAction pendingAction = DisplayAction::NONE;
DeviceType targetDeviceType = DeviceType::DELTA_3;

// Temp variables for editing
int tempAcLimit = 400;
int tempMaxChg = 100;
int tempMinDsg = 0;

// --- Colors ---
uint32_t cRed, cYellow, cGreen, cWhite, cOff, cBlue;
uint32_t frameBuffer[9][20];

// --- Helper: Map (x,y) to strip index ---
uint16_t getPixelIndex(int x, int y) {
    if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS) return NUMPIXELS;
    uint16_t base = x * NUM_ROWS;
    if (x % 2 == 0) return base + y;
    else return base + (8 - y);
}

void setPixel(int x, int y, uint32_t color) {
    uint16_t index = getPixelIndex(x, y);
    if (index < NUMPIXELS) strip.setPixelColor(index, color);
}

void setupDisplay() {
    strip.begin();
    strip.setBrightness(25);
    strip.fill(0);
    strip.show();

    cRed = strip.Color(255, 0, 0);
    cYellow = strip.Color(255, 255, 0);
    cGreen = strip.Color(0, 255, 0);
    cWhite = strip.Color(255, 255, 255);
    cOff = strip.Color(0, 0, 0);
    cBlue = strip.Color(0, 0, 255);

    lastInteractionTime = millis();
}

void clearFrame() {
    for(int y=0; y<NUM_ROWS; y++) {
        for(int x=0; x<NUM_COLS; x++) {
            frameBuffer[y][x] = cOff;
        }
    }
}

void renderFrame() {
    strip.clear();
    for(int y=0; y<NUM_ROWS; y++) {
        for(int x=0; x<NUM_COLS; x++) {
            if(frameBuffer[y][x] != 0) setPixel(x, y, frameBuffer[y][x]);
        }
    }
    strip.show();
}

int drawChar(int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 95) c = '?';
    const uint8_t* bitmap = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        int dx = x + col;
        if (dx >= 0 && dx < NUM_COLS) {
            for (int row = 0; row < 7; row++) {
                int dy = y + row;
                if (dy >= 0 && dy < NUM_ROWS) {
                    if (bitmap[col] & (1 << row)) frameBuffer[dy][dx] = color;
                }
            }
        }
    }
    return 6;
}

void drawText(int x, int y, String text, uint32_t color) {
    int curX = x;
    for (int i = 0; i < text.length(); i++) {
        if (curX >= NUM_COLS) break;
        if (curX + 6 >= 0) drawChar(curX, y, text[i], color);
        curX += 6;
    }
}

int getTextWidth(String text) {
    return text.length() * 6;
}

void flashScreen(uint32_t color) {
    strip.fill(color);
    strip.show();
    delay(100); // Blocking delay for feedback
    strip.fill(0);
}

void drawNcScreen() {
    String text = "NC";
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, cRed);
}

void drawDashboard(DeviceSlot* slotD3, DeviceSlot* slotW2) {
    bool w2Connected = slotW2->isConnected;
    bool w2HasBatt = w2Connected && (slotW2->instance->getBatteryLevel() > 0);

    if (currentDashboardView == DashboardView::W2_BATT && !w2HasBatt) currentDashboardView = DashboardView::D3_BATT;
    if (currentDashboardView == DashboardView::W2_TEMP && !w2Connected) currentDashboardView = DashboardView::D3_BATT;
    if ((currentDashboardView == DashboardView::D3_BATT || currentDashboardView == DashboardView::D3_SOLAR) && !slotD3->isConnected) {
         strip.setBrightness(25);
         drawNcScreen();
         return;
    }

    uint32_t color = cGreen;
    String text = "";
    int brightness = 25;

    switch(currentDashboardView) {
        case DashboardView::D3_BATT:
        {
            int batt = slotD3->instance->getBatteryLevel();
            if (slotD3->instance->getInputPower() > 0) {
                float t = (float)millis() / 2000.0f;
                float val = (sin(t) + 1.0f) / 2.0f;
                int minB = 7;
                int maxB = 51;
                brightness = minB + (int)(val * (maxB - minB));
                color = cGreen;
            } else {
                if (batt < 20) color = cRed;
                else if (batt < 60) color = cYellow;
                else color = cGreen;
            }
            if (batt > 99) batt = 99;
            text = String(batt) + "%";
            break;
        }
        case DashboardView::D3_SOLAR:
        {
            int solar = slotD3->instance->getSolarInputPower();
            text = String(solar);
            color = cYellow;
            break;
        }
        case DashboardView::W2_BATT:
        {
             int batt = slotW2->instance->getBatteryLevel();
             if (batt > 99) batt = 99;
             text = String(batt) + "%";
             color = cBlue;
             break;
        }
        case DashboardView::W2_TEMP:
        {
            int temp = slotW2->instance->getAmbientTemperature();
            text = String(temp) + "C";
            color = cWhite;
            break;
        }
    }

    strip.setBrightness(brightness);
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

String getSelectionText(SelectionPage page) {
    switch(page) {
        case SelectionPage::AC: return "AC";
        case SelectionPage::DC: return "DC";
        case SelectionPage::USB: return "USB";
        case SelectionPage::SOL: return "SOL";
        case SelectionPage::IN: return "IN";
        case SelectionPage::SET: return "SET";
        case SelectionPage::DEV: return "DEV";
        default: return "?";
    }
}

void drawSelectionMenu() {
    if (isAnimating) {
        int step = animationStep;
        String oldText = getSelectionText(prevSelection);
        int oldWidth = getTextWidth(oldText) - 1;
        int oldX = (NUM_COLS - oldWidth) / 2;
        int oldY = (slideDirection == 1) ? (1 - step) : (1 + step);
        drawText(oldX, oldY, oldText, cWhite);

        String newText = getSelectionText(currentSelection);
        int newWidth = getTextWidth(newText) - 1;
        int newX = (NUM_COLS - newWidth) / 2;
        int newY = (slideDirection == 1) ? (1 + (9 - step)) : (1 - (9 - step));
        drawText(newX, newY, newText, cWhite);

        animationStep+=2;
        if (animationStep > 9) isAnimating = false;
    } else {
        String text = getSelectionText(currentSelection);
        int width = getTextWidth(text) - 1;
        int x = (NUM_COLS - width) / 2;
        drawText(x, 1, text, cWhite);
    }
}

void drawDetailMenu(DeviceType activeType) {
    String text = "";
    uint32_t color = cWhite;

    bool acOn = false;
    int acOut = 0;
    bool dcOn = false;
    int dcOut = 0;
    bool usbOn = false;
    int solIn = 0;
    int totIn = 0;

    switch (activeType) {
        case DeviceType::DELTA_3:
            // Default to Delta/River V3 logic
            acOn = currentData.delta3.acOn;
            acOut = abs((int)currentData.delta3.acOutputPower);
            dcOn = currentData.delta3.dcOn;
            dcOut = abs((int)currentData.delta3.dc12vOutputPower);
            usbOn = currentData.delta3.usbOn;
            solIn = (int)currentData.delta3.solarInputPower;
            totIn = (int)currentData.delta3.inputPower;
            break;

        case DeviceType::WAVE_2:
            // Default to Wave 2 logic
            acOn = (currentData.wave2.mode != 0);
            acOut = 0;
            dcOn = (currentData.wave2.powerMode != 0);
            dcOut = currentData.wave2.psdrPwrWatt;
            usbOn = false;
            solIn = currentData.wave2.mpptPwrWatt;
            totIn = (currentData.wave2.batPwrWatt > 0) ? currentData.wave2.batPwrWatt : 0;
            break;

        default:
            // Fallback for unknown devices
            acOn = false;
            acOut = 0;
            dcOn = false;
            dcOut = 0;
            usbOn = false;
            solIn = 0;
            totIn = 0;
            break;
    }

    switch(currentSelection) {
        case SelectionPage::AC:
            text = acOn ? String(acOut) + "W" : "OFF";
            color = acOn ? cGreen : cRed;
            break;
        case SelectionPage::DC:
            text = dcOn ? String(dcOut) + "W" : "OFF";
            color = dcOn ? cGreen : cRed;
            break;
        case SelectionPage::USB:
            text = usbOn ? "ON" : "OFF";
            color = usbOn ? cGreen : cRed;
            break;
        case SelectionPage::SOL:
            text = String(solIn) + "W";
            color = cYellow;
            break;
        case SelectionPage::IN:
            text = String(totIn) + "W";
            color = cYellow;
            break;
        default: break;
    }
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

void drawSettingsSubmenu() {
    String text = (currentSettingsPage == SettingsPage::CHG) ? "CHG" : "LIM";
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, cWhite);
}

void drawLimitsSubmenu() {
    String text = (currentLimitsPage == LimitsPage::UP) ? "UP" : "DN";
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, cWhite);
}

void drawEditScreen(String label, int value, String unit) {
    String text;
    if (currentState == MenuState::EDIT_CHG) {
        text = String(value);
    } else {
        text = String(value) + unit;
    }

    uint32_t color = cBlue;
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

void drawDeviceSelectMenu(DeviceSlot* slotD3, DeviceSlot* slotW2) {
    String text = (currentDevicePage == DevicePage::D3) ? "D3" : "W2";
    uint32_t color = (currentDevicePage == DevicePage::D3) ? (slotD3->isConnected ? cGreen : cRed) : (slotW2->isConnected ? cGreen : cRed);
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

void drawDeviceActionMenu(DeviceSlot* slot) {
    String text = "";
    uint32_t color = cWhite;
    switch(currentDeviceAction) {
        case DeviceActionPage::CON: text = "CON"; color = cBlue; break;
        case DeviceActionPage::DIS: text = "DIS"; color = cRed; break;
        case DeviceActionPage::RET: text = "RET"; color = cYellow; break;
    }
    int width = getTextWidth(text) - 1;
    int x = (NUM_COLS - width) / 2;
    drawText(x, 1, text, color);
}

// --- State Machine & Logic ---

void updateDisplay(const EcoflowData& data, DeviceSlot* activeSlot, bool isScanning) {
    currentData = data;
    DeviceSlot* slotD3 = DeviceManager::getInstance().getSlot(DeviceType::DELTA_3);
    DeviceSlot* slotW2 = DeviceManager::getInstance().getSlot(DeviceType::WAVE_2);

    clearFrame();

    unsigned long now = millis();
    unsigned long timeout = 10000; // Default 10s
    if (currentState == MenuState::DASHBOARD) timeout = 0; // No timeout
    else if (currentState == MenuState::EDIT_CHG || currentState == MenuState::EDIT_SOC_UP || currentState == MenuState::EDIT_SOC_DN) {
        timeout = 60000; // 60s for editing
    }

    if (timeout > 0 && (now - lastInteractionTime > timeout)) {
        currentState = MenuState::DASHBOARD;
    }

    if (currentState == MenuState::DASHBOARD) {
        drawDashboard(slotD3, slotW2);
    } else if (currentState == MenuState::SELECTION) {
        strip.setBrightness(25);
        drawSelectionMenu();
    } else if (currentState == MenuState::DETAIL) {
        strip.setBrightness(25);
        DeviceType activeType = activeSlot ? activeSlot->type : DeviceType::DELTA_3;
        drawDetailMenu(activeType);
    } else if (currentState == MenuState::SETTINGS_SUBMENU) {
        strip.setBrightness(25);
        drawSettingsSubmenu();
    } else if (currentState == MenuState::LIMITS_SUBMENU) {
        strip.setBrightness(25);
        drawLimitsSubmenu();
    } else if (currentState == MenuState::EDIT_CHG) {
        strip.setBrightness(25);
        drawEditScreen("CHG", tempAcLimit, "");
    } else if (currentState == MenuState::EDIT_SOC_UP) {
        strip.setBrightness(25);
        drawEditScreen("UP", tempMaxChg, "%");
    } else if (currentState == MenuState::EDIT_SOC_DN) {
        strip.setBrightness(25);
        drawEditScreen("DN", tempMinDsg, "%");
    } else if (currentState == MenuState::DEVICE_SELECT) {
        strip.setBrightness(25);
        drawDeviceSelectMenu(slotD3, slotW2);
    } else if (currentState == MenuState::DEVICE_ACTION) {
        strip.setBrightness(25);
        DeviceSlot* s = (currentDevicePage == DevicePage::D3) ? slotD3 : slotW2;
        drawDeviceActionMenu(s);
    }

    if (isScanning) setPixel(19, 0, cBlue);
    renderFrame();
}

DisplayAction handleDisplayInput(ButtonInput input) {
    lastInteractionTime = millis();

    if (currentState != MenuState::DASHBOARD) {
        if (input == ButtonInput::BTN_ENTER_MEDIUM) {
            currentState = MenuState::DASHBOARD;
            return DisplayAction::NONE;
        }
    }

    if (currentState == MenuState::DASHBOARD) {
        if (input == ButtonInput::BTN_UP || input == ButtonInput::BTN_DOWN) {
             DeviceSlot* sW2 = DeviceManager::getInstance().getSlot(DeviceType::WAVE_2);
             bool w2Avail = sW2->isConnected;
             bool w2BattAvail = w2Avail && (sW2->instance->getBatteryLevel() > 0);

             int currentIdx = (int)currentDashboardView;
             int nextIdx = currentIdx;
             int dir = (input == ButtonInput::BTN_UP) ? 1 : -1;

             for(int i=0; i<4; i++) {
                 nextIdx += dir;
                 if(nextIdx > 3) nextIdx = 0;
                 if(nextIdx < 0) nextIdx = 3;

                 DashboardView v = (DashboardView)nextIdx;
                 if (v == DashboardView::D3_BATT) { currentDashboardView=v; break; }
                 if (v == DashboardView::D3_SOLAR) { currentDashboardView=v; break; }
                 if (v == DashboardView::W2_BATT && w2BattAvail) { currentDashboardView=v; break; }
                 if (v == DashboardView::W2_TEMP && w2Avail) { currentDashboardView=v; break; }
             }
        } else if (input == ButtonInput::BTN_ENTER_SHORT) {
            currentState = MenuState::SELECTION;
            currentSelection = SelectionPage::AC;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::SELECTION) {
        switch(input) {
            case ButtonInput::BTN_UP:
                prevSelection = currentSelection;
                if (currentSelection == SelectionPage::AC) currentSelection = SelectionPage::DEV;
                else currentSelection = (SelectionPage)((int)currentSelection - 1);
                slideDirection = -1; isAnimating = true; animationStep = 0;
                break;
            case ButtonInput::BTN_DOWN:
                prevSelection = currentSelection;
                if (currentSelection == SelectionPage::DEV) currentSelection = SelectionPage::AC;
                else currentSelection = (SelectionPage)((int)currentSelection + 1);
                slideDirection = 1; isAnimating = true; animationStep = 0;
                break;
            case ButtonInput::BTN_ENTER_SHORT:
                if (currentSelection == SelectionPage::DEV) {
                    currentState = MenuState::DEVICE_SELECT;
                    currentDevicePage = DevicePage::D3;
                } else if (currentSelection == SelectionPage::SET) {
                    currentState = MenuState::SETTINGS_SUBMENU;
                    currentSettingsPage = SettingsPage::CHG;
                } else {
                    currentState = MenuState::DETAIL;
                }
                break;
            case ButtonInput::BTN_ENTER_LONG:
                flashScreen(cWhite);
                switch(currentSelection) {
                    case SelectionPage::AC: return DisplayAction::TOGGLE_AC;
                    case SelectionPage::DC: return DisplayAction::TOGGLE_DC;
                    case SelectionPage::USB: return DisplayAction::TOGGLE_USB;
                    default: break;
                }
                break;
             default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::SETTINGS_SUBMENU) {
        switch(input) {
            case ButtonInput::BTN_UP:
            case ButtonInput::BTN_DOWN:
                if (currentSettingsPage == SettingsPage::CHG) currentSettingsPage = SettingsPage::LIM;
                else currentSettingsPage = SettingsPage::CHG;
                break;
            case ButtonInput::BTN_ENTER_SHORT:
                if (currentSettingsPage == SettingsPage::CHG) {
                    currentState = MenuState::EDIT_CHG;
                    tempAcLimit = currentData.delta3.acChargingSpeed;
                    if (tempAcLimit < 200 || tempAcLimit > 2900) tempAcLimit = 400;
                } else {
                    currentState = MenuState::LIMITS_SUBMENU;
                    currentLimitsPage = LimitsPage::UP;
                }
                break;
             default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::LIMITS_SUBMENU) {
        switch(input) {
            case ButtonInput::BTN_UP:
            case ButtonInput::BTN_DOWN:
                if (currentLimitsPage == LimitsPage::UP) currentLimitsPage = LimitsPage::DN;
                else currentLimitsPage = LimitsPage::UP;
                break;
            case ButtonInput::BTN_ENTER_SHORT:
                tempMaxChg = currentData.delta3.batteryChargeLimitMax;
                if (tempMaxChg < 50 || tempMaxChg > 100) tempMaxChg = 100;

                tempMinDsg = currentData.delta3.batteryChargeLimitMin;
                if (tempMinDsg < 0 || tempMinDsg > 30) tempMinDsg = 0;

                if (currentLimitsPage == LimitsPage::UP) {
                    currentState = MenuState::EDIT_SOC_UP;
                } else {
                    currentState = MenuState::EDIT_SOC_DN;
                }
                break;
            default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::EDIT_CHG) {
        switch(input) {
            case ButtonInput::BTN_UP:
                tempAcLimit += 100;
                if (tempAcLimit > 1500) tempAcLimit = 1500;
                break;
            case ButtonInput::BTN_DOWN:
                tempAcLimit -= 100;
                if (tempAcLimit < 400) tempAcLimit = 400;
                break;
            case ButtonInput::BTN_ENTER_LONG:
                flashScreen(cWhite);
                currentState = MenuState::DASHBOARD;
                return DisplayAction::SET_AC_LIMIT;
            default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::EDIT_SOC_UP) {
        switch(input) {
            case ButtonInput::BTN_UP:
                tempMaxChg += 10;
                if (tempMaxChg > 100) tempMaxChg = 100;
                break;
            case ButtonInput::BTN_DOWN:
                tempMaxChg -= 10;
                if (tempMaxChg < 50) tempMaxChg = 50;
                break;
            case ButtonInput::BTN_ENTER_LONG:
                flashScreen(cWhite);
                currentState = MenuState::DASHBOARD;
                return DisplayAction::SET_SOC_LIMITS;
            default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::EDIT_SOC_DN) {
        switch(input) {
            case ButtonInput::BTN_UP:
                tempMinDsg += 10;
                if (tempMinDsg > 30) tempMinDsg = 30;
                break;
            case ButtonInput::BTN_DOWN:
                tempMinDsg -= 10;
                if (tempMinDsg < 0) tempMinDsg = 0;
                break;
            case ButtonInput::BTN_ENTER_LONG:
                flashScreen(cWhite);
                currentState = MenuState::DASHBOARD;
                return DisplayAction::SET_SOC_LIMITS;
            default: break;
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::DETAIL) {
        if (input == ButtonInput::BTN_ENTER_LONG) {
            flashScreen(cWhite);
            switch(currentSelection) {
                case SelectionPage::AC: return DisplayAction::TOGGLE_AC;
                case SelectionPage::DC: return DisplayAction::TOGGLE_DC;
                case SelectionPage::USB: return DisplayAction::TOGGLE_USB;
                default: break;
            }
        }
        return DisplayAction::NONE;
    }

    if (currentState == MenuState::DEVICE_SELECT) {
        switch(input) {
            case ButtonInput::BTN_UP:
            case ButtonInput::BTN_DOWN:
                if (currentDevicePage == DevicePage::D3) currentDevicePage = DevicePage::W2;
                else currentDevicePage = DevicePage::D3;
                break;
            case ButtonInput::BTN_ENTER_SHORT:
                currentState = MenuState::DEVICE_ACTION;
                currentDeviceAction = DeviceActionPage::CON;
                targetDeviceType = (currentDevicePage == DevicePage::D3) ? DeviceType::DELTA_3 : DeviceType::WAVE_2;
                break;
            default: break;
        }
    }

    if (currentState == MenuState::DEVICE_ACTION) {
         switch(input) {
            case ButtonInput::BTN_UP:
            case ButtonInput::BTN_DOWN:
                if (currentDeviceAction == DeviceActionPage::CON) currentDeviceAction = DeviceActionPage::DIS;
                else if (currentDeviceAction == DeviceActionPage::DIS) currentDeviceAction = DeviceActionPage::RET;
                else currentDeviceAction = DeviceActionPage::CON;
                break;
            case ButtonInput::BTN_ENTER_SHORT:
                if (currentDeviceAction == DeviceActionPage::CON) return DisplayAction::CONNECT_DEVICE;
                else if (currentDeviceAction == DeviceActionPage::DIS) return DisplayAction::DISCONNECT_DEVICE;
                else currentState = MenuState::DEVICE_SELECT;
                break;
            default: break;
         }
    }

    return DisplayAction::NONE;
}

DisplayAction getPendingAction() {
    DisplayAction act = pendingAction;
    pendingAction = DisplayAction::NONE;
    return act;
}

int getSetAcLimit() { return tempAcLimit; }
int getSetMaxChgSoc() { return tempMaxChg; }
int getSetMinDsgSoc() { return tempMinDsg; }
DeviceType getTargetDeviceType() { return targetDeviceType; }
