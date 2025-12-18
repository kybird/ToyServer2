#pragma once
#include "System/Network/IPacketEncryption.h"
#include <vector>

namespace System {

// Simple XOR-CBC Encryption.
// Fast, Hardware-friendly, Obfuscates patterns.
class XorEncryption : public IPacketEncryption
{
public:
    XorEncryption(uint8_t key = 0xA5) : _key(key)
    {
    }

    void Encrypt(const uint8_t *src, uint8_t *dest, size_t length) override
    {
        uint8_t key = _key;
        for (size_t i = 0; i < length; ++i)
        {
            uint8_t plain = src[i];
            uint8_t cipher = plain ^ key;
            dest[i] = cipher;
            key = cipher; // Feedback
        }
    }

    void Decrypt(const uint8_t *src, uint8_t *dest, size_t length) override
    {
        uint8_t key = _key;
        for (size_t i = 0; i < length; ++i)
        {
            uint8_t cipher = src[i];
            uint8_t plain = cipher ^ key;
            dest[i] = plain;
            key = cipher; // Feedback
        }
    }

private:
    uint8_t _key;
};

} // namespace System
