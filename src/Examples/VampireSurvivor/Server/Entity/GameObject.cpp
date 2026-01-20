#include "Entity/GameObject.h"
#include "System/ILog.h"

namespace SimpleGame {

void GameObject::UpdateStateExpiry(float currentTime)
{
    if (_stateExpiresAt > 0.0f && currentTime >= _stateExpiresAt)
    {
        // [Verification Log] Track state transitions
        LOG_INFO(
            "[State] Object {} state expired. {} -> IDLE (At: {:.2f}, Exp: {:.2f})",
            _id,
            (int)_state,
            currentTime,
            _stateExpiresAt
        );

        _state = Protocol::ObjectState::IDLE;
        _stateExpiresAt = 0.0f;
    }
}

} // namespace SimpleGame
