#pragma once

#include "System/ILog.h"
#include "System/Pch.h"
#include "System/Session/UDP/IKCPAdapter.h"

#include <ikcp.h>

namespace System {

class KCPAdapter : public IKCPAdapter
{
public:
    KCPAdapter(uint32_t conv);
    ~KCPAdapter() override;

    void SetOutputCallback(std::function<int(const char *, int)> callback) override;

    int Send(const void *data, int length) override;
    int Input(const void *data, int length) override;
    void Update(uint32_t current) override;
    int Output(uint8_t *buffer, int maxSize) override;
    int Recv(uint8_t *buffer, int maxSize) override;

    // Internal callback entry
    int RecvOutput(const char *buf, int len);

private:
    void *_kcp;
    std::function<int(const char *, int)> _outputCallback;
};

} // namespace System
