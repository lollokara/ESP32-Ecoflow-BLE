/**
 * @file EcoflowProtocol.h
 * @author Lollokara
 * @brief Defines the packet structures for the EcoFlow BLE protocol.
 *
 * This file contains the definitions for the two main packet types used in
 * EcoFlow communication:
 * 1. `Packet`: The inner, unencrypted packet structure containing the command and payload.
 * 2. `EncPacket`: The outer, encrypted packet structure that wraps an inner `Packet`.
 */

#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <Arduino.h>
#include <vector>
#include <string>
#include "EcoflowCrypto.h"

/**
 * @class Packet
 * @brief Represents the inner, unencrypted data packet.
 *
 * This class handles the serialization and deserialization of the core data
 * packets that contain the actual commands and payloads sent to and from the
 * EcoFlow device.
 */
class Packet {
public:
    static const uint8_t PREFIX = 0xAA;

    Packet(uint8_t src, uint8_t dest, uint8_t cmdSet, uint8_t cmdId, const std::vector<uint8_t>& payload, uint8_t check_type = 0x01, uint8_t encrypted = 0x01, uint8_t version = 0x03, uint32_t seq = 0, uint8_t product_id = 0x0d);

    /**
     * @brief Deserializes a byte array into a Packet object.
     * @param data The raw byte data.
     * @param len The length of the data.
     * @param is_xor Whether to apply XOR deobfuscation (for certain device models).
     * @return A pointer to a new Packet object, or nullptr if parsing fails.
     */
    static Packet* fromBytes(const uint8_t* data, size_t len, bool is_xor = false);

    /**
     * @brief Serializes the Packet object into a byte vector.
     * @return A vector of bytes ready to be sent.
     */
    std::vector<uint8_t> toBytes() const;
    
    // --- Getters ---
    uint8_t getCmdId() const { return _cmdId; }
    const std::vector<uint8_t>& getPayload() const { return _payload; }
    uint8_t getSrc() const { return _src; }
    uint8_t getDest() const { return _dest; }
    uint8_t getCmdSet() const { return _cmdSet; }
    uint32_t getSeq() const { return _seq; }
    uint8_t getVersion() const { return _version; }

private:
    uint8_t _src;
    uint8_t _dest;
    uint8_t _cmdSet;
    uint8_t _cmdId;
    std::vector<uint8_t> _payload;
    uint8_t _check_type;
    uint8_t _encrypted;
    uint8_t _version;
    uint32_t _seq;
    uint8_t _product_id;
};

/**
 * @class EncPacket
 * @brief Represents the outer, encrypted packet that wraps an inner Packet.
 *
 * This class handles the framing, encryption, and decryption of the outer
 * packets that are sent over BLE. It is responsible for assembling the final
 * BLE payload and parsing incoming raw data.
 */
class EncPacket {
public:
    static const uint16_t PREFIX = 0x5A5A;
    static const uint8_t FRAME_TYPE_COMMAND = 0x00;
    static const uint8_t FRAME_TYPE_PROTOCOL = 0x01;
    static const uint8_t PAYLOAD_TYPE_VX_PROTOCOL = 0x00;

    EncPacket(uint8_t frame_type, uint8_t payload_type, const std::vector<uint8_t>& payload,
              uint8_t needs_ack = 0, uint8_t is_ack = 0);

    const std::vector<uint8_t>& getPayload() const { return _payload; }

    /**
     * @brief Serializes the EncPacket into a byte vector, optionally encrypting it.
     * @param crypto A pointer to an EcoflowCrypto instance for encryption. If null, the payload is not encrypted.
     * @return A vector of bytes ready to be sent over BLE.
     */
    std::vector<uint8_t> toBytes(EcoflowCrypto* crypto = nullptr) const;

    /**
     * @brief Parses raw BLE data, handling fragmentation and decryption, to extract inner Packets.
     * @param data The incoming raw byte data.
     * @param len The length of the data.
     * @param crypto The EcoflowCrypto instance for decryption.
     * @param rxBuffer A buffer to handle fragmented packets across multiple notifications.
     * @param isAuthenticated True if the connection is authenticated, which determines if decryption is needed.
     * @return A vector of fully parsed inner Packet objects.
     */
    static std::vector<Packet> parsePackets(const uint8_t* data, size_t len, EcoflowCrypto& crypto, std::vector<uint8_t>& rxBuffer, bool isAuthenticated = false);

    /**
     * @brief A simplified parser for unencrypted, non-fragmented packets used during the auth handshake.
     * @param data The incoming raw byte data.
     * @param len The length of the data.
     * @return The extracted payload as a byte vector.
     */
    static std::vector<uint8_t> parseSimple(const uint8_t* data, size_t len);

private:
    uint8_t _frame_type;
    uint8_t _payload_type;
    std::vector<uint8_t> _payload;
    uint8_t _needs_ack;
    uint8_t _is_ack;
};

#endif // ECOFLOW_PROTOCOL_H
