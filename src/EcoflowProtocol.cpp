/**
 * @file EcoflowProtocol.cpp
 * @author Lollokara
 * @brief Implementation of the EcoFlow BLE protocol packet structures.
 *
 * This file contains the serialization and deserialization logic for the
 * `Packet` and `EncPacket` classes, which are used to construct and parse

 * messages sent to and from the EcoFlow device.
 */

#include "EcoflowProtocol.h"
#include <Arduino.h>
#include <cstring>
#include "esp_log.h"

static const char* TAG = "EcoflowProtocol";

// Constants defined in the header
const uint8_t Packet::PREFIX;
const uint16_t EncPacket::PREFIX;

//--------------------------------------------------------------------------
//--- CRC Helper Functions
//--------------------------------------------------------------------------

/**
 * @brief Calculates the CRC-16/MODBUS for a given data buffer.
 * @param data Pointer to the data.
 * @param len Length of the data.
 * @return The 16-bit CRC value.
 */
static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Calculates the CRC-8 for a given data buffer.
 * Used for validating the header of an inner `Packet`.
 * @param data Pointer to the data.
 * @param len Length of the data.
 * @return The 8-bit CRC value.
 */
static uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

//--------------------------------------------------------------------------
//--- Packet Class Implementation
//--------------------------------------------------------------------------

Packet::Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type, uint8_t encrypted, uint8_t version, uint32_t seq, uint8_t product_id) :
    _src(src), _dest(dest), _cmdSet(cmdSet), _cmdId(cmdId), _payload(payload), _check_type(check_type), _encrypted(encrypted), _version(version), _seq(seq), _product_id(product_id) {
}

Packet* Packet::fromBytes(const uint8_t* data, size_t len, bool is_xor) {
    if (len < 16 || data[0] != PREFIX) {
        return nullptr;
    }

    uint8_t version = data[1];
    uint16_t payload_len = data[2] | (data[3] << 8);

    if (crc8(data, 4) != data[4]) {
        ESP_LOGE(TAG, "Packet header CRC8 mismatch");
        return nullptr;
    }

    uint8_t product_id = data[5];
    uint32_t seq = data[6] | (data[7] << 8) | (data[8] << 16) | (data[9] << 24);
    uint8_t src = data[12];
    uint8_t dest = data[13];

    uint8_t dsrc = 0, ddest = 0, cmd_set = 0, cmd_id = 0;
    size_t payload_offset = 0;

    if (version == 3 || version == 19) { // V3 protocol
        if (len < 18) return nullptr;
        dsrc = data[14];
        ddest = data[15];
        cmd_set = data[16];
        cmd_id = data[17];
        payload_offset = 18;
    } else if (version == 2) { // V2 protocol
        if (len < 16) return nullptr;
        cmd_set = data[14];
        cmd_id = data[15];
        payload_offset = 16;
    } else {
        ESP_LOGE(TAG, "Unsupported packet version %d", version);
        return nullptr;
    }

    std::vector<uint8_t> payload;
    if (payload_len > 0 && (len >= payload_offset + payload_len)) {
        payload.assign(data + payload_offset, data + payload_offset + payload_len);
        if (is_xor) {
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] ^= data[6]; // Deobfuscate using the sequence number byte
            }
        }
    }

    return new Packet(src, dest, cmd_set, cmd_id, payload, dsrc, ddest, version, seq, product_id);
}

std::vector<uint8_t> Packet::toBytes() const {
    std::vector<uint8_t> bytes;
    bytes.push_back(PREFIX);
    bytes.push_back(_version);
    bytes.push_back(_payload.size() & 0xFF);
    bytes.push_back((_payload.size() >> 8) & 0xFF);
    bytes.push_back(crc8(bytes.data(), bytes.size())); // Header CRC

    bytes.push_back(_product_id);
    bytes.insert(bytes.end(), {(uint8_t)(_seq & 0xFF), (uint8_t)((_seq >> 8) & 0xFF), (uint8_t)((_seq >> 16) & 0xFF), (uint8_t)((_seq >> 24) & 0xFF)});
    bytes.push_back(0); // Reserved
    bytes.push_back(0); // Reserved
    bytes.push_back(_src);
    bytes.push_back(_dest);

    if (_version == 3) {
        bytes.push_back(_check_type);
        bytes.push_back(_encrypted);
    }

    bytes.push_back(_cmdSet);
    bytes.push_back(_cmdId);
    bytes.insert(bytes.end(), _payload.begin(), _payload.end());

    uint16_t crc = crc16(bytes.data(), bytes.size());
    bytes.push_back(crc & 0xFF);
    bytes.push_back((crc >> 8) & 0xFF);

    return bytes;
}

