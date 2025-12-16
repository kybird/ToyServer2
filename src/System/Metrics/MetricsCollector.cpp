#include "System/Metrics/MetricsCollector.h"
#include "System/ILog.h"
#include "System/Pch.h"

namespace System {

MetricsCollector::MetricsCollector()
{
    _acceptCounter = GetCounter("server_accepts");
    _packetCounter = GetCounter("server_packets_total");
    _jobCounter = GetCounter("server_jobs_total");
}

MetricsCollector::~MetricsCollector()
{
}

std::shared_ptr<Counter> MetricsCollector::GetCounter(const std::string &name)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_registry.find(name) == _registry.end())
    {
        _registry[name] = std::make_shared<CounterImpl>();
    }
    // Dynamic cast check could be added here for safety
    return std::static_pointer_cast<Counter>(_registry[name]);
}

std::shared_ptr<Gauge> MetricsCollector::GetGauge(const std::string &name)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_registry.find(name) == _registry.end())
    {
        _registry[name] = std::make_shared<GaugeImpl>();
    }
    return std::static_pointer_cast<Gauge>(_registry[name]);
}

void MetricsCollector::RecordAccept()
{
    if (_acceptCounter)
        _acceptCounter->Increment();
}

void MetricsCollector::RecordPacket(uint32_t count)
{
    if (_packetCounter)
        _packetCounter->Increment(count);
}

void MetricsCollector::RecordJob()
{
    if (_jobCounter)
        _jobCounter->Increment();
}

void MetricsCollector::LogMetrics()
{
    // Basic Logging for now
    uint64_t accepts = _acceptCounter ? _acceptCounter->GetValue() : 0;
    uint64_t packets = _packetCounter ? _packetCounter->GetValue() : 0;
    uint64_t jobs = _jobCounter ? _jobCounter->GetValue() : 0;

    LOG_INFO("[Metrics] Accepts: {}, PPS/Total: {}, Jobs: {}", accepts, packets, jobs);
}

// Global Accessor Implementation
IMetrics &GetMetrics()
{
    static MetricsCollector instance;
    return instance;
}

} // namespace System
