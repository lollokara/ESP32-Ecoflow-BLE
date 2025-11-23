/**
 * @file EcoflowESP32.cpp
 * @author Lollokara
 * @brief Implementation file for the EcoflowESP32 library.
 *
 * This file contains the core logic for managing the BLE connection, handling
 * the state machine for authentication, and processing incoming/outgoing data.
 * It uses a dedicated FreeRTOS task to handle all BLE operations non-blockingly.
 */

#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include "EcoflowDataParser.h"
#include <NimBLEDevice.h>
#include "esp_log.h"
#include "pd335_sys.pb.h"
#include "mr521.pb.h"
#include "dc009_apl_comm.pb.h"
#include <pb_encode.h>
static const char* TAG = "EcoflowESP32";

// Static vector to hold instances for the static notify callback
std::vector<EcoflowESP32*> EcoflowESP32::_instances;

//--------------------------------------------------------------------------
//--- Static BLE Callbacks
//--------------------------------------------------------------------------

/**
 * @brief Static callback for BLE notifications.
 * This function receives data from the device and queues it for processing
 * in the dedicated BLE task.
 */
void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    ESP_LOGV(TAG, "Notify callback received %d bytes", length);

    NimBLEClient* pClient = pRemoteCharacteristic->getRemoteService()->getClient();
    for (auto* instance : _instances) {
        if (instance->_pClient == pClient && instance->_ble_queue) {
            BleNotification* notification = new BleNotification;
            notification->data = new uint8_t[length];
            memcpy(notification->data, pData, length);
            notification->length = length;
            if (xQueueSend(instance->_ble_queue, &notification, 0) != pdTRUE) {
                ESP_LOGW(TAG, "BLE notification queue full, dropping packet.");
                delete[] notification->data;
                delete notification;
            }
            return;
        }
    }
    ESP_LOGW(TAG, "Notification received but no matching instance found");
}

EcoflowClientCallback::EcoflowClientCallback(EcoflowESP32* instance) : _instance(instance) {}

void EcoflowClientCallback::onConnect(NimBLEClient* pClient) {
    ESP_LOGI(TAG, "Connected to device");
    if (_instance) {
        _instance->onConnect(pClient);
    }
}

void EcoflowClientCallback::onDisconnect(NimBLEClient* pClient) {
    ESP_LOGI(TAG, "Disconnected from device");
    if (_instance) {
        _instance->onDisconnect(pClient);
    }
}

//--------------------------------------------------------------------------
//--- Constructor / Destructor
//--------------------------------------------------------------------------

EcoflowESP32::EcoflowESP32() {
    _instances.push_back(this);
    _clientCallback = new EcoflowClientCallback(this);
}

EcoflowESP32::~EcoflowESP32() {
    if (_pClient) {
        NimBLEDevice::deleteClient(_pClient);
    }
    delete _clientCallback;

    for (auto it = _instances.begin(); it != _instances.end(); ++it) {
        if (*it == this) {
            _instances.erase(it);
            break;
        }
    }
}

//--------------------------------------------------------------------------
//--- Public API
//--------------------------------------------------------------------------

bool EcoflowESP32::begin(const std::string& userId, const std::string& deviceSn, const std::string& ble_address, uint8_t protocolVersion) {
    ESP_LOGI(TAG, "begin: Initializing device %s with protocol version %d", deviceSn.c_str(), protocolVersion);
    _userId = userId;
    _deviceSn = deviceSn;
    _ble_address = ble_address;
    _protocolVersion = protocolVersion;

    if (!_pClient) {
        _pClient = NimBLEDevice::createClient();
        _pClient->setClientCallbacks(_clientCallback);
    }

    if (!_ble_queue) {
        _ble_queue = xQueueCreate(10, sizeof(BleNotification*));
    }

    if (!_ble_task_handle) {
        xTaskCreate(ble_task_entry, "ble_task", 12288, this, 5, &_ble_task_handle);
    }
    return true;
}

void EcoflowESP32::update() {
    // This function is intentionally left empty.
    // All processing is handled by the dedicated FreeRTOS task.
}

void EcoflowESP32::disconnectAndForget() {
    if (_pClient && _pClient->isConnected()) {
        _pClient->disconnect();
    }
    _ble_address = "";
    _deviceSn = "";
    _state = ConnectionState::NOT_CONNECTED;
    if (_pAdvertisedDevice) {
        delete _pAdvertisedDevice;
        _pAdvertisedDevice = nullptr;
    }
}


//--------------------------------------------------------------------------
//--- BLE Task and State Machine
//--------------------------------------------------------------------------

/**
 * @brief Main entry point for the FreeRTOS task that handles all BLE logic.
 */
