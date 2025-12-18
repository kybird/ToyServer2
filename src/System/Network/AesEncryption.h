#pragma once
#include "System/Network/IPacketEncryption.h"
#include <array>
#include <vector>


namespace System {

// AES-128 CBC Encryption.
// Implemented as standalone (TinyAES logic) to avoid OpenSSL dependency.
class AesEncryption : public IPacketEncryption
{
public:
    // Key must be 16 bytes. IV must be 16 bytes.
    AesEncryption(const std::vector<uint8_t> &key, const std::vector<uint8_t> &iv);

    // Padding Note:
    // This low-level function expects 'length' to be a multiple of 16 (Block Size).
    // The Protocol layer handles padding if necessary (though our packets are variable length,
    // we assume the upper layer pads or we use text stealing - for now we strictly assume block alignment or padding
    // handled by caller).
    // * However, for simplicity in this ToyServer, we will implement PKCS7 padding internally if dest buffer is large
    // enough,
    // * BUT 'IPacketEncryption' contract says 'Encrypt length bytes'.
    // * So we assume the INPUT is already padded to 16 bytes, OR we use streaming mode (CFB/OFB) which we won't.
    // * DECISION: We enforce that packet body is padded to 16 bytes by the Caller (Session/Serializer) OR we use CTS.
    // * FOR SIMPLICITY: We will assume standard CBC and the caller ensures length % 16 == 0.

    void Encrypt(const uint8_t *src, uint8_t *dest, size_t length) override;
    void Decrypt(const uint8_t *src, uint8_t *dest, size_t length) override;

private:
    std::array<uint8_t, 16> _roundKey[11]; // AES-128 has 10 rounds + initial
    std::array<uint8_t, 16> _iv;

    void KeyExpansion(const uint8_t *key);
    void EncryptBlock(const uint8_t *in, uint8_t *out);
    void DecryptBlock(const uint8_t *in, uint8_t *out);
};

} // namespace System
