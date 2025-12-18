# Future Ideas & Todo

## Scripting Integration
- **Idea**: Integrate Lua or Python for game logic.
- **Why**: Allows game designers to tweak formulas/skills without recompiling.
- **Status**: Deferred to Phase 3 (Game Logic). Low priority for Infrastructure phase.

## External Control Plane
- **Idea**: Use Kafka/RabbitMQ or a Web Interface for server control.
- **Why**: Scalability for multi-server architecture.
- **Status**: Too early. Adds unnecessary infrastructure complexity for a single-node server.

## Advanced Database
- **Idea**: Connection Pool for MySQL/PostgreSQL.
- **Why**: Performance for heavy DB operations.
- **Status**: Planned for Phase 3.

## File Watcher
- **Idea**: Hot-reloading of configuration files (JSON) and scripts.
- **Why**: Faster iteration without restarting server.
- **Status**: Deferred. Priority given to Core Game Logic.

## Advanced Database Architecture (Capability-based)
- **Todo**: Refactor `IDatabase` to support Capability querying (e.g., `GetCapability<ISQLiteSpecific>()`).
- **Why**: To cleanly separate RDBMS-specific features (Vacuum, Backup, Kill) from the common interface.
- **Status**: Planned for future phase.

## Database Monitoring & Statistics
- **Todo**: Implement `IDatabaseMonitor` interface.
    - `OnAcquireTimeout()`: Track connection pool saturation.
    - `OnConnectionInvalid()`: Track driver/network stability.
    - `OnTransactionRollback()`: Track logical or runtime errors.
- **Why**: Essential for operating a high-performance DB layer. Current system lacks visibility into pool exhaustion or hidden errors.
- **Status**: **High Priority** for next iteration.