void EcoflowESP32::ble_task_entry(void* pvParameters) {
    EcoflowESP32* self = (EcoflowESP32*)pvParameters;
    for (;;) {
        // Log state changes for debugging
        if (self->_state != self->_lastState) {
            ESP_LOGI(TAG, "State changed: %d -> %d", (int)self->_lastState, (int)self->_state);
            self->_lastState = self->_state;
        }

        // --- Connection Management ---
        if (self->_pAdvertisedDevice && !self->_pClient->isConnected()) {
            // Attempt to connect if a device has been found
            if (millis() - self->_lastConnectionAttempt > 5000) { // 5s retry delay
                self->_lastConnectionAttempt = millis();
                if (self->_connectionRetries < MAX_CONNECT_ATTEMPTS) {
                    ESP_LOGI(TAG, "Connecting... (Attempt %d/%d)", self->_connectionRetries + 1, MAX_CONNECT_ATTEMPTS);
                    self->_state = ConnectionState::ESTABLISHING_CONNECTION;
                    self->_connectionRetries++;
                    if(!self->_pClient->connect(self->_pAdvertisedDevice)) {
                         ESP_LOGE(TAG, "Connect failed");
                    }
                } else {
                    ESP_LOGE(TAG, "Max connection attempts reached.");
                    delete self->_pAdvertisedDevice;
                    self->_pAdvertisedDevice = nullptr;
                    self->_state = ConnectionState::DISCONNECTED;
                }
            }
        } else if (self->_state >= ConnectionState::CONNECTED && !self->_pClient->isConnected()){
             ESP_LOGW(TAG, "Client disconnected unexpectedly");
             self->onDisconnect(self->_pClient);
        }

        // --- State Machine for Connected Client ---
        if (self->_pClient->isConnected()) {
            switch(self->_state) {
                case ConnectionState::SERVICE_DISCOVERY: {
                    NimBLERemoteService* pSvc = self->_pClient->getService("00000001-0000-1000-8000-00805f9b34fb");
                    if (pSvc) {
                        self->_pWriteChr = pSvc->getCharacteristic("00000002-0000-1000-8000-00805f9b34fb");
                        self->_pReadChr = pSvc->getCharacteristic("00000003-0000-1000-8000-00805f9b34fb");
                        if (self->_pReadChr && self->_pWriteChr) {
                            self->_state = ConnectionState::SUBSCRIBING_NOTIFICATIONS;
                        }
                    } else {
                        ESP_LOGE(TAG, "Service not found, disconnecting");
                        self->_pClient->disconnect();
                    }
                    break;
                }
                case ConnectionState::SUBSCRIBING_NOTIFICATIONS:
                    if (self->_pReadChr->canNotify() && self->_pReadChr->subscribe(true, notifyCallback)) {
                        ESP_LOGI(TAG, "Subscribed to notifications");
                        self->_state = ConnectionState::CONNECTED;
                    } else {
                        ESP_LOGE(TAG, "Failed to subscribe to notifications");
                        self->_pClient->disconnect();
                    }
                    break;
                case ConnectionState::CONNECTED:
                    self->_startAuthentication();
                    break;
                case ConnectionState::AUTHENTICATED:
                    // Send a keep-alive request for data every 5 seconds
                    if (millis() - self->_lastKeepAliveTime > 5000) {
                        self->_lastKeepAliveTime = millis();
                        self->requestData();
                    }
                    break;
                default:
                    // Handle authentication timeout
                    if (self->_state > ConnectionState::CONNECTED && self->_state < ConnectionState::AUTHENTICATED) {
                        if (millis() - self->_lastAuthActivity > 10000) {
                            ESP_LOGW(TAG, "Authentication timed out");
                            self->_pClient->disconnect();
                        }
                    }
                    break;
            }
        }
        /*
        // --- Process Incoming Notifications ---
        BleNotification* notification;
        if (xQueueReceive(self->_ble_queue, &notification, 0) == pdTRUE) {
            self->_lastAuthActivity = millis();
            if (self->_state == ConnectionState::PUBLIC_KEY_EXCHANGE) {
                auto parsed_payload = EncPacket::parseSimple(notification->data, notification->length);
                if (!parsed_payload.empty() && parsed_payload.size() >= 43 && parsed_payload[0] == 0x01) {
                    uint8_t peer_pub_key[41];
                    peer_pub_key[0] = 0x04; // Add the 0x04 prefix for mbedtls
                    memcpy(peer_pub_key + 1, parsed_payload.data() + 3, 40);
                    if (self->_crypto.compute_shared_secret(peer_pub_key, sizeof(peer_pub_key))) {
                        self->_state = ConnectionState::REQUESTING_SESSION_KEY;
                        ESP_LOGD(TAG, "Shared secret computed, requesting session key");
                        std::vector<uint8_t> req_payload = {0x02};
                        EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, req_payload);
                        self->_sendCommand(enc_packet.toBytes());
                    } else {
                        ESP_LOGE(TAG, "Failed to compute shared secret");
                        self->_pClient->disconnect();
                    }
                }
            } else if (self->_state == ConnectionState::REQUESTING_SESSION_KEY) {
                auto parsed_payload = EncPacket::parseSimple(notification->data, notification->length);
                if (!parsed_payload.empty() && parsed_payload.size() > 1 && parsed_payload[0] == 0x02) {
                    std::vector<uint8_t> decrypted_payload(parsed_payload.size() - 1);
                    self->_crypto.decrypt_shared(parsed_payload.data() + 1, parsed_payload.size() - 1, decrypted_payload.data());

                    if (!decrypted_payload.empty()) {
                        uint8_t padding = decrypted_payload.back();
                        if (padding > 0 && padding <= 16 && decrypted_payload.size() >= padding) {
                            decrypted_payload.resize(decrypted_payload.size() - padding);
                        }
                    }

                    if (decrypted_payload.size() >= 18) {
                        self->_crypto.generate_session_key(decrypted_payload.data() + 16, decrypted_payload.data());
                        self->_state = ConnectionState::REQUESTING_AUTH_STATUS;
                        ESP_LOGD(TAG, "Session key generated, requesting auth status");
                        uint8_t dest = self->_getDeviceDest();
                        Packet auth_status_pkt(0x21, dest, 0x35, 0x89, {}, 0x01, 0x01, self->_protocolVersion, self->_txSeq++, 0x0d);
                        EncPacket enc_auth_status(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, auth_status_pkt.toBytes());
                        self->_sendCommand(enc_auth_status.toBytes(&self->_crypto));
                    }
                }
            } else {
                 std::vector<Packet> packets = EncPacket::parsePackets(notification->data, notification->length, self->_crypto, self->_rxBuffer, self->isAuthenticated());
                for (auto &packet : packets) {
                    self->_handlePacket(&packet);
                }
            }
            delete[] notification->data;
            delete notification;
        }*/
        BleNotification *notification;
        if (xQueueReceive(self->_ble_queue, &notification, 0) == pdTRUE) {
          if (self->_state == ConnectionState::PUBLIC_KEY_EXCHANGE ||
              self->_state == ConnectionState::REQUESTING_SESSION_KEY) {
            std::vector<uint8_t> raw_payload =
                EncPacket::parseSimple(notification->data, notification->length);
            if (!raw_payload.empty()) {
              self->_handleAuthHandshake(raw_payload);
            }
          } else {
            std::vector<Packet> packets = EncPacket::parsePackets(
                notification->data, notification->length, self->_crypto,
                self->_rxBuffer, self->isAuthenticated());
            if (packets.empty() && notification->length > 0) {
                ESP_LOGW(TAG, "Received %d bytes but no packets parsed", notification->length);
            }
            for (auto &packet : packets) {
              self->_handlePacket(&packet);
            }
          }
          delete[] notification->data;
          delete notification;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

//--------------------------------------------------------------------------
//--- Internal Connection and Authentication Logic
//--------------------------------------------------------------------------

void EcoflowESP32::connectTo(NimBLEAdvertisedDevice* device) {
    if (_pAdvertisedDevice) delete _pAdvertisedDevice;
    _pAdvertisedDevice = new NimBLEAdvertisedDevice(*device);
    _state = ConnectionState::CREATED; // Signal task to connect
    _connectionRetries = 0;
}

void EcoflowESP32::onConnect(NimBLEClient* pClient) {
    _connectionRetries = 0;
    _state = ConnectionState::SERVICE_DISCOVERY;
    _lastAuthActivity = millis();
}

void EcoflowESP32::onDisconnect(NimBLEClient* pClient) {
    _state = ConnectionState::DISCONNECTED;
    if (_pAdvertisedDevice) {
        delete _pAdvertisedDevice;
        _pAdvertisedDevice = nullptr;
    }
}

/**
 * @brief Handles raw payload received during initial handshake (Public Key,
 * Session Key).
 */
void EcoflowESP32::_handleAuthHandshake(const std::vector<uint8_t> &payload) {
  ESP_LOGV(TAG, "_handleAuthHandshake: received %d bytes", payload.size());

  if (_state == ConnectionState::PUBLIC_KEY_EXCHANGE) {
    if (payload.size() >= 43 && payload[0] == 0x01) {
      uint8_t peer_pub_key[41];
      peer_pub_key[0] = 0x04; // Uncompressed point
      memcpy(peer_pub_key + 1, payload.data() + 3, 40);
      if (_crypto.compute_shared_secret(peer_pub_key, sizeof(peer_pub_key))) {
        _state = ConnectionState::REQUESTING_SESSION_KEY;
        std::vector<uint8_t> req_payload = {0x02};
        EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND,
                             EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, req_payload);
        _sendCommand(enc_packet.toBytes());
      } else {
        ESP_LOGE(TAG, "Failed to compute shared secret");
      }
    }
  } else if (_state == ConnectionState::REQUESTING_SESSION_KEY) {
    if (payload.size() > 1) {
      std::vector<uint8_t> decrypted_payload;
      size_t encrypted_len = payload.size() - 1;
      decrypted_payload.resize(encrypted_len);
      // Decrypt starting from payload[1], length is size-1
      _crypto.decrypt_shared(payload.data() + 1, encrypted_len,
                             decrypted_payload.data());
      
      // Remove padding
      if (!decrypted_payload.empty()) {
          uint8_t padding = decrypted_payload.back();
          if (padding > 0 && padding <= 16 && decrypted_payload.size() >= padding) {
              decrypted_payload.resize(decrypted_payload.size() - padding);
          }
      }

      if (decrypted_payload.size() >= 18) {
        // Use decrypted_payload.data() + 16 as seed, decrypted_payload.data() as srand
        _crypto.generate_session_key(decrypted_payload.data() + 16,
                                     decrypted_payload.data());
        _state = ConnectionState::REQUESTING_AUTH_STATUS;

        // Use correct version and sequence for V2 devices
        ESP_LOGI(TAG, "_handleAuthHandshake: Requesting Auth Status. Protocol Version: %d", _protocolVersion);
        uint8_t auth_version = (_protocolVersion == 2) ? 2 : 3;
        uint32_t auth_seq = (_protocolVersion == 2) ? _txSeq++ : 0;

        Packet auth_status_pkt(0x21, 0x35, 0x35, 0x89, {}, 0x01, 0x01,
                               auth_version, auth_seq, 0x0d);
        EncPacket enc_auth_status(EncPacket::FRAME_TYPE_PROTOCOL,
                                  EncPacket::PAYLOAD_TYPE_VX_PROTOCOL,
                                  auth_status_pkt.toBytes());
        _sendCommand(enc_auth_status.toBytes(&_crypto));
      }
    } else {
      ESP_LOGE(TAG, "Invalid session key payload size");
    }
  }
}

void EcoflowESP32::_startAuthentication() {
    ESP_LOGI(TAG, "Starting authentication");
    //_txSeq = esp_random();
    _txSeq = 1;
    if (!_crypto.generate_keys()) {
        ESP_LOGE(TAG, "Failed to generate keys");
        return;
    }

    std::vector<uint8_t> payload;
    payload.push_back(0x01); // Handshake type: Public Key
    payload.push_back(0x00); // Reserved
    uint8_t* pub_key = _crypto.get_public_key();
    payload.insert(payload.end(), pub_key, pub_key + _crypto.get_public_key_len());

    ESP_LOGD(TAG, "Initiating handshake: Sending public key...");
    EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, payload);
    if (_sendCommand(enc_packet.toBytes())) {
        _state = ConnectionState::PUBLIC_KEY_EXCHANGE;
        _lastAuthActivity = millis();
    } else {
        ESP_LOGE(TAG, "Failed to send public key packet.");
        _pClient->disconnect();
    }
}

/**
 * @brief Main packet handler, routes packets to the correct handler based on auth state.
 */
void EcoflowESP32::_handlePacket(Packet* pkt) {
    ESP_LOGD(TAG, "_handlePacket: cmdId=0x%02x", pkt->getCmdId());
    if (isAuthenticated()) {
        EcoflowDataParser::parsePacket(*pkt, _data);

        // For Protocol V2 (Wave 2), prevent infinite loops by NOT replying to Control/Status commands (0x51-0x5E)
        // These packets are likely notifications that share the same ID as the Set command.
        // Replying to them triggers the device to treat the ACK as a new Set command, creating a loop.
        bool shouldReply = true;
        if (_protocolVersion == 2) {
             uint8_t cmdId = pkt->getCmdId();
             // Range 0x51-0x5E covers all known Set/Control commands for Wave 2
             if (cmdId >= 0x51 && cmdId <= 0x5E) {
                 ESP_LOGD(TAG, "Skipping reply for Wave 2 Control Packet (CmdId: 0x%02x)", cmdId);
                 shouldReply = false;
             }
        }

        // Reply to packets that require it to keep the data flowing
        if (shouldReply && pkt->getDest() == 0x21) {
            Packet reply(pkt->getDest(), pkt->getSrc(), pkt->getCmdSet(), pkt->getCmdId(), pkt->getPayload(), 0x01, 0x01, pkt->getVersion(), pkt->getSeq(), 0x0d);
            EncPacket enc_reply(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, reply.toBytes());
            _sendCommand(enc_reply.toBytes(&_crypto));
        }
    } else {
        _handleAuthPacket(pkt);
    }
}

/**
 * @brief Handles packets received during the authentication process.
 */
void EcoflowESP32::_handleAuthPacket(Packet* pkt) {
    ESP_LOGD(TAG, "_handleAuthPacket: cmdId=0x%02x", pkt->getCmdId());
    const auto& payload = pkt->getPayload();

    if (_state == ConnectionState::PUBLIC_KEY_EXCHANGE) {
        if (!payload.empty() && payload.size() >= 43 && payload[0] == 0x01) {
            uint8_t peer_pub_key[41];
            peer_pub_key[0] = 0x04;
            memcpy(peer_pub_key + 1, payload.data() + 3, 40);
            if (_crypto.compute_shared_secret(peer_pub_key, sizeof(peer_pub_key))) {
                _state = ConnectionState::REQUESTING_SESSION_KEY;
                std::vector<uint8_t> req_payload = {0x02};
                EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, req_payload);
                _sendCommand(enc_packet.toBytes());
            }
        }
    } else if (_state == ConnectionState::REQUESTING_SESSION_KEY) {
        std::vector<uint8_t> decrypted_payload;
        _crypto.decrypt_shared(payload.data(), payload.size(), decrypted_payload.data());
        if (decrypted_payload.size() >= 18) {
            _crypto.generate_session_key(decrypted_payload.data() + 16, decrypted_payload.data());
            _state = ConnectionState::REQUESTING_AUTH_STATUS;

            uint8_t auth_version = (_protocolVersion == 2) ? 2 : 3;
            uint32_t auth_seq = (_protocolVersion == 2) ? _txSeq++ : 0;

            Packet auth_status_pkt(0x21, 0x35, 0x35, 0x89, {}, 0x01, 0x01, auth_version, auth_seq, 0x0d);
            EncPacket enc_auth_status(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, auth_status_pkt.toBytes());
            _sendCommand(enc_auth_status.toBytes(&_crypto));
        }
    } else if (_state == ConnectionState::REQUESTING_AUTH_STATUS) {
        if (pkt->getCmdSet() == 0x35 && pkt->getCmdId() == 0x89) {
            _state = ConnectionState::AUTHENTICATING;
            uint8_t md5_data[16];
            mbedtls_md5((const unsigned char*)(_userId + _deviceSn).c_str(), _userId.length() + _deviceSn.length(), md5_data);
            char hex_data[33];
            for(int i=0; i<16; i++) sprintf(&hex_data[i*2], "%02X", md5_data[i]);
            hex_data[32] = 0;
            std::vector<uint8_t> auth_payload(hex_data, hex_data + 32);
            // Authenticating packet (0x86) requires adaptive logic:
            // - V3 (Delta 3): Requires Version 3, Sequence 0.
            // - V2 (Wave 2): Requires Version 2 (short header), Incrementing Sequence.
            uint8_t auth_version = (_protocolVersion == 2) ? 2 : 3;
            uint32_t auth_seq = (_protocolVersion == 2) ? _txSeq++ : 0;

            Packet auth_pkt(0x21, 0x35, 0x35, 0x86, auth_payload, 0x01, 0x01, auth_version, auth_seq, 0x0d);
            EncPacket enc_auth(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, auth_pkt.toBytes());
            _sendCommand(enc_auth.toBytes(&_crypto));
        }
    } else if (_state == ConnectionState::AUTHENTICATING) {
        if (pkt->getCmdSet() == 0x35 && pkt->getCmdId() == 0x86 && payload.size() > 0 && payload[0] == 0x00) {
            _state = ConnectionState::AUTHENTICATED;
            ESP_LOGI(TAG, "Authentication successful!");
        } else {
            ESP_LOGE(TAG, "Authentication failed!");
        }
    }
}

