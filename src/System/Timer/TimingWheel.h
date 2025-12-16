#pragma once
#include "System/ITimer.h"
#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <vector>

namespace System {

class TimingWheel
{
public:
    // Linux Kernel Style Constants
    static constexpr int TVR_BITS = 8;
    static constexpr int TVN_BITS = 6;
    static constexpr int TVR_SIZE = 1 << TVR_BITS; // 256
    static constexpr int TVN_SIZE = 1 << TVN_BITS; // 64
    static constexpr int TVR_MASK = TVR_SIZE - 1;
    static constexpr int TVN_MASK = TVN_SIZE - 1;

    struct Node
    {
        uint64_t id;
        uint32_t logicTimerId;
        void *pParam;

        uint32_t expiryTick;   // Absolute Tick
        uint32_t intervalTick; // 0 if one-shot

        ITimerListener *rawListener;
        std::weak_ptr<ITimerListener> weakListener;
        bool useWeak;
        bool cancelled = false; // Added for soft delete

        // Intrusive List support could be added here for zero-alloc
    };

    TimingWheel() : _currentTick(0)
    {
    }

    void Add(std::shared_ptr<Node> node)
    {
        uint32_t expires = node->expiryTick;
        uint32_t idx = expires - _currentTick;

        if (idx < TVR_SIZE)
        {
            // Level 1: 0 - 255 ticks (0 - 2.55s)
            size_t i = expires & TVR_MASK;
            _tv1[i].push_back(node);
        }
        else if (idx < (1 << (TVR_BITS + TVN_BITS)))
        {
            // Level 2: 256 - 16383 ticks (2.56s - 163.8s)
            size_t i = (expires >> TVR_BITS) & TVN_MASK;
            _tv2[i].push_back(node);
        }
        else if (idx < (1 << (TVR_BITS + 2 * TVN_BITS)))
        {
            // Level 3: 16k - 1M ticks (2.7m - 2.9h)
            size_t i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
            _tv3[i].push_back(node);
        }
        else if (idx < (1 << (TVR_BITS + 3 * TVN_BITS)))
        {
            // Level 4: 1M - 67M ticks (2.9h - 7.6d)
            size_t i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
            _tv4[i].push_back(node);
        }
        else if ((int64_t)idx < 0)
        {
            // Already expired? (Negative logic due to uint32 wraparound handling if careful, but here using simple
            // subtractions) If expiryTick < currentTick, treat as immediate (next slot)
            _tv1[(_currentTick + 1) & TVR_MASK].push_back(node);
        }
        else
        {
            // Level 5: > 67M ticks
            size_t i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
            _tv5[i].push_back(node);
        }
    }

    // Returns expired nodes
    void Advance(uint32_t currentTick, std::vector<std::shared_ptr<Node>> &outExpired)
    {
        _currentTick = currentTick;

        // 1. Process Level 1 current slot
        size_t index = _currentTick & TVR_MASK;

        // Cascade if index == 0 (Wrapped Level 1)
        if (index == 0)
        {
            // Cascade Level 2
            size_t i2 = (_currentTick >> TVR_BITS) & TVN_MASK;
            Cascade(_tv2, i2);

            if (i2 == 0)
            {
                // Cascade Level 3
                size_t i3 = (_currentTick >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
                Cascade(_tv3, i3);

                if (i3 == 0)
                {
                    // Cascade Level 4
                    size_t i4 = (_currentTick >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
                    Cascade(_tv4, i4);

                    if (i4 == 0)
                    {
                        // Cascade Level 5
                        size_t i5 = (_currentTick >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
                        Cascade(_tv5, i5);
                    }
                }
            }
        }

        // Now pop everything in Level 1 bucket
        if (!_tv1[index].empty())
        {
            // Transfer to output
            // Filter out cancelled nodes
            for (auto it = _tv1[index].begin(); it != _tv1[index].end();)
            {
                if (!(*it)->cancelled)
                {
                    outExpired.push_back(*it);
                    it = _tv1[index].erase(it);
                }
                else
                {
                    it = _tv1[index].erase(it); // Remove cancelled node
                }
            }
        }
    }

    void Remove(std::shared_ptr<Node> node)
    {
        // Search is still needed if we don't know the exact bucket.
        // But we can calculate the bucket from node->expiryTick!
        // This makes removal O(L) where L is average bucket load.
        // With millions of timers, we might want O(1) removal via iterator storage.
        // For now, calculating bucket is fast.

        uint32_t expires = node->expiryTick;
        uint32_t idx = expires - _currentTick; // NOTE: This idx changes as currentTick advances!
        // !CRITICAL ISSUE!: We CANNOT easily calculate the *current* bucket of a node
        // without knowing when it was added? No, bucket depends on Expiry, not Addition.
        // Hierarchy bucket depends on (Expiry - Current).
        // Wait, (Expiry >> Bits) & Mask IS the bucket index.
        // IT DOES NOT DEPEND ON CURRENT TICK for the upper levels (absolute indexing).
        // Let's re-verify Linux Kernel scheme.
        // Level 1 is index = expiry & MASK. Absolute.
        // Level 2 is index = (expiry >> 8) & MASK. Absolute.
        // Yes, the buckets are absolute time slots!
        // It's just that we only Look at them relative to cascading.
        // So we CAN calculate exact bucket.

        // Correction: The `Add` logic used `idx = expires - currentTick` to decide *Level*.
        // If we want to find it later, we need to know which Level it is in.
        // Since `currentTick` moved, the `expires - currentTick` is different now.
        // The node might have been cascaded!
        // So checking the original level is hard without storing "Current Level/Bucket" in Node.
        // SOLUTION: O(1) Removal requires storing `std::list::iterator` and `List*` in the Node.
        // Or we just mark Node as "Cancelled" (Soft Delete) and ignore it on expiration.
        // Given `TimerImpl` maintains `_timers` map, we can just remove from Map.
        // When Wheel pops it, we check if it exists in Map?
        // But Wheel holds shared_ptr. The Node still exists.
        // We need `cancelled` flag in Node.
        // Let's add `bool cancelled` to Node.
        // Soft Delete is O(1).

        node->cancelled = true;
    }

private:
    void Cascade(std::array<std::list<std::shared_ptr<Node>>, TVN_SIZE> &tv, size_t index)
    {
        auto &list = tv[index];
        if (list.empty())
            return;

        // Move list to temp to iterate safely
        std::list<std::shared_ptr<Node>> temp;
        temp.splice(temp.begin(), list);

        for (auto &node : temp)
        {
            Add(node); // Re-insert (will fall to lower level)
        }
    }

private:
    uint32_t _currentTick;

    // 5 Vectors of Lists
    std::array<std::list<std::shared_ptr<Node>>, TVR_SIZE> _tv1;
    std::array<std::list<std::shared_ptr<Node>>, TVN_SIZE> _tv2;
    std::array<std::list<std::shared_ptr<Node>>, TVN_SIZE> _tv3;
    std::array<std::list<std::shared_ptr<Node>>, TVN_SIZE> _tv4;
    std::array<std::list<std::shared_ptr<Node>>, TVN_SIZE> _tv5;
};

} // namespace System
