/**
 * @file EcoflowCrypto.h
 * @author Lollokara
 * @brief Handles all cryptographic operations for the EcoFlow BLE protocol.
 *
 * This file defines the EcoflowCrypto class, which is responsible for:
 * - Elliptic Curve Diffie-Hellman (ECDH) key generation and shared secret computation.
 * - AES-128-CBC encryption and decryption for session communication.
 * - Derivation of session keys and initialization vectors (IV).
 */

#ifndef ECOFLOW_CRYPTO_H
#define ECOFLOW_CRYPTO_H

#include <stdint.h>
#include "mbedtls/ecdh.h"
#include "mbedtls/md5.h"
#include "mbedtls/aes.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include <Arduino.h>

/**
 * @class EcoflowCrypto
 * @brief Manages cryptographic functions for secure communication.
 *
 * This class encapsulates all the necessary mbedTLS logic for the EcoFlow
 * authentication handshake and for encrypting/decrypting data packets
 * during a session.
 */
class EcoflowCrypto {
public:
    EcoflowCrypto();
    ~EcoflowCrypto();

    /**
     * @brief Generates a new ECDH public/private key pair.
     * @return True on success, false on failure.
     */
    bool generate_keys();

    /**
     * @brief Computes the shared secret using our private key and the device's public key.
     * @param peer_pub_key A pointer to the peer's public key.
     * @param peer_pub_key_len The length of the peer's public key.
     * @return True on success, false on failure.
     */
    bool compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_pub_key_len);

    /**
     * @brief Generates the final AES session key from a seed and random data provided by the device.
     * @param seed A pointer to the seed value.
     * @param srand A pointer to the random data.
     */
    void generate_session_key(const uint8_t* seed, const uint8_t* srand);

    /**
     * @brief Encrypts data using the established AES session key.
     * @param input The plaintext data to encrypt.
     * @param input_len The length of the plaintext data.
     * @param output A buffer to store the encrypted ciphertext.
     */
    void encrypt_session(const uint8_t* input, size_t input_len, uint8_t* output);

    /**
     * @brief Decrypts data using the established AES session key.
     * @param input The ciphertext to decrypt.
     * @param input_len The length of the ciphertext.
     * @param output A buffer to store the decrypted plaintext.
     */
    void decrypt_session(const uint8_t* input, size_t input_len, uint8_t* output);

    /**
     * @brief Decrypts the session key response from the device, which is uniquely
     *        encrypted with the temporary shared secret.
     * @param input The encrypted session key data.
     * @param input_len The length of the encrypted data.
     * @param output A buffer to store the decrypted data.
     */
    void decrypt_shared(const uint8_t* input, size_t input_len, uint8_t* output);

    // --- Getters for internal state (used by other library components) ---
    uint8_t* get_public_key() { return public_key; }
    size_t get_public_key_len() { return sizeof(public_key); }

private:
    mbedtls_ecp_group grp;
    mbedtls_mpi d; // Our private key
    mbedtls_ecp_point Q; // Our public key point
    mbedtls_aes_context aes_ctx;

    uint8_t public_key[40];    // Our public key (X and Y coordinates)
    uint8_t shared_secret[20]; // ECDH shared secret (X-coordinate of shared point)
    uint8_t session_key[16];   // Final AES session key
    uint8_t iv[16];            // AES Initialization Vector

    /**
     * @brief A static wrapper for the ESP32 hardware random number generator to be
     *        used by mbedTLS.
     */
    static int f_rng(void* p_rng, unsigned char* output, size_t output_len);
};

#endif // ECOFLOW_CRYPTO_H