uint8_t EcoflowESP32::_getDeviceDest() {
    return (_protocolVersion == 2) ? 0x42 : 0x02;
}

//--------------------------------------------------------------------------
//--- Command Sending
//--------------------------------------------------------------------------

bool EcoflowESP32::_sendCommand(const std::vector<uint8_t>& command) {
    if (_pWriteChr && isConnected()) {
        ESP_LOGV(TAG, "Sending %d bytes", command.size());
        _pWriteChr->writeValue(command.data(), command.size(), false); // Write without response
        return true;
    }
    return false;
}

void EcoflowESP32::_sendConfigPacket(const pd335_sys_ConfigWrite& config) {
    if (!isAuthenticated()) return;

    uint8_t buffer[128];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, pd335_sys_ConfigWrite_fields, &config)) {
        ESP_LOGE(TAG, "Failed to encode config protobuf message");
        return;
    }

    std::vector<uint8_t> payload(buffer, buffer + stream.bytes_written);
    Packet packet(0x20, 0x02, 0xFE, 0x11, payload, 0x01, 0x01, _protocolVersion, _txSeq++);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    _sendCommand(enc_packet.toBytes(&_crypto));
}

//--------------------------------------------------------------------------
//--- Getters and Setters
//--------------------------------------------------------------------------

