#ifndef ECOFLOW_DATA_H
#define ECOFLOW_DATA_H

#include <stdint.h>
#include <string>

struct Delta3Data {
    // Fields from delta3_classic.py
    float batteryLevel = 0;
    float acInputPower = 0;
    float acOutputPower = 0;
    float inputPower = 0;
    float outputPower = 0;
    float dc12vOutputPower = 0;
    float dcPortInputPower = 0;
    int dcPortState = -1; // Unknown=-1, Off=0, Car=1, Solar=2

    float usbcOutputPower = 0;
    float usbc2OutputPower = 0;
    float usbaOutputPower = 0;
    float usba2OutputPower = 0;

    bool pluggedInAc = false;
    bool energyBackup = false;
    int energyBackupBatteryLevel = 0;

    float batteryInputPower = 0;
    float batteryOutputPower = 0;

    int batteryChargeLimitMin = 0;
    int batteryChargeLimitMax = 100;

    int cellTemperature = 0;
    bool dc12vPort = false;
    bool acPorts = false;

    float solarInputPower = 0;
    int acChargingSpeed = 0;
    int maxAcChargingPower = 1500;

    // Common fields inferred/mapped
    bool acOn = false;
    bool dcOn = false;
    bool usbOn = false;
};

struct Wave2Data {
    // Fields from kt210_ble_parser.py (108 bytes)
    int mode = 0;
    int subMode = 0;
    int setTemp = 0;
    int fanValue = 0;
    float envTemp = 0.0f;
    int tempSys = 0;
    int displayIdleTime = 0;
    int displayIdleMode = 0;
    int timeEn = 0;
    int timeSetVal = 0;
    int timeRemainVal = 0;
    int beepEnable = 0;
    uint32_t errCode = 0;
    int refEn = 0;
    int bmsPid = 0;
    int wteFthEn = 0;
    int tempDisplay = 0;
    int powerMode = 0;
    int powerSrc = 0;
    int psdrPwrWatt = 0;
    int batPwrWatt = 0;
    int mpptPwrWatt = 0;
    uint32_t batDsgRemainTime = 0;
    uint32_t batChgRemainTime = 0;
    int batSoc = 0;
    int batChgStatus = 0;
    float outLetTemp = 0.0f;
    int mpptWork = 0;
    int bmsErr = 0;
    int rgbState = 0;
    int waterValue = 0;
    int bmsBoundFlag = 0;
    int bmsUndervoltage = 0;
    int ver = 0;

    // Helpers
    int remainingTime = 0;
};

struct DeltaPro3Data {
    float batteryLevel = 0;
    float acInputPower = 0;
    float acLvOutputPower = 0;
    float acHvOutputPower = 0;
    float inputPower = 0;
    float outputPower = 0;
    float dc12vOutputPower = 0;
    float dcLvInputPower = 0;
    float dcHvInputPower = 0;
    int dcLvInputState = -1;
    int dcHvInputState = -1;
    float usbcOutputPower = 0;
    float usbc2OutputPower = 0;
    float usbaOutputPower = 0;
    float usba2OutputPower = 0;
    int acChargingSpeed = 0;
    int maxAcChargingPower = 0;
    bool pluggedInAc = false;
    bool energyBackup = false;
    int energyBackupBatteryLevel = 0;
    int batteryChargeLimitMin = 0;
    int batteryChargeLimitMax = 100;
    int cellTemperature = 0;
    bool dc12vPort = false;
    bool acLvPort = false;
    bool acHvPort = false;
    float solarLvPower = 0;
    float solarHvPower = 0;
};

struct AlternatorChargerData {
    float batteryLevel = 0;
    float batteryTemperature = 0;
    float dcPower = 0;
    float carBatteryVoltage = 0;
    float startVoltage = 0;
    int startVoltageMin = 11;
    int startVoltageMax = 31;
    int chargerMode = 0; // 0=Idle, 1=Charge, 2=Maintenance, 3=Reverse
    bool chargerOpen = false;
    int powerLimit = 0;
    int powerMax = 0;
    float reverseChargingCurrentLimit = 0;
    float chargingCurrentLimit = 0;
    float reverseChargingCurrentMax = 0;
    float chargingCurrentMax = 0;
};

struct EcoflowData {
    bool isConnected = false;

    // Substructs
    Delta3Data delta3;
    Wave2Data wave2;
    DeltaPro3Data deltaPro3;
    AlternatorChargerData alternatorCharger;
};

#endif // ECOFLOW_DATA_H
