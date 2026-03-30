/**
 * @file DeviceManager.cpp
 * @author Lollokara
 * @brief Implementation for the DeviceManager class.
 *
 * This file contains the logic for managing multiple EcoFlow devices, including
 * BLE scanning, connection management, and persistence of device details.
 */

#include "DeviceManager.h"
#include "Credentials.h"
#include <NimBLEDevice.h>

/**
 * @brief Extracts the device serial number from the BLE manufacturer data.
 * @param manufacturerData The raw manufacturer data from the advertisement packet.
 * @return The extracted serial number as a string, or an empty string if not found.
 */
static std::string extractSerial(const std::string& manufacturerData) {
    // The serial number is typically embedded in the manufacturer data payload.
    // For EcoFlow devices, it is 16 bytes long and starts at an offset.
    if (manufacturerData.length() < 19) return "";

    char serialBuf[17];
    memcpy(serialBuf, manufacturerData.data() + 3, 16);
    serialBuf[16] = '\0';
    return std::string(serialBuf);
}

//--------------------------------------------------------------------------
//--- Singleton and Constructor
//--------------------------------------------------------------------------

DeviceManager& DeviceManager::getInstance() {
    static DeviceManager instance;
    return instance;
}

DeviceManager::DeviceManager() {
    // Initialize device slots
    slotD3.instance = &d3;
    slotD3.name = "D3";
    slotD3.type = DeviceType::DELTA_3;
    slotD3.isConnected = false;

    slotW2.instance = &w2;
    slotW2.name = "W2";
    slotW2.type = DeviceType::WAVE_2;
    slotW2.isConnected = false;

    slotD3P.instance = &d3p;
    slotD3P.name = "D3P";
    slotD3P.type = DeviceType::DELTA_PRO_3;
    slotD3P.isConnected = false;

    slotAC.instance = &ac;
    slotAC.name = "CHG";
    slotAC.type = DeviceType::ALTERNATOR_CHARGER;
    slotAC.isConnected = false;

    _scanMutex = xSemaphoreCreateMutex();
}

//--------------------------------------------------------------------------
//--- Public Methods
//--------------------------------------------------------------------------

void DeviceManager::initialize() {
    NimBLEDevice::init(""); // Initialize BLE centrally
    prefs.begin("ecoflow", false);
    loadDevices();

    // Initialize instances for any saved devices
    if (!slotD3.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring D3: %s", slotD3.serialNumber.c_str());
        slotD3.instance->begin(ECOFLOW_USER_ID, slotD3.serialNumber, slotD3.macAddress, 3);
    }
    if (!slotW2.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring W2: %s", slotW2.serialNumber.c_str());
        slotW2.instance->begin(ECOFLOW_USER_ID, slotW2.serialNumber, slotW2.macAddress, 2);
    }
    if (!slotD3P.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring D3P: %s", slotD3P.serialNumber.c_str());
        slotD3P.instance->begin(ECOFLOW_USER_ID, slotD3P.serialNumber, slotD3P.macAddress, 3);
    }
    if (!slotAC.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring AC: %s", slotAC.serialNumber.c_str());
        slotAC.instance->begin(ECOFLOW_USER_ID, slotAC.serialNumber, slotAC.macAddress, 3);
    }
}

void DeviceManager::update() {
    // The main loop is split into three parts:
    // 1. Handle any pending connection found during a scan.
    _handlePendingConnection();

    // 2. Call the update loop for each device instance.
    slotD3.instance->update();
    slotW2.instance->update();
    slotD3P.instance->update();
    slotAC.instance->update();

    slotD3.isConnected = slotD3.instance->isConnected();
    slotW2.isConnected = slotW2.instance->isConnected();
    slotD3P.isConnected = slotD3P.instance->isConnected();
    slotAC.isConnected = slotAC.instance->isConnected();

    // 3. Manage the scanning state (auto-reconnect or timeout).
    _manageScanning();

    // 4. Update telemetry history (every 60 seconds)
    _updateHistory();
}

void DeviceManager::scanAndConnect(DeviceType type) {
    if (_isScanning) return;
    startScan(type);
}

void DeviceManager::disconnect(DeviceType type) {
    DeviceSlot* slot = getSlot(type);
    if (slot) {
        slot->instance->disconnectAndForget();
        if (type == DeviceType::DELTA_3) {
            prefs.remove("d3_mac");
            prefs.remove("d3_sn");
        } else if (type == DeviceType::WAVE_2) {
            prefs.remove("w2_mac");
            prefs.remove("w2_sn");
        } else if (type == DeviceType::DELTA_PRO_3) {
            prefs.remove("d3p_mac");
            prefs.remove("d3p_sn");
        } else if (type == DeviceType::ALTERNATOR_CHARGER) {
            prefs.remove("ac_mac");
            prefs.remove("ac_sn");
        }
        slot->macAddress = "";
        slot->serialNumber = "";
    }
}

