# Server Framework Roadmap

## 🔄 Remaining Tasks (Phase 2+)

### Operation Tools (Phase 2 - Continued)
- [x] **Structured Logger** (Context, Macros)
    - **Goal**: Add Context fields to logs for better debugging.
    - **Priority**: Done
- [x] **Rate Limiter** (Token Bucket)
    - **Goal**: Network protection against DDoS/Spam.
    - **Method**: Token Bucket (Header-only).
- [x] **Command Console**
    - **Goal**: Runtime server control via stdin.

### Convenience Features (Phase 3)
- [x] **Event Bus**
    - **Goal**: Decouple modules using Publish/Subscribe pattern.
- [x] **ByteBuffer**
    - **Goal**: Serialization utility for packet payload construction.
- [x] **Connection Pool (DB)**
    - **Goal**: Database connection management.

### Game Server Basics (Phase 4)
- [x] **Protocol & Packet Handling**
    - **Goal**: Standardize Request/Response using ByteBuffer.
- [x] **Authentication**
    - **Goal**: Login flow using EventBus and DB Pool.
- [x] **Room Management**
    - **Goal**: Managing player sessions and game rooms.

### Advanced Features (Phase 5)
- [x] **SQLite Integration**
    - **Goal**: Lightweight, server-less DB for development.
- [ ] **File Watcher** (Deferred)
- [ ] **Metrics Dashboard** (Deferred)

## 🎮 Final Project: Vampire Survivor Clone (Portfolio)
- **Goal**: Create a complete multiplayer Vampire Survivor-like game.
- **Client**: Unity or Unreal (TBD).
- **Server**: Use current `System` framework.
- **Features**:
    - Real-time movement synchronization.
    - Large number of enemy entities.
    - Spatial Partitioning (Grid/Quadtree).
    - Protobuf Integration (for efficient packet size).

### Optional / Future
- [ ] **File Watcher**
    - **Goal**: Hot Reloading of config/data.
- [x] **Crash Handler**
    - **Goal**: Minidump generation on crash. Included Enhanced Dump Flags.

---

## 🎯 Next Steps Recommendations

### Option A: Complete Operation Tools (Pragmatic)
- **Next**: Rate Limiter
- **Reason**: Essential for network security.
- **Difficulty**: ⭐⭐ (1 Day)

### Option B: Start Game Development (Fun)
- **Next**: Vampire Survivor Game Logic
- **Scope**: Entity Manager, Spatial Grid, Protobuf Integration.
- **Reason**: Framework is "good enough" to start making the game.

### Option C: Enhance Logging (Stability)
- **Next**: Structured Logger
- **Reason**: Deep debugging capability.
- **Difficulty**: ⭐⭐ (1 Day)
