#include "System/Network/ByteBuffer.h"
#include <gtest/gtest.h>


using namespace System;

TEST(ByteBufferTest, WriteAndReadPrimitives)
{
    ByteBuffer buffer;

    int32_t val1 = 12345;
    float val2 = 3.14f;
    uint8_t val3 = 255;

    buffer.Write(val1);
    buffer.Write(val2);
    buffer.Write(val3);

    EXPECT_EQ(buffer.Read<int32_t>(), val1);
    EXPECT_FLOAT_EQ(buffer.Read<float>(), val2);
    EXPECT_EQ(buffer.Read<uint8_t>(), val3);
}

TEST(ByteBufferTest, WriteAndReadString)
{
    ByteBuffer buffer;
    std::string text = "Hello World";

    buffer.Write(text);

    std::string readText;
    buffer.Read(readText);

    EXPECT_EQ(text, readText);
}

TEST(ByteBufferTest, UnderflowCheck)
{
    ByteBuffer buffer;
    buffer.Write<int>(10);
    buffer.Read<int>();

    EXPECT_THROW(buffer.Read<int>(), std::underflow_error);
}

TEST(ByteBufferTest, MixedTypes)
{
    ByteBuffer buffer;
    buffer.Write<int>(1);
    buffer.Write(std::string("Test"));
    buffer.Write<double>(99.9);

    EXPECT_EQ(buffer.Read<int>(), 1);

    std::string s;
    buffer.Read(s);
    EXPECT_EQ(s, "Test");

    EXPECT_DOUBLE_EQ(buffer.Read<double>(), 99.9);
}
