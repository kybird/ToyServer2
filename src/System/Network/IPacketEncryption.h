#pragma once
#include <cstddef>
#include <cstdint>


namespace System {

// Interface for Packet Payload Encryption.
// Implementations must be thread-safe if shared, or instantiated per Session.
// (Currently designed to be instantiated Per-Session for unique keys/IVs).
struct IPacketEncryption
{
    virtual ~IPacketEncryption() = default;

    // Encrypts 'length' bytes from src to dest.
    // src and dest MAY be the same pointer (In-place encryption support required).
    // The implementation MUST NOT change the data length (Block cipher padding should be handled by protocol or
    // pre-allocated).
    virtual void Encrypt(const uint8_t *src, uint8_t *dest, size_t length) = 0;

    // Decrypts 'length' bytes from src to dest.
    // src and dest MAY be the same pointer.
    virtual void Decrypt(const uint8_t *src, uint8_t *dest, size_t length) = 0;
};

} // namespace System