EcoflowESP32* DeviceManager::getDevice(DeviceType type) {
    if (type == DeviceType::DELTA_PRO_3) return &d3p;
    if (type == DeviceType::ALTERNATOR_CHARGER) return &ac;
    return (type == DeviceType::DELTA_3) ? &d3 : &w2;
}

DeviceSlot* DeviceManager::getSlot(DeviceType type) {
    if (type == DeviceType::DELTA_PRO_3) return &slotD3P;
    if (type == DeviceType::ALTERNATOR_CHARGER) return &slotAC;
    return (type == DeviceType::DELTA_3) ? &slotD3 : &slotW2;
}

bool DeviceManager::isScanning() {
    return _isScanning;
}

bool DeviceManager::isAnyConnecting() {
    return d3.isConnecting() || w2.isConnecting() || d3p.isConnecting() || ac.isConnecting();
}

//--------------------------------------------------------------------------
//--- Management Commands
//--------------------------------------------------------------------------

void DeviceManager::printStatus() {
    Serial.println("=== Device Connection Status ===");

    auto printSlot = [](DeviceSlot& slot) {
        Serial.printf("[%s] %s (%s): %s\n",
            slot.name.c_str(),
            slot.isConnected ? "CONNECTED" : "DISCONNECTED",
            slot.macAddress.empty() ? "Unpaired" : slot.macAddress.c_str(),
            slot.serialNumber.c_str()
        );
    };

    printSlot(slotD3);
    printSlot(slotW2);
    printSlot(slotD3P);
    printSlot(slotAC);
}

void DeviceManager::forget(DeviceType type) {
    disconnect(type);
    // disconnect already clears prefs and slot info
    Serial.println("Device forgotten.");
}

String DeviceManager::getDeviceStatusJson() {
    // Simple JSON construction manually to avoid big dependency overhead if not needed,
    // but since we added ArduinoJson, let's use it?
    // Actually manual string building is often faster/smaller for simple fixed structures.
    // Let's use string building for now.

    String json = "{";
    json += "\"d3\":{\"connected\":" + String(slotD3.isConnected) + ", \"sn\":\"" + String(slotD3.serialNumber.c_str()) + "\", \"batt\":" + String(d3.getBatteryLevel()) + "},";
    json += "\"w2\":{\"connected\":" + String(slotW2.isConnected) + ", \"sn\":\"" + String(slotW2.serialNumber.c_str()) + "\", \"batt\":" + String(w2.getBatteryLevel()) + "},";
    json += "\"d3p\":{\"connected\":" + String(slotD3P.isConnected) + ", \"sn\":\"" + String(slotD3P.serialNumber.c_str()) + "\", \"batt\":" + String((int)d3p.getData().deltaPro3.batteryLevel) + "},";
    json += "\"ac\":{\"connected\":" + String(slotAC.isConnected) + ", \"sn\":\"" + String(slotAC.serialNumber.c_str()) + "\", \"batt\":" + String((int)ac.getData().alternatorCharger.batteryLevel) + "}";
    json += "}";
    return json;
}


//--------------------------------------------------------------------------
//--- Private Helper Methods
//--------------------------------------------------------------------------

/**
 * @brief Handles the connection logic for a device that was found during a scan.
 * This is called from the main update loop to avoid blocking the BLE callback.
 */
