#include "System/Metrics/MetricsCollector.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>


using namespace System;

class MetricsTest : public ::testing::Test
{
protected:
    std::unique_ptr<MetricsCollector> metrics;

    void SetUp() override
    {
        metrics = std::make_unique<MetricsCollector>();
    }

    void TearDown() override
    {
        metrics.reset();
    }
};

TEST_F(MetricsTest, CounterIncrement)
{
    // Metrics are implicitly registered on first access in this implementation
    auto counter = metrics->GetCounter("test_counter");

    counter->Increment(5);
    counter->Increment(3);

    EXPECT_EQ(counter->GetValue(), 8);
}

TEST_F(MetricsTest, GaugeSetGet)
{
    auto gauge = metrics->GetGauge("cpu_usage");

    gauge->Set(45); // Gauge uses int64_t in this impl
    EXPECT_EQ(gauge->GetValue(), 45);

    gauge->Set(78);
    EXPECT_EQ(gauge->GetValue(), 78);
}

TEST_F(MetricsTest, ConcurrentCounterIncrement)
{
    auto counter = metrics->GetCounter("concurrent");

    std::vector<std::thread> threads;
    for (int i = 0; i < 1000; ++i)
    {
        threads.emplace_back(
            [counter]()
            {
                counter->Increment(1);
            }
        );
    }

    for (auto &t : threads)
    {
        t.join();
    }

    EXPECT_EQ(counter->GetValue(), 1000);
}

TEST_F(MetricsTest, DynamicRegistry)
{
    auto m1 = metrics->GetCounter("metric_1");
    auto m2 = metrics->GetCounter("metric_2");

    m1->Increment(10);
    m2->Increment(20);

    EXPECT_EQ(m1->GetValue(), 10);
    EXPECT_EQ(m2->GetValue(), 20);
}

TEST_F(MetricsTest, SharedReference)
{
    auto c1 = metrics->GetCounter("shared");
    auto c2 = metrics->GetCounter("shared");

    c1->Increment(10);
    EXPECT_EQ(c2->GetValue(), 10);
}