// --- Data Getters ---
int EcoflowESP32::getBatteryLevel() {
    if (_protocolVersion == 2) return _data.wave2.batSoc;
    return (int)_data.delta3.batteryLevel;
}
int EcoflowESP32::getInputPower() {
    if (_protocolVersion == 2) return (_data.wave2.batPwrWatt > 0) ? _data.wave2.batPwrWatt : 0;
    return (int)_data.delta3.inputPower;
}
int EcoflowESP32::getOutputPower() {
    if (_protocolVersion == 2) return (_data.wave2.batPwrWatt < 0) ? abs(_data.wave2.batPwrWatt) : 0;
    return (int)_data.delta3.outputPower;
}
int EcoflowESP32::getBatteryVoltage() { return 0; } // Not available in current data structure
int EcoflowESP32::getACVoltage() { return 0; } // Not available
int EcoflowESP32::getACFrequency() { return 0; } // Not available
int EcoflowESP32::getSolarInputPower() {
    if (_protocolVersion == 2) return _data.wave2.mpptPwrWatt;
    return (int)_data.delta3.solarInputPower;
}
int EcoflowESP32::getAcOutputPower() {
    if (_protocolVersion == 2) return 0; // Wave 2 is typically DC
    return (int)abs(_data.delta3.acOutputPower);
}
int EcoflowESP32::getDcOutputPower() {
    if (_protocolVersion == 2) return _data.wave2.psdrPwrWatt;
    return (int)abs(_data.delta3.dc12vOutputPower);
}
int EcoflowESP32::getCellTemperature() {
    if (_protocolVersion == 2) return (int)_data.wave2.outLetTemp;
    return _data.delta3.cellTemperature;
}
int EcoflowESP32::getAmbientTemperature() {
    if (_protocolVersion == 2) return (int)_data.wave2.envTemp;
    return 0; // Not available for Delta 3
}
int EcoflowESP32::getMaxChgSoc() {
    if (_protocolVersion == 2) return 100; // Not configurable on Wave 2
    return _data.delta3.batteryChargeLimitMax;
}
int EcoflowESP32::getMinDsgSoc() {
    if (_protocolVersion == 2) return 0; // Not configurable on Wave 2
    return _data.delta3.batteryChargeLimitMin;
}
int EcoflowESP32::getAcChgLimit() {
    if (_protocolVersion == 2) return 0; // Not applicable
    return _data.delta3.acChargingSpeed;
}