void DeviceManager::_handlePendingConnection() {
    if (!_hasPendingConnection) return;

    if (xSemaphoreTake(_scanMutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        if (_pendingDevice) {
            // Stop the scan before attempting to connect
            if (pScan && pScan->isScanning()) {
                pScan->stop();
                _isScanning = false;
            }

            // Defer connection if another device is already in the process of connecting
            if (isAnyConnecting()) {
                ESP_LOGW("DeviceManager", "Another device is connecting, deferring pending connection");
                xSemaphoreGive(_scanMutex);
                return;
            }

            ESP_LOGI("DeviceManager", "Executing pending connection...");
            DeviceSlot* slot = getSlot(_targetScanType);

            // Use _pendingSN captured during scan, not slot->serialNumber which might be empty
            if (!_pendingSN.empty()) {
                saveDevice(_targetScanType, _pendingDevice->getAddress().toString(), _pendingSN);
            } else {
                // Fallback to existing if available (shouldn't happen for new discovery)
                saveDevice(_targetScanType, _pendingDevice->getAddress().toString(), slot->serialNumber);
            }

            uint8_t version = (_targetScanType == DeviceType::WAVE_2) ? 2 : 3;
            slot->instance->begin(ECOFLOW_USER_ID, slot->serialNumber, slot->macAddress, version);
            slot->instance->connectTo(_pendingDevice);

            // Clean up
            delete _pendingDevice;
            _pendingDevice = nullptr;
            _hasPendingConnection = false;
            _pendingSN = "";
        }
        xSemaphoreGive(_scanMutex);
    }
}

/**
 * @brief Manages the BLE scanning state, initiating scans for disconnected devices
 * and handling scan timeouts.
 */
void DeviceManager::_manageScanning() {
    if (_isScanning) {
        // Stop scanning if a device starts connecting or if the scan times out
        if (isAnyConnecting()) {
            stopScan();
            ESP_LOGI("DeviceManager", "Stopping scan due to active connection attempt");
        } else if (millis() - _scanStartTime > 10000) { // 10-second scan timeout
            stopScan();
            ESP_LOGI("DeviceManager", "Scan timeout");
        }
    } else if (!isAnyConnecting()) {
        // If not scanning and no device is connecting, check if we need to start a scan
        bool d3NeedsConnect = !slotD3.isConnected && !slotD3.macAddress.empty() && !d3.isConnecting();
        bool w2NeedsConnect = !slotW2.isConnected && !slotW2.macAddress.empty() && !w2.isConnecting();
        bool d3pNeedsConnect = !slotD3P.isConnected && !slotD3P.macAddress.empty() && !d3p.isConnecting();
        bool acNeedsConnect = !slotAC.isConnected && !slotAC.macAddress.empty() && !ac.isConnecting();

        if (d3NeedsConnect) startScan(DeviceType::DELTA_3);
        else if (w2NeedsConnect) startScan(DeviceType::WAVE_2);
        else if (d3pNeedsConnect) startScan(DeviceType::DELTA_PRO_3);
        else if (acNeedsConnect) startScan(DeviceType::ALTERNATOR_CHARGER);
    }
}

void DeviceManager::loadDevices() {
    String mac = prefs.getString("d3_mac", "");
    String sn = prefs.getString("d3_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotD3.macAddress = mac.c_str();
        slotD3.serialNumber = sn.c_str();
    }

    mac = prefs.getString("w2_mac", "");
    sn = prefs.getString("w2_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotW2.macAddress = mac.c_str();
        slotW2.serialNumber = sn.c_str();
    }

    mac = prefs.getString("d3p_mac", "");
    sn = prefs.getString("d3p_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotD3P.macAddress = mac.c_str();
        slotD3P.serialNumber = sn.c_str();
    }

    mac = prefs.getString("ac_mac", "");
    sn = prefs.getString("ac_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotAC.macAddress = mac.c_str();
        slotAC.serialNumber = sn.c_str();
    }
}

void DeviceManager::saveDevice(DeviceType type, const std::string& mac, const std::string& sn) {
    if (type == DeviceType::DELTA_3) {
        prefs.putString("d3_mac", mac.c_str());
        prefs.putString("d3_sn", sn.c_str());
        slotD3.macAddress = mac;
        slotD3.serialNumber = sn;
    } else if (type == DeviceType::WAVE_2) {
        prefs.putString("w2_mac", mac.c_str());
        prefs.putString("w2_sn", sn.c_str());
        slotW2.macAddress = mac;
        slotW2.serialNumber = sn;
    } else if (type == DeviceType::DELTA_PRO_3) {
        prefs.putString("d3p_mac", mac.c_str());
        prefs.putString("d3p_sn", sn.c_str());
        slotD3P.macAddress = mac;
        slotD3P.serialNumber = sn;
    } else if (type == DeviceType::ALTERNATOR_CHARGER) {
        prefs.putString("ac_mac", mac.c_str());
        prefs.putString("ac_sn", sn.c_str());
        slotAC.macAddress = mac;
        slotAC.serialNumber = sn;
    }
}

void DeviceManager::_updateHistory() {
    if (millis() - _lastHistorySample > 60000) { // Every minute
        _lastHistorySample = millis();

        // Wave 2 Ambient Temp
        if (slotW2.isConnected) {
            int temp = w2.getAmbientTemperature();
            _wave2History.push_back((int8_t)temp);
            if (_wave2History.size() > 60) _wave2History.pop_front();
        }

        // Delta 3 Solar Input
        if (slotD3.isConnected) {
            int solar = d3.getSolarInputPower();
            _d3SolarHistory.push_back((int16_t)solar);
            if (_d3SolarHistory.size() > 60) _d3SolarHistory.pop_front();
        }

        // Delta Pro 3 Solar Input
        if (slotD3P.isConnected) {
            const DeltaPro3Data& data = d3p.getData().deltaPro3;
            int solar = (int)(data.solarLvPower + data.solarHvPower);
            _d3pSolarHistory.push_back((int16_t)solar);
            if (_d3pSolarHistory.size() > 60) _d3pSolarHistory.pop_front();
        }
    }
}

std::vector<int> DeviceManager::getWave2TempHistory() {
    std::vector<int> result;
    for (int8_t t : _wave2History) result.push_back((int)t);
    return result;
}

std::vector<int> DeviceManager::getSolarHistory(DeviceType type) {
    std::vector<int> result;
    if (type == DeviceType::DELTA_3) {
        for (int16_t val : _d3SolarHistory) result.push_back((int)val);
    } else if (type == DeviceType::DELTA_PRO_3) {
        for (int16_t val : _d3pSolarHistory) result.push_back((int)val);
    }
    return result;
}


//--------------------------------------------------------------------------
//--- BLE Scanning Logic
//--------------------------------------------------------------------------

void DeviceManager::startScan(DeviceType type) {
    ESP_LOGI("DeviceManager", "Starting scan for type %d", (int)type);
    _targetScanType = type;
    _isScanning = true;
    _scanStartTime = millis();
    _hasPendingConnection = false;

    if (!pScan) {
        pScan = NimBLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(new ManagerScanCallbacks(this), true);
        pScan->setActiveScan(true);
        pScan->setInterval(100);
        pScan->setWindow(99);
    }
    pScan->start(0, nullptr, false); // Non-blocking scan
}

void DeviceManager::stopScan() {
    if (xSemaphoreTake(_scanMutex, portMAX_DELAY) == pdTRUE) {
        if (pScan && pScan->isScanning()) {
            pScan->stop();
        }
        _isScanning = false;
        xSemaphoreGive(_scanMutex);
    }
}

void DeviceManager::ManagerScanCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (_parent->isScanning()) {
        _parent->onDeviceFound(advertisedDevice);
    }
}

