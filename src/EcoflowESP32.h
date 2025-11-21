/**
 * @file EcoflowESP32.h
 * @author Lollokara
 * @brief Main header file for the EcoflowESP32 library.
 *
 * This file defines the primary class, EcoflowESP32, which manages the BLE
 * connection, authentication, and communication with EcoFlow devices.
 */

#ifndef ECOFLOW_ESP32_H
#define ECOFLOW_ESP32_H

#include <NimBLEDevice.h>
#include "EcoflowData.h"
#include "EcoflowCrypto.h"
#include <vector>
#include <string>
#include "freertos/task.h"
#include "freertos/queue.h"
#include "pd335_sys.pb.h"

#define MAX_CONNECT_ATTEMPTS 5

// Forward declaration
class Packet;

/**
 * @enum ConnectionState
 * @brief Defines the possible states of the BLE connection with the EcoFlow device.
 */
enum class ConnectionState {
    NOT_CONNECTED,
    SCANNING,
    CREATED,
    ESTABLISHING_CONNECTION,
    CONNECTED,
    SERVICE_DISCOVERY,
    SUBSCRIBING_NOTIFICATIONS,
    PUBLIC_KEY_EXCHANGE,
    PUBLIC_KEY_RECEIVED,
    REQUESTING_SESSION_KEY,
    SESSION_KEY_RECEIVED,
    REQUESTING_AUTH_STATUS,
    AUTH_STATUS_RECEIVED,
    AUTHENTICATING,
    AUTHENTICATED,

    ERROR_TIMEOUT,
    ERROR_NOT_FOUND,
    ERROR_BLE,
    ERROR_PACKET_PARSE,
    ERROR_SEND_REQUEST,
    ERROR_UNKNOWN,
    ERROR_AUTH_FAILED,
    ERROR_TOO_MANY_ERRORS,

    RECONNECTING,
    ERROR_MAX_RECONNECT_ATTEMPTS_REACHED,

    DISCONNECTING,
    DISCONNECTED
};

/**
 * @class EcoflowClientCallback
 * @brief Internal class to handle NimBLE client connection and disconnection events.
 */
class EcoflowClientCallback : public NimBLEClientCallbacks {
public:
    EcoflowClientCallback(class EcoflowESP32* instance);
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;
private:
    class EcoflowESP32* _instance;
};

/**
 * @class EcoflowESP32
 * @brief The main class for interacting with an EcoFlow device over BLE.
 *
 * This class handles everything from scanning and connecting to a device,
 * to authenticating and sending/receiving commands. It manages the connection
 * state and provides a simple API for accessing device data and controlling its ports.
 */
class EcoflowESP32 {
public:
    /**
     * @brief Constructor for the EcoflowESP32 class.
     */
    EcoflowESP32();
    /**
     * @brief Destructor for the EcoflowESP32 class.
     */
    ~EcoflowESP32();

    /**
     * @brief Initializes the library and starts the BLE connection process.
     * @param userId Your EcoFlow user ID.
     * @param deviceSn The serial number of your EcoFlow device.
     * @param ble_address The BLE MAC address of your device.
     * @param protocolVersion The protocol version to use (default is 3).
     * @return True if initialization was successful, false otherwise.
     */
    bool begin(const std::string& userId, const std::string& deviceSn, const std::string& ble_address, uint8_t protocolVersion = 3);

    /**
     * @brief Main loop function, should be called repeatedly in your sketch's loop().
     * This function processes incoming BLE data and manages the connection state.
     */
    void update();

    //--------------------------------------------------------------------------
    //--- Device Data Getters
    //--------------------------------------------------------------------------

    const EcoflowData& getData() const { return _data; }
    int getBatteryLevel();
    int getInputPower();
    int getOutputPower();
    int getBatteryVoltage();
    int getACVoltage();
    int getACFrequency();
    int getSolarInputPower();
    int getAcOutputPower();
    int getDcOutputPower();
    int getCellTemperature();
    int getAmbientTemperature();
    int getMaxChgSoc();
    int getMinDsgSoc();
    int getAcChgLimit();

    //--------------------------------------------------------------------------
    //--- Device State Getters
    //--------------------------------------------------------------------------
    bool isAcOn();
    bool isDcOn();
    bool isUsbOn();
    bool isConnected();
    bool isConnecting();
    bool isAuthenticated();

    //--------------------------------------------------------------------------
    //--- Device Control Functions
    //--------------------------------------------------------------------------
    bool requestData();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);
    bool setAcChargingLimit(int watts);
    bool setBatterySOCLimits(int maxChg, int minDsg);

    /**
     * @brief Disconnects from the device and clears saved credentials.
     */
    void disconnectAndForget();

    //--------------------------------------------------------------------------
    //--- Public members for internal FreeRTOS task access ---
    //--- (Do not use these directly in your sketch)
    //--------------------------------------------------------------------------
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    void connectTo(NimBLEAdvertisedDevice* device);

    uint32_t _lastKeepAliveTime = 0;
    uint8_t _connectionRetries = 0;
    uint32_t _lastConnectionAttempt = 0;
    uint32_t _lastScanTime = 0;
    uint32_t _lastAuthActivity = 0;

    EcoflowCrypto _crypto;
    EcoflowData _data;
    NimBLEClient* _pClient = nullptr;
    NimBLEAdvertisedDevice* _pAdvertisedDevice = nullptr;

    static void ble_task_entry(void* pvParameters);
    TaskHandle_t _ble_task_handle = nullptr;
    QueueHandle_t _ble_queue = nullptr;

    struct BleNotification {
        uint8_t* data;
        size_t length;
    };
    std::vector<uint8_t> _rxBuffer;

private:
    static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void _handlePacket(Packet* pkt);

    bool _sendCommand(const std::vector<uint8_t>& command);
    
    // --- Authentication Flow ---
    void _startAuthentication();
    void _handleAuthPacket(Packet* pkt);
    void _sendConfigPacket(const pd335_sys_ConfigWrite& config);

    static std::vector<EcoflowESP32*> _instances;
    ConnectionState _state = ConnectionState::NOT_CONNECTED;
    ConnectionState _lastState = ConnectionState::NOT_CONNECTED;

    std::string _userId;
    std::string _deviceSn;
    std::string _ble_address;
    uint8_t _protocolVersion = 3;
    uint32_t _txSeq = 0;

    NimBLERemoteCharacteristic* _pWriteChr = nullptr;
    NimBLERemoteCharacteristic* _pReadChr = nullptr;
    EcoflowClientCallback* _clientCallback;
};

#endif // ECOFLOW_ESP32_H