// --- State Getters ---
bool EcoflowESP32::isAcOn() {
    if (_protocolVersion == 2) return (_data.wave2.mode != 0);
    return _data.delta3.acOn;
}
bool EcoflowESP32::isDcOn() {
    if (_protocolVersion == 2) return (_data.wave2.powerMode != 0);
    return _data.delta3.dcOn;
}
bool EcoflowESP32::isUsbOn() {
    if (_protocolVersion == 2) return false; // Not applicable to Wave 2
    return _data.delta3.usbOn;
}
bool EcoflowESP32::isConnected() { return _state >= ConnectionState::CONNECTED && _state <= ConnectionState::AUTHENTICATED; }
bool EcoflowESP32::isConnecting() { return (_state >= ConnectionState::CREATED && _state < ConnectionState::AUTHENTICATED); }
bool EcoflowESP32::isAuthenticated() { return _state == ConnectionState::AUTHENTICATED; }

// --- Control Setters ---
bool EcoflowESP32::requestData() {
    if (!isAuthenticated()) return false;

    // Determine destination and version based on device type is tricky if we don't track it explicitly in EcoflowESP32
    // But we have _protocolVersion.
    // D3: src=0x02, dest=0x20 (or 0x20->0x02)
    // Wave 2: src=0x21, dest=0x42
    // D3P: src=0x02, dest=0x20
    // AltChg: src=0x14, dest=0x20

    // Default to Delta 3 behavior (0x02) if version 3
    uint8_t dest = 0x02;
    if (_protocolVersion == 2) dest = 0x42;
    // We might need to differentiate D3P and AltChg.
    // Ideally pass DeviceType to begin() or infer from SN.

    if (_deviceSn.rfind("MR51", 0) == 0) dest = 0x02; // D3P
    else if (_deviceSn.rfind("F371", 0) == 0 || _deviceSn.rfind("F372", 0) == 0 || _deviceSn.rfind("DC01", 0) == 0) dest = 0x14; // AltChg

    // For D3/D3P/AltChg, src is 0x20 (app), dest is device ID.
    // Packet(src, dest, cmdSet, cmdId, ...)
    // Wait, Packet constructor: Packet(src, dest, ...)
    // D3P: Packet(0x20, 0x02, 0xFE, 0x11, ...)
    // AltChg: Packet(0x20, 0x14, 0xFE, 0x11, ...)

    // Wait, requestData() sends a request for data.
    // Delta 3: Packet(0x01, 0x02, 0xFE, 0x11...) ?
    // In original code: Packet(0x01, 0x02, 0xFE, 0x11, ...)
    // 0x01 is src? No, src is usually 0x20 or 0x21 (Android/iOS).
    // Original code: Packet(0x01, 0x02, ...)
    // Let's stick to what worked for D3 and Wave 2 first.
    // For Wave 2, it seems handled by not calling this or using V2 logic.

    Packet packet(0x20, dest, 0xFE, 0x11, {}, 0x01, 0x01, _protocolVersion, _txSeq++, 0x0d);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    return _sendCommand(enc_packet.toBytes(&_crypto));
}

