#pragma once

#include <memory>
#include <string>

namespace System {

// Metric Types
class IMetric
{
public:
    virtual ~IMetric() = default;
};

class Counter : public IMetric
{
public:
    virtual void Increment(uint64_t value = 1) = 0;
    virtual uint64_t GetValue() const = 0;
};

class Gauge : public IMetric
{
public:
    virtual void Set(int64_t value) = 0;
    virtual int64_t GetValue() const = 0;
};

// Interface
class IMetrics
{
public:
    virtual ~IMetrics() = default;

    // Registration / Access
    virtual std::shared_ptr<Counter> GetCounter(const std::string &name) = 0;
    virtual std::shared_ptr<Gauge> GetGauge(const std::string &name) = 0;

    virtual void LogMetrics() = 0;
    virtual std::string ToJson() = 0;
};

// Singleton Accessor (To replace GetMonitor)
IMetrics &GetMetrics();

} // namespace System
