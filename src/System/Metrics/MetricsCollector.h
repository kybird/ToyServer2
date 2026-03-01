#pragma once

#include "System/Metrics/IMetrics.h"
#include <map>
#include <mutex>

namespace System {

class MetricImpl : public IMetric
{
};

class CounterImpl : public Counter
{
public:
    void Increment(uint64_t value = 1) override
    {
        _value.fetch_add(value, std::memory_order_relaxed);
    }
    uint64_t GetValue() const override
    {
        return _value.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> _value{0};
};

class GaugeImpl : public Gauge
{
public:
    void Set(int64_t value) override
    {
        _value.store(value, std::memory_order_relaxed);
    }
    int64_t GetValue() const override
    {
        return _value.load(std::memory_order_relaxed);
    }

private:
    std::atomic<int64_t> _value{0};
};

class MetricsCollector : public IMetrics
{
public:
    MetricsCollector();
    virtual ~MetricsCollector() override;

    std::shared_ptr<Counter> GetCounter(const std::string &name) override;
    std::shared_ptr<Gauge> GetGauge(const std::string &name) override;

    void LogMetrics() override;
    std::string ToJson() override;

private:
    std::mutex _mutex;
    std::map<std::string, std::shared_ptr<IMetric>> _registry;

    // Cached Common Metrics for Performance
    std::shared_ptr<Counter> _acceptCounter;
    std::shared_ptr<Counter> _packetCounter;
    std::shared_ptr<Counter> _jobCounter;
};

} // namespace System
