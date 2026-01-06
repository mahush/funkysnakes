# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

### Build
```bash
# Standard build
cmake -S . -B build
cmake --build build

# With specific compiler
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-13
cmake --build build
```

### Run
```bash
./build/snake_actors      # Run main application
./build/test_snake        # Run tests
```

### Testing
```bash
# Build with tests enabled (default)
cmake -S . -B build -DSNAKE_BUILD_TESTS=ON
cmake --build build
./build/test_snake

# Run specific test
./build/test_snake --gtest_filter=TestName.*
```

**IMPORTANT:** Always use GoogleTest (gtest) for writing tests. Add new tests to `test/test_actors.cpp` or create new test files in the `test/` directory following the existing patterns.

### Code Quality
```bash
# Generate compile_commands.json (automatic with current CMakeLists.txt)
cmake -S . -B build
# Symlink already exists: compile_commands.json -> build/compile_commands.json

# Format code (uses .clang-format - Google style, 120 cols)
clang-format -i src/*.cpp include/**/*.hpp

# Lint (uses .clang-tidy)
clang-tidy src/*.cpp -- -I include
```

## Architecture Overview

### Actor-Based Message Passing System

This project implements a **typed actor model** using standalone Asio for thread-safe, message-based concurrency. The core architecture consists of:

1. **Topic-Based Pub/Sub** (`topic.hpp`, `topic_subscription.hpp`, `topic_publisher.hpp`)
   - Topics are typed message channels (`Topic<Msg>`)
   - Publishers send messages via `Publisher<Msg>::publish()`
   - Actors subscribe with `Subscription<Msg>` and pull messages via `tryReceive()`
   - Thread-safe: multiple actors can publish/subscribe concurrently
   - Uses weak_ptr for automatic cleanup of dead subscribers

2. **Actor Base Class** (`actor.hpp`)
   - CRTP-based `Actor<Derived>` provides factory pattern and subscription lifecycle
   - Each actor has an Asio strand for serialized execution (no explicit locking needed)
   - Factory method: `Actor<T>::create(io, ...)` ensures proper shared_ptr initialization
   - Subscriptions are deferred until after construction (enables `shared_from_this()`)
   - Helper methods: `create_sub()` and `create_pub()` for subscription/publisher creation

3. **Message Processing Flow**
   - Topics notify actors via `asio::post(strand, processMessages())`
   - Actors implement `processMessages()` to pull and process queued messages
   - Only notifies when queue transitions from empty→non-empty (avoids spam)
   - Pull-based model: actors control when to process messages

### Game Components

- **GameManager** (`game_manager.hpp/.cpp`): Coordinates game lifecycle, owns game timer, generates Tick messages
- **GameSession** (`game_session.hpp/.cpp`): Core game engine, manages game state (board, snakes, food, scores)
- **Renderer** (`renderer.hpp/.cpp`): Subscribes to StateUpdate, renders game state to console
- **InputActor** (`input_actor.hpp/.cpp`): Reads keyboard input, publishes DirectionChange messages

### Message Types

- **Control Messages** (`control_messages.hpp`): JoinRequest, LeaveRequest, StartGame, GameSummary
- **Game Messages** (`game_messages.hpp`): Tick, DirectionChange, StateUpdate, GameOver, LevelChange

### Key Patterns

- **Factory Construction**: Always use `Actor<T>::create(io, ...)` instead of constructors directly
- **Strand Isolation**: Each actor runs on its own strand - no manual locking needed
- **Weak References**: Topics hold weak_ptr to actors for automatic cleanup
- **Deferred Initialization**: Subscriptions registered after construction to enable shared_from_this()
- **Pull-Based Processing**: Actors pull messages from subscriptions rather than push callbacks

### Lens & Traversal Naming

**Goal:** Consistent, self‑descriptive helpers for accessing and updating parts of complex state.

| Prefix | Meaning | Type | Returns |
|---------|----------|------|---------|
| `over_` | Apply a transformation to one sub‑part of a structure (modify) | Lens (setter) | Updated structure |
| `over_each_` | Apply a transformation to every element of a contained list (modify) | Traversal (setter) | Updated structure |
| `with_` | Extract sub‑parts from a structure (read-only) | View (getter) | Extracted value(s) |

**Lens vs View Distinction**
- **Lenses (`over_*`)**: Focus on a part of state, apply a transformation, return updated state. These are the *setter* side of lenses.
- **Views (`with_*`)**: Focus on parts of state for read-only access, extract values without modification. These are *getters*.

**Rules**
- Always use lowercase snake_case.
- Name the focused part explicitly (e.g. `over_snake`, `with_board_and_snakes`).
- Add `_combining_scores` suffix when the helper accumulates score effects.
- Use `over_` when the function modifies and returns updated state.
- Use `with_` when the function only extracts data for reading.

**Examples**
```cpp
// Lens (modifier): updates direction_command state
GameState new_state = over_direction_command_and_snakes(state, filter_op, direction_cmd);

// Traversal (modifier): calls function multiple times (once per alive snake)
GameState new_state = over_each_alive_snake_combining_scores(state, move_snake);

// Lens (modifier): calls function once with all selected snakes as parameters
GameState new_state = over_selected_snakes_combining_scores(state, handle_collision, "player1", "player2");

// View (getter): extracts board and snakes for read-only access
Point pos = with_board_and_snakes(state, generateRandomPosition);
```

**Rationale:**
Functions starting with `over_` clearly signal "focus and modify part of state"; `over_each_` signals iteration with modification; `with_` signals read-only extraction. This keeps transformations declarative, composable, and easily searchable.

## Important Implementation Notes

### Actor Creation
Always use the factory pattern for actors:
```cpp
// CORRECT
auto actor = MyActor::create(io, topic1, topic2);

// WRONG - constructor is protected
auto actor = std::make_shared<MyActor>(io, topic1, topic2);
```

### Message Processing Pattern
Actors process messages by pulling from subscriptions:
```cpp
void MyActor::processMessages() {
  while (auto msg = my_sub_->tryReceive()) {
    handleMessage(*msg);
  }
}
```

### Topic Lifecycle
- Topics should be created first and shared among actors
- Use `TopicPtr<Msg>` (shared_ptr) to pass topics around
- Topics are thread-safe - safe to publish from any thread

### Subscription vs Publisher
- **Subscription**: Owned by actor (value or shared_ptr), used to pull messages
- **Publisher**: Lightweight wrapper, used to send messages to topic
- Create with `create_sub()` and `create_pub()` helpers in Actor base class

## Dependencies

- **Standalone Asio 1.30.2**: Auto-downloaded via CMake FetchContent
- **GoogleTest 1.15.2**: Auto-downloaded for tests (optional)
- **C++17**: Required (GCC 11+, Clang 14+)
- **CMake 3.12+**: Build system

## CI/CD

Automated builds on push/PR test against:
- GCC: 11, 12, 13, 14
- Clang: 14, 15, 16, 17, 18
- All with `-Werror` (warnings as errors)