//--------------------------------------------------------------------------
//--- EncPacket Class Implementation
//--------------------------------------------------------------------------

EncPacket::EncPacket(uint8_t frame_type, uint8_t payload_type, const std::vector<uint8_t>& payload, uint8_t needs_ack, uint8_t is_ack)
    : _frame_type(frame_type), _payload_type(payload_type), _payload(payload), _needs_ack(needs_ack), _is_ack(is_ack) {}

std::vector<uint8_t> EncPacket::toBytes(EcoflowCrypto* crypto) const {
    std::vector<uint8_t> processed_payload = _payload;
    if (crypto) {
        // Apply PKCS7 padding before encryption
        int padding = 16 - (_payload.size() % 16);
        std::vector<uint8_t> padded_payload = _payload;
        padded_payload.insert(padded_payload.end(), padding, padding);

        processed_payload.resize(padded_payload.size());
        crypto->encrypt_session(padded_payload.data(), padded_payload.size(), processed_payload.data());
    }

    std::vector<uint8_t> packet_data;
    packet_data.push_back(PREFIX & 0xFF);
    packet_data.push_back((PREFIX >> 8) & 0xFF);
    packet_data.push_back((_frame_type << 4) | _payload_type);
    packet_data.push_back(0x01); // Protocol version/hardcoded value

    uint16_t len = processed_payload.size() + 2; // Payload + 2 bytes for CRC
    packet_data.push_back(len & 0xFF);
    packet_data.push_back((len >> 8) & 0xFF);

    packet_data.insert(packet_data.end(), processed_payload.begin(), processed_payload.end());

    uint16_t crc = crc16(packet_data.data(), packet_data.size());
    packet_data.push_back(crc & 0xFF);
    packet_data.push_back((crc >> 8) & 0xFF);

    return packet_data;
}

std::vector<Packet> EncPacket::parsePackets(const uint8_t* data, size_t len, EcoflowCrypto& crypto, std::vector<uint8_t>& rxBuffer, bool isAuthenticated) {
    std::vector<Packet> packets;
    rxBuffer.insert(rxBuffer.end(), data, data + len);

    while (rxBuffer.size() >= 8) { // Minimum size for an EncPacket header + CRC
        if (rxBuffer[0] != (PREFIX & 0xFF) || rxBuffer[1] != ((PREFIX >> 8) & 0xFF)) {
            rxBuffer.erase(rxBuffer.begin());
            continue;
        }

        uint16_t frame_len = rxBuffer[4] | (rxBuffer[5] << 8);
        size_t total_len = 6 + frame_len;
        if (rxBuffer.size() < total_len) {
            break; // Not enough data yet
        }

        uint16_t crc_from_packet = rxBuffer[total_len - 2] | (rxBuffer[total_len - 1] << 8);
        if (crc_from_packet != crc16(rxBuffer.data(), total_len - 2)) {
            rxBuffer.erase(rxBuffer.begin());
            continue;
        }

        size_t payload_len = frame_len - 2;
        std::vector<uint8_t> raw_payload(rxBuffer.begin() + 6, rxBuffer.begin() + 6 + payload_len);
        std::vector<uint8_t> processed_payload;

        if(isAuthenticated) {
            processed_payload.resize(raw_payload.size());
            crypto.decrypt_session(raw_payload.data(), raw_payload.size(), processed_payload.data());

            // Remove PKCS7 padding
            if (!processed_payload.empty()) {
                uint8_t padding = processed_payload.back();
                if (padding > 0 && padding <= 16 && processed_payload.size() >= padding) {
                    processed_payload.resize(processed_payload.size() - padding);
                }
            }
        } else {
            processed_payload = raw_payload;
        }

        Packet* packet = Packet::fromBytes(processed_payload.data(), processed_payload.size(), true); // is_xor=true for Delta 3
        if (packet) {
            packets.push_back(*packet);
            delete packet;
        }

        rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + total_len);
    }
    return packets;
}

std::vector<uint8_t> EncPacket::parseSimple(const uint8_t* data, size_t len) {
    if (len < 8 || (data[0] | (data[1] << 8)) != PREFIX) {
        return {};
    }

    uint16_t len_from_packet = data[4] | (data[5] << 8);
    size_t frame_end = 6 + len_from_packet;

    if (len < frame_end || len_from_packet < 2) {
        return {};
    }

    uint16_t calculated_crc = crc16(data, frame_end - 2);
    uint16_t received_crc = data[frame_end - 2] | (data[frame_end - 1] << 8);

    if (calculated_crc != received_crc) {
        return {};
    }

    return std::vector<uint8_t>(data + 6, data + frame_end - 2);
}
