#pragma once

#include "System/Session/UDP/IKCPAdapter.h"
#include "System/ILog.h"
#include "System/Pch.h"

#include <ikcp.h>

namespace System {

class KCPAdapter : public IKCPAdapter
{
public:
    KCPAdapter(uint32_t conv);
    ~KCPAdapter() override;

    int Send(const void *data, int length) override;
    int Input(const void *data, int length) override;
    void Update(uint32_t current) override;
    int Output(uint8_t *buffer, int maxSize) override;
    int Recv(uint8_t *buffer, int maxSize) override;

private:
    void *_kcp;
};

} // namespace System
