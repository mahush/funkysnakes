# Architecture Concepts

This document describes key architectural patterns and principles used in this codebase.

---

## 1. Self-Contained Modules with Opaque State

### Core Idea

When designing functional modules that thread state explicitly, maintain strong encapsulation boundaries by treating module state as opaque at the domain level.

### Key Principles

#### 1.1 Module Owns Its State Type

Internal state types should be defined **within the module**, not at domain level. Wrap internal representation in a module-specific `State` struct. Domain code should never directly access or understand the internal structure.

```cpp
// In direction_command_filter.hpp (module level)
namespace direction_command_filter {
  // Internal type - not exposed to domain
  using PerPlayerDirectionQueue = std::map<PlayerId, std::deque<Direction>>;

  // Opaque state wrapper
  struct State {
    PerPlayerDirectionQueue queues;  // Implementation detail
  };
}

// In game_messages.hpp (domain level)
struct GameState {
  direction_command_filter::State direction_command_filter_state;  // Opaque!
  // Domain code cannot see the queues inside
};
```

#### 1.2 Naming Signals Ownership

At the domain level, use explicit naming to make it obvious that state belongs to a specific module:

- **Type name**: Use fully qualified module namespace (`direction_command_filter::State`)
- **Variable name**: Use module prefix (`direction_command_filter_state`)

This immediately communicates: "This is not domain-level state with semantic meaning—this is internal state that belongs to a specific module."

```cpp
struct GameState {
  direction_command_filter::State direction_command_filter_state;
};
```

#### 1.3 Module Manages Its Own Lifecycle

Since the module owns the state, it should handle:
- State initialization (lazy or explicit)
- State validation
- State transformations

Domain code should never directly construct or manipulate module state—only pass it through module functions.

```cpp
// Module handles lazy initialization internally
State try_add(State state, const PerPlayerSnakes& snakes, const DirectionCommand& cmd) {
  // Lazy init: create queue if player doesn't exist
  auto [queue_it, inserted] = state.queues.try_emplace(cmd.player_id, std::deque<Direction>{});
  // ... rest of logic
}
```

#### 1.4 Separation of Concerns in Domain Operations

Domain-level operations (like `addPlayer`) should only handle domain concerns (snakes, scores), not module-internal state. Let modules initialize their own state when needed.

```cpp
std::tuple<PerPlayerSnakes, PerPlayerScores>
addPlayer(...) {
  // Only handles domain concerns (snakes, scores)
  // Module initializes its own state when needed
  return {snakes, scores};
}
```

### Benefits

1. **Information Hiding**: Domain cannot depend on module implementation details
2. **Easier Refactoring**: Module can change internal representation without touching domain code
3. **Clear Boundaries**: Module prefix makes ownership and responsibilities obvious
4. **Testability**: Modules can be tested independently with their own state
5. **Scalability**: Pattern scales to multiple modules with their own state

### When to Apply

Use this pattern when:
- Creating a **functional module** (stateless functions that transform state explicitly)
- The module has **internal state structure** that domain shouldn't know about
- You want **strong encapsulation** despite explicit state passing
- The state is **only meaningful within the module's context**

### Guidelines for Future Modules

1. Define state types inside the module namespace
2. Wrap implementation in a `ModuleName::State` struct
3. Use `module_name::State module_name_state` at domain level
4. Module functions handle their own state lifecycle
5. Domain operations don't initialize or manipulate module state directly

This architectural pattern allows us to maintain functional programming benefits (explicit state) while preserving object-oriented encapsulation principles (information hiding, clear boundaries).

---