/**
 * @brief Callback function that is executed when a BLE device is found during a scan.
 * It checks if the found device matches a configured device and flags it for connection.
 */
void DeviceManager::onDeviceFound(NimBLEAdvertisedDevice* device) {
    if (_hasPendingConnection) return; // Already found a device, ignore others

    if (xSemaphoreTake(_scanMutex, 100 / portTICK_PERIOD_MS) == pdTRUE) {
        if (_hasPendingConnection) {
            xSemaphoreGive(_scanMutex);
            return;
        }

        if (!device->haveManufacturerData()) {
            xSemaphoreGive(_scanMutex);
            return;
        }

        std::string sn = extractSerial(device->getManufacturerData());
        if (sn.empty()) {
            xSemaphoreGive(_scanMutex);
            return;
        }

        DeviceSlot* targetSlot = nullptr;
        if (isTargetDevice(sn, DeviceType::DELTA_3) && !slotD3.isConnected && !d3.isConnecting()) {
            targetSlot = &slotD3;
        } else if (isTargetDevice(sn, DeviceType::WAVE_2) && !slotW2.isConnected && !w2.isConnecting()) {
            targetSlot = &slotW2;
        } else if (isTargetDevice(sn, DeviceType::DELTA_PRO_3) && !slotD3P.isConnected && !d3p.isConnecting()) {
            targetSlot = &slotD3P;
        } else if (isTargetDevice(sn, DeviceType::ALTERNATOR_CHARGER) && !slotAC.isConnected && !ac.isConnecting()) {
            targetSlot = &slotAC;
        }

        if (targetSlot) {
            bool macMatch = !targetSlot->macAddress.empty() && (device->getAddress().toString() == targetSlot->macAddress);
            bool isNewDeviceScan = targetSlot->macAddress.empty() && _targetScanType == targetSlot->type;

            if (macMatch || isNewDeviceScan) {
                ESP_LOGI("DeviceManager", "Match found for %s (%s)! Pending connection...", targetSlot->name.c_str(), sn.c_str());
                if (_pendingDevice) delete _pendingDevice;
                _pendingDevice = new NimBLEAdvertisedDevice(*device);
                _pendingSN = sn; // Capture the SN for connection
                _hasPendingConnection = true;
                _targetScanType = targetSlot->type; // Lock in the target type
            }
        }
        xSemaphoreGive(_scanMutex);
    }
}

bool DeviceManager::isTargetDevice(const std::string& sn, DeviceType type) {
    if (type == DeviceType::DELTA_3) {
        return (sn.rfind("P2", 0) == 0 || sn.rfind("R", 0) == 0);
    } else if (type == DeviceType::WAVE_2) {
        return (sn.rfind("KT", 0) == 0);
    } else if (type == DeviceType::DELTA_PRO_3) {
        return (sn.rfind("MR51", 0) == 0);
    } else if (type == DeviceType::ALTERNATOR_CHARGER) {
        return (sn.rfind("F371", 0) == 0 || sn.rfind("F372", 0) == 0 || sn.rfind("DC01", 0) == 0);
    }
    return false;
}
