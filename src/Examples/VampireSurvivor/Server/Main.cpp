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
        std::cerr << "Failed to initialize server application." << std::endl;
        return 1;
    }

    return 0;
}