bool EcoflowESP32::setAC(bool on) {
    if (_deviceSn.rfind("MR51", 0) == 0) {
        // Delta Pro 3 AC is complicated (HV/LV), but maybe main AC toggle?
        // D3P doesn't seem to have a single AC toggle in python file, it has AC_HV and AC_LV.
        // Assuming setAcHvPort for now or generic if available.
        // Python code: enable_ac_hv_port and enable_ac_lv_port.
        // Let's default to LV for "AC" toggle or ignore if ambiguous.
        // For now, let's use LV.
        return setAcLvPort(on);
    }

    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    config.has_cfg_ac_out_open = true;
    config.cfg_ac_out_open = on;
    _sendConfigPacket(config);
    return true;
}

void EcoflowESP32::_sendConfigPacket(const mr521_ConfigWrite& config) {
    if (!isAuthenticated()) return;

    uint8_t buffer[128];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, mr521_ConfigWrite_fields, &config)) {
        ESP_LOGE(TAG, "Failed to encode MR521 config protobuf message");
        return;
    }

    std::vector<uint8_t> payload(buffer, buffer + stream.bytes_written);
    // D3P: dest 0x02
    Packet packet(0x20, 0x02, 0xFE, 0x11, payload, 0x01, 0x01, _protocolVersion, _txSeq++);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    _sendCommand(enc_packet.toBytes(&_crypto));
}

