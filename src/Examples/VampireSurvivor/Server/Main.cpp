#include "Core/ServerApp.h"
#include <iostream>

int main()
{
    SimpleGame::ServerApp app;

    if (app.Init())
    {
        app.Run();
    }
    else
    {
        std::cerr << "ServerApp Initialization Failed.\n";
        return 1;
    }

    return 0;
}
