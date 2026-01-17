#include "System/Console/CommandConsole.h"
#include "System/IConfig.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace System;

class MockConfig : public IConfig
{
public:
    bool Load(const std::string &filePath) override
    {
        return true;
    }
    const ServerConfig &GetConfig() const override
    {
        return _config;
    }

private:
    ServerConfig _config;
};

class CommandConsoleTest : public ::testing::Test
{
protected:
    std::shared_ptr<MockConfig> config;
    std::shared_ptr<CommandConsole> console;

    void SetUp() override
    {
        config = std::make_shared<MockConfig>();
        console = std::make_shared<CommandConsole>(config);
    }
};

TEST_F(CommandConsoleTest, RegisterAndExecute)
{
    bool called = false;
    std::string receivedArg;

    console->RegisterCommand(
        {"/test",
         "Test Command",
         [&](const std::vector<std::string> &args)
         {
             called = true;
             if (!args.empty())
                 receivedArg = args[0];
         }}
    );

    console->ProcessCommand("/test arg1");

    EXPECT_TRUE(called);
    EXPECT_EQ(receivedArg, "arg1");
}

TEST_F(CommandConsoleTest, DuplicateRegistration)
{
    int callCount1 = 0;
    int callCount2 = 0;

    console->RegisterCommand(
        {"/dup",
         "Dup 1",
         [&](const std::vector<std::string> &)
         {
             callCount1++;
         }}
    );

    console->ProcessCommand("/dup");
    EXPECT_EQ(callCount1, 1);

    // Overwrite
    console->RegisterCommand(
        {"/dup",
         "Dup 2",
         [&](const std::vector<std::string> &)
         {
             callCount2++;
         }}
    );

    console->ProcessCommand("/dup");
    EXPECT_EQ(callCount1, 1); // Should not be called
    EXPECT_EQ(callCount2, 1); // Should be called
}

TEST_F(CommandConsoleTest, ExceptionIsolation)
{
    console->RegisterCommand(
        {"/crash",
         "Crash Command",
         [&](const std::vector<std::string> &)
         {
             throw std::runtime_error("Crash Test");
         }}
    );

    // Should not throw and crash the test
    EXPECT_NO_THROW(console->ProcessCommand("/crash"));
}

TEST_F(CommandConsoleTest, Unregister)
{
    bool called = false;
    console->RegisterCommand(
        {"/temp",
         "Temp",
         [&](const std::vector<std::string> &)
         {
             called = true;
         }}
    );

    console->UnregisterCommand("/temp");
    console->ProcessCommand("/temp");

    EXPECT_FALSE(called);
}

TEST_F(CommandConsoleTest, DefaultCommands)
{
    // Just ensure they don't crash when called (we can't easily check output here without mocking logger)
    EXPECT_NO_THROW(console->ProcessCommand("/status"));
    EXPECT_NO_THROW(console->ProcessCommand("/help"));
    // /quit calls exit(0), so we can't test it easily without death test or logic change.
}
