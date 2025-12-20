#pragma once

#include <algorithm>
#include <chrono>

namespace System {

class RateLimiter
{
public:
    RateLimiter(double rate = 100.0, double burst = 200.0) : _refillRate(rate), _capacity(burst), _tokens(burst)
    {
        _lastRefillTime = std::chrono::steady_clock::now();
    }

    // Called on the Hot Path! Must be extremely fast.
    bool TryConsume(double amount = 1.0)
    {
        auto now = std::chrono::steady_clock::now();
        // Calculate duration in seconds (double)
        std::chrono::duration<double> delta = now - _lastRefillTime;

        // Refill tokens (Lazy Calculation)
        double newTokens = delta.count() * _refillRate;
        if (newTokens > 0)
        {
            _tokens = std::min(_capacity, _tokens + newTokens);
            _lastRefillTime = now;
        }

        if (_tokens >= amount)
        {
            _tokens -= amount;
            return true;
        }
        return false;
    }

    void UpdateConfig(double rate, double burst)
    {
        _refillRate = rate;
        _capacity = burst;
        // Optional: Clamp tokens? No, let them drain naturally or stay high.
        if (_tokens > _capacity)
            _tokens = _capacity;
    }

private:
    double _refillRate;
    double _capacity;
    double _tokens; // Not atomic, assuming Session runs in a Strand
    std::chrono::steady_clock::time_point _lastRefillTime;
};

} // namespace System
