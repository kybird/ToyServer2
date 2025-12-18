#pragma once

// Core Interfaces
#include "IConfig.h"
#include "IDatabase.h"
#include "IFramework.h"
#include "ILog.h"
#include "ITimer.h"


// Network & Dispatcher
#include "System/Dispatcher/IPacketHandler.h"
#include "System/ISession.h"
#include "System/Network/PacketUtils.h"

namespace System {
}

// Note: Internal implementation headers (Session.h, MessagePool.h etc.) are purposely excluded.