void EcoflowESP32::_sendConfigPacket(const dc009_apl_comm_ConfigWrite& config) {
    if (!isAuthenticated()) return;

    uint8_t buffer[128];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, dc009_apl_comm_ConfigWrite_fields, &config)) {
        ESP_LOGE(TAG, "Failed to encode DC009 config protobuf message");
        return;
    }

    std::vector<uint8_t> payload(buffer, buffer + stream.bytes_written);
    // AltChg: dest 0x14
    Packet packet(0x20, 0x14, 0xFE, 0x11, payload, 0x01, 0x01, _protocolVersion, _txSeq++);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    _sendCommand(enc_packet.toBytes(&_crypto));
}

//--------------------------------------------------------------------------
//--- Delta Pro 3 Specific Commands
//--------------------------------------------------------------------------

bool EcoflowESP32::setEnergyBackup(bool enabled) {
    mr521_ConfigWrite config = mr521_ConfigWrite_init_zero;
    config.has_cfg_energy_backup = true;
    config.cfg_energy_backup.energy_backup_en = enabled;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setEnergyBackupLevel(int level) {
    mr521_ConfigWrite config = mr521_ConfigWrite_init_zero;
    config.has_cfg_energy_backup = true;
    config.cfg_energy_backup.has_energy_backup_start_soc = true; // Verify if this field exists or is implied
    config.cfg_energy_backup.energy_backup_start_soc = level;
    config.cfg_energy_backup.energy_backup_en = true; // Usually need to enable to set level
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setAcHvPort(bool enabled) {
    mr521_ConfigWrite config = mr521_ConfigWrite_init_zero;
    config.has_cfg_hv_ac_out_open = true;
    config.cfg_hv_ac_out_open = enabled;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setAcLvPort(bool enabled) {
    mr521_ConfigWrite config = mr521_ConfigWrite_init_zero;
    config.has_cfg_lv_ac_out_open = true;
    config.cfg_lv_ac_out_open = enabled;
    _sendConfigPacket(config);
    return true;
}

//--------------------------------------------------------------------------
//--- Alternator Charger Specific Commands
//--------------------------------------------------------------------------

bool EcoflowESP32::setChargerOpen(bool enabled) {
    dc009_apl_comm_ConfigWrite config = dc009_apl_comm_ConfigWrite_init_zero;
    config.has_cfg_sp_charger_chg_open = true;
    config.cfg_sp_charger_chg_open = enabled;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setChargerMode(int mode) {
    dc009_apl_comm_ConfigWrite config = dc009_apl_comm_ConfigWrite_init_zero;
    config.has_cfg_sp_charger_chg_mode = true;
    config.cfg_sp_charger_chg_mode = (dc009_apl_comm_SP_CHARGER_CHG_MODE)mode;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setPowerLimit(int limit) {
    dc009_apl_comm_ConfigWrite config = dc009_apl_comm_ConfigWrite_init_zero;
    config.has_cfg_sp_charger_chg_pow_limit = true;
    config.cfg_sp_charger_chg_pow_limit = limit;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setBatteryVoltage(float voltage) {
    dc009_apl_comm_ConfigWrite config = dc009_apl_comm_ConfigWrite_init_zero;
    config.has_cfg_sp_charger_car_batt_vol_setting = true;
    config.cfg_sp_charger_car_batt_vol_setting = (int)(voltage * 10);
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setCarBatteryChargeLimit(float amps) {
    dc009_apl_comm_ConfigWrite config = dc009_apl_comm_ConfigWrite_init_zero;
    config.has_cfg_sp_charger_car_batt_chg_amp_limit = true;
    config.cfg_sp_charger_car_batt_chg_amp_limit = amps;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setDeviceBatteryChargeLimit(float amps) {
    dc009_apl_comm_ConfigWrite config = dc009_apl_comm_ConfigWrite_init_zero;
    config.has_cfg_sp_charger_dev_batt_chg_amp_limit = true;
    config.cfg_sp_charger_dev_batt_chg_amp_limit = amps;
    _sendConfigPacket(config);
    return true;
}

//--------------------------------------------------------------------------
//--- Wave 2 Specific Commands
//--------------------------------------------------------------------------

bool EcoflowESP32::_sendWave2Command(uint8_t cmdId, const std::vector<uint8_t>& payload) {
    if (!isAuthenticated()) return false;

    // Wave 2 uses Protocol V2: src=0x21, dest=0x42, cmdSet=0x42
    Packet packet(0x21, 0x42, 0x42, cmdId, payload, 0x01, 0x01, _protocolVersion, _txSeq++);

    // For V2, we need to construct the packet carefully.
    // The EcoflowProtocol::Packet::toBytes method handles V2 logic if _version is 2.
    // However, EcoflowESP32::_protocolVersion handles this.

    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    return _sendCommand(enc_packet.toBytes(&_crypto));
}

void EcoflowESP32::setAmbientLight(uint8_t status) {
    _sendWave2Command(0x5C, {status});
}

void EcoflowESP32::setAutomaticDrain(uint8_t enable) {
    _sendWave2Command(0x59, {enable});
}

void EcoflowESP32::setBeep(uint8_t on) {
    _sendWave2Command(0x56, {on});
}

void EcoflowESP32::setFanSpeed(uint8_t speed) {
    _sendWave2Command(0x5E, {speed});
}

void EcoflowESP32::setMainMode(uint8_t mode) {
    _sendWave2Command(0x51, {mode});
}

void EcoflowESP32::setPowerState(uint8_t on) {
    _sendWave2Command(0x5B, {on});
}

void EcoflowESP32::setTemperature(uint8_t temp) {
    _sendWave2Command(0x58, {temp});
}

void EcoflowESP32::setCountdownTimer(uint8_t status) {
    // User specified: [0x00, 0x00, status]
    _sendWave2Command(0x55, {0x00, 0x00, status});
}

void EcoflowESP32::setIdleScreenTimeout(uint8_t time) {
    // User specified: [0x00, 0x00, time]
    _sendWave2Command(0x54, {0x00, 0x00, time});
}

void EcoflowESP32::setSubMode(uint8_t sub_mode) {
    _sendWave2Command(0x52, {sub_mode});
}

void EcoflowESP32::setTempDisplayType(uint8_t type) {
    _sendWave2Command(0x5D, {type});
}

void EcoflowESP32::setTempUnit(uint8_t unit) {
    _sendWave2Command(0x53, {unit});
}

bool EcoflowESP32::setDC(bool on) {
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    config.has_cfg_dc_12v_out_open = true;
    config.cfg_dc_12v_out_open = on;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setUSB(bool on) {
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    config.has_cfg_usb_open = true;
    config.cfg_usb_open = on;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setAcChargingLimit(int watts) {
    if (watts < 200 || watts > 2900) {
        ESP_LOGW(TAG, "AC Charging limit %d W out of range", watts);
    }
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    config.has_cfg_ac_in_chg_mode = true;
    config.cfg_ac_in_chg_mode = pd335_sys_AC_IN_CHG_MODE_AC_IN_CHG_MODE_SELF_DEF_POW;
    config.has_cfg_plug_in_info_ac_in_chg_pow_max = true;
    config.cfg_plug_in_info_ac_in_chg_pow_max = watts;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setBatterySOCLimits(int maxChg, int minDsg) {
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    if (maxChg >= 50 && maxChg <= 100) {
        config.has_cfg_max_chg_soc = true;
        config.cfg_max_chg_soc = maxChg;
    }
    if (minDsg >= 0 && minDsg <= 30) {
        config.has_cfg_min_dsg_soc = true;
        config.cfg_min_dsg_soc = minDsg;
    }
    _sendConfigPacket(config);
    return true;
}
