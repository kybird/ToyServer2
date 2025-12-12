# Coding Conventions

This document outlines the coding standards and architectural patterns used in the ToyServer2 project. Strive to adhere to these guidelines to maintain consistency and modularity.

## 1. Directory Structure & Visibility

### **Interfaces (Public API)**
*   **Location**: Module's root directory.
*   **Naming**: `I` prefix + PascalCase (e.g., `IDispatcher.h`, `ISession.h`).
*   **Purpose**: Define the contract. These are the **ONLY** files that should be included by other modules.

### **Implementations (Private Details)**
*   **Location**: Subdirectory within the module.
*   **Naming**:
    *   Directory: Often capitalized or tech-specific (e.g., `DISPATCHER`, `ASIO`, `BOOTSTRAP`).
    *   File/Class: `Impl` suffix or specific technology name (e.g., `DispatcherImpl.h`, `AsioSession.h`).
*   **Purpose**: Concrete logic. These should **NOT** be included directly by external modules if possible. Use Dependency Injection.

**Example:**
```
src/System/Dispatcher/
├── IDispatcher.h          (Public Interface)
└── DISPATCHER/            (Hidden Implementation)
    ├── DispatcherImpl.h
    └── DispatcherImpl.cpp
```

## 2. Naming Conventions

### **Class Names**
*   **Avoid 'Manager'**: Do not use the suffix `Manager` (e.g., `PacketManager`, `SessionManager`). It is vague and often leads to god classes.
*   **Preferred Alternatives**:
    *   `Service` (e.g., `AsioService`)
    *   `System`
    *   `Controller`
    *   `Dispatcher`, `Factory`, `Loader` (Functional names)
*   **Interfaces**: Always start with `I`.

### **Variables**
*   **Member Variables**: Prefix with `_` (e.g., `_dispatcher`, `_socket`).
*   **Global/Static**: Explicit prefixes like `g_` are sometimes used but DI is preferred.

## 3. Architecture Patterns

### **Dependency Injection (DI)**
*   Favor injecting dependencies (via constructors) over global singletons or static accessors.
*   Example: `AsioSession` receives `IDispatcher*` in `Reset()`, rather than calling `GetDispatcher()`.

### **RAII & Ownership**
*   Use `std::shared_ptr` / `std::unique_ptr` for resource management.
*   `Framework` class owns core components (`AsioService`, `DispatcherImpl`) explicitily.
