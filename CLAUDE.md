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

### Coding Style

#### Naming Conventions

**Types** (classes, structs, enums, type aliases): `CamelCase`
```cpp
class GameEngineActor { };
struct Point { };
enum class Direction { UP, DOWN, LEFT, RIGHT };
using GameTimer = Timer<GameTimerElapsedEvent, GameTimerCommand>;
```

**Template Parameters**: `TCamelCase` - prefix with `T`, append `Fn` for callables
```cpp
template <typename TState, typename TTransformFn>
void transform(TState& state, TTransformFn&& fn);
```

**Functions/Methods**: `camelCase`
```cpp
void processInputs();
Point getHead(const Snake& snake);
std::optional<Message> tryTakeMessage();
```

**Member Variables**: `snake_case_` - lowercase with trailing underscore
```cpp
class Actor {
  asio::strand<...> strand_;
  GameState game_state_;
  PublisherPtr<Msg> state_pub_;
};
```

**Local Variables & Parameters**: `snake_case` - lowercase, no trailing underscore
```cpp
void moveSnake(const Snake& snake, Direction new_dir) {
  Point next_pos = calculateNextPosition(snake.head, new_dir);
}
```

**Constants & Enum Values**: `SCREAMING_SNAKE_CASE`
```cpp
constexpr size_t MAX_QUEUE_SIZE = 2;
constexpr int MIN_FOOD_COUNT = 5;
inline constexpr const char* PLAYER_A = "Player A";

enum class Direction { UP, DOWN, LEFT, RIGHT };  // Enum values: all caps
```

**Namespaces**: `snake_case`
```cpp
namespace actor_core { }
namespace snake { }
```

**Files**: `snake_case.hpp` / `snake_case.cpp`
```
game_engine_actor.hpp
snake_operations.cpp
```

**Include Guards**: `SCREAMING_SNAKE_CASE` with full path
```cpp
#ifndef SNAKE_GAME_TYPES_HPP
#define SNAKE_GAME_TYPES_HPP
// ... or use #pragma once
```

#### Initialization

**Constructor Initializer Lists:** Always use curly braces `{}` for member initialization in constructor initializer lists. This prevents narrowing conversions.

**Variable Initialization:** For regular variable declarations, use assignment `=` or curly braces `{}`.

#### Documentation

**Doxygen-style comments** for public APIs:
```cpp
/**
 * @brief Brief description of what the function does
 *
 * More detailed explanation if needed (optional).
 *
 * @param param_name Description of parameter
 * @return Description of return value
 */
int doSomething(int param_name);
```

**Inline comments** for implementation details:
```cpp
// Single-line comment for explanation
void processInputs() {
  // Pull all pending messages
  while (auto msg = sub_->tryTakeMessage()) {
    handleMessage(*msg);
  }
}
```

#### Header Organization

**Include Order**: Standard library → Third-party → Local headers
```cpp
#include <memory>      // Standard library
#include <vector>

#include "asio.hpp"    // Third-party

#include "actor-core/actor.hpp"    // Local headers
#include "snake/game_types.hpp"
```

**Include Guards**: Use either `#pragma once` (preferred for new files) or traditional include guards:
```cpp
// Option 1: pragma once (simpler)
#pragma once

// Option 2: traditional guards (full path in SCREAMING_SNAKE_CASE)
#ifndef SNAKE_GAME_TYPES_HPP
#define SNAKE_GAME_TYPES_HPP
// ...
#endif  // SNAKE_GAME_TYPES_HPP
```

**Namespace Closing**: Add comment for clarity
```cpp
namespace snake {
// ...
}  // namespace snake
```

#### Class Organization

**Member Access Order**: `public` → `protected` → `private`
```cpp
class MyClass {
 public:
  // Public interface first

 protected:
  // Protected members for derived classes

 private:
  // Private implementation last
};
```

**Group Related Members** with comments:
```cpp
class GameEngineActor {
 private:
  // Publishers for sending messages
  PublisherPtr<StateMsg> state_pub_;
  PublisherPtr<EventMsg> event_pub_;

  // Subscriptions for pulling messages
  SubscriptionPtr<DirectionMsg> direction_sub_;
  SubscriptionPtr<TickMsg> tick_sub_;

  // Game state
  GameState game_state_;
};
```

#### Type Aliases

**Define type aliases** for commonly used pointer types to reduce verbosity:
```cpp
using GameTimerPtr = std::shared_ptr<GameTimer>;
template <typename T>
using TopicPtr = std::shared_ptr<Topic<T>>;
template <typename T>
using PublisherPtr = std::shared_ptr<Publisher<T>>;
template <typename T>
using SubscriptionPtr = std::shared_ptr<Subscription<T>>;
```

#### Parameter Passing

**Pass by const reference** for read-only complex types:
```cpp
Point getNextHeadPosition(const Snake& snake);
bool isBiteDropFoodMode(const GameState& state);
```

**Pass by value** for:
- Small POD types (int, bool, enum)
- When taking ownership and modifying (functional style)
```cpp
Point wrapPoint(Point p, const Board& board);
Snake moveSnake(Snake snake, const Board& board, bool is_eating);
```

**Forward references** for template perfect forwarding:
```cpp
template <typename TFn>
void process(TFn&& fn);
```

#### const Correctness

**Mark read-only parameters** as `const&`:
```cpp
void processState(const GameState& state);
```

**Mark non-mutating methods** as `const`:
```cpp
bool operator==(const Point& other) const;
size_t length() const;
```

**Mark const member variables** when they never change after construction:
```cpp
class Board {
  const int width_;
  const int height_;
};
```

#### auto Usage

**Use auto** for:
- Complex template return types
- Structured bindings
- Iterator types
- Lambda captures

```cpp
// Complex template return types
auto result = makePipe(transform1, transform2, transform3);
auto pipeline = with_effect_handling(process_fn, effect_handler);

// Structured bindings
auto [new_state, score_delta, food_effect] = processTick(state, tick);
auto [snakes, scores, directions] = extractGameData(state);

// Iterators
for (auto it = container.begin(); it != container.end(); ++it) { }
// Or better: range-based for
for (const auto& item : container) { }
```

**Avoid auto** when type clarity is important:
```cpp
// Prefer explicit type for clarity
int score = calculateScore();  // Better than: auto score = calculateScore();
bool is_alive = checkAlive();  // Better than: auto is_alive = checkAlive();
```

#### Internal Linkage in .cpp Files

**Use unnamed (anonymous) namespace** for internal helpers in `.cpp` files:
```cpp
// In game_engine_actor.cpp
namespace {

// Internal helper function
bool isBiteDropFoodMode(const GameState& state) {
  return state.collision_mode == CollisionMode::BITE_DROP_FOOD;
}

// Internal helper types
struct CollisionResult {
  PerPlayerSnakes snakes;
  std::vector<Point> dropped_food;
};

}  // namespace

// Public functions from .hpp follow below
void GameEngineActor::processInputs() { ... }
```

This prevents name collisions and keeps internal implementation details from leaking to other translation units.

## Architecture Overview

### Actor-Based Message Passing System

This project implements a **typed actor model** using standalone Asio for thread-safe, message-based concurrency. The core architecture consists of:

1. **Topic-Based Pub/Sub** (`topic.hpp`, `subscription.hpp`, `publisher.hpp`)
   - Topics are typed message channels (`Topic<TMessage>`)
   - Publishers send messages via `Publisher<TMessage>::publish()`
   - Actors subscribe with `Subscription<TMessage>` and pull messages via `tryTakeMessage()`
   - Thread-safe: multiple actors can publish/subscribe concurrently
   - Uses weak_ptr for automatic cleanup of dead subscribers

2. **Actor Base Class** (`actor.hpp`)
   - CRTP-based `Actor<Derived>` provides factory pattern and subscription lifecycle
   - Each actor has an Asio strand for serialized execution (no explicit locking needed)
   - Factory method: `Actor<T>::create(io, ...)` ensures proper shared_ptr initialization
   - Subscriptions are deferred until after construction (enables `shared_from_this()`)
   - Helper methods: `create_sub()` and `create_pub()` for subscription/publisher creation

3. **Message Processing Flow**
   - Topics notify actors via `asio::post(strand, processInputs())`
   - Actors implement `processInputs()` to pull and process queued messages
   - Only notifies when queue transitions from empty→non-empty (avoids spam)
   - Pull-based model: actors control when to process messages

### Game Components

- **GameManager** (`game_manager.hpp/.cpp`): Coordinates game lifecycle, owns game timer, generates TickMsg messages
- **GameEngineActor** (`game_engine_actor.hpp/.cpp`): Core game engine, manages game state (board, snakes, food, scores)
- **Renderer** (`renderer.hpp/.cpp`): Subscribes to StateUpdate, renders game state to console
- **InputActor** (`input_actor.hpp/.cpp`): Reads keyboard input, publishes DirectionMsg messages

### Message Types

- **Control Messages** (`control_messages.hpp`): JoinRequest, LeaveRequest, StartGameMsg, GameSummaryMsg
- **Game Messages** (`game_messages.hpp`): TickMsg, DirectionMsg, StateUpdate, GameOverMsg, LevelChange

### Key Patterns

- **Factory Construction**: Always use `Actor<T>::create(io, ...)` instead of constructors directly
- **Strand Isolation**: Each actor runs on its own strand - no manual locking needed
- **Weak References**: Topics hold weak_ptr to actors for automatic cleanup
- **Deferred Initialization**: Subscriptions registered after construction to enable shared_from_this()
- **Pull-Based Processing**: Actors pull messages from subscriptions rather than push callbacks

### Lens & Traversal Naming

**Goal:** Consistent, self‑descriptive helpers for accessing and updating parts of complex state.

| Pattern | Meaning | Type | Returns |
|---------|----------|------|---------|
| `over_X` | Apply a transformation to one sub‑part of a structure (modify X) | Lens (setter) | Updated structure |
| `over_X_viewing_Y` | Apply a transformation to X while reading Y as context | Lens (setter) | Updated structure |
| `over_each_X` | Apply a transformation to every element of a contained list | Traversal (setter) | Updated structure |
| `view_X` | Extract sub‑parts from a structure (read-only) | View (getter) | Extracted value(s) |

**Lens vs View Distinction**
- **Lenses (`over_*`)**: Focus on a part of state, apply a transformation, return updated state. These are the *setter* side of lenses.
  - `over_X`: Mutate X only
  - `over_X_and_Y`: Mutate both X and Y
  - `over_X_viewing_Y`: Mutate X while viewing Y as read-only context
  - `over_X_and_Y_viewing_Z`: Mutate X and Y while viewing Z as read-only context
- **Views (`view_*`)**: Standalone functions that extract parts of state for read-only access. These are *getters*.

**Rules**
- Always use lowercase snake_case.
- Name the focused part explicitly (e.g. `over_snakes`, `view_board_and_snakes`).
- Add `_combining_scores` suffix when the helper accumulates score effects.
- Use `over_` prefix for lenses that modify and return updated state.
- Use `_viewing_` infix to indicate read-only context fields in lenses.
- Use `view_` prefix for standalone getter functions.

**Examples**
```cpp
// Lens (modifier): updates direction_command, views snakes as context
GameState new_state = over_direction_command_filter_state_viewing_snakes(state, filter_op);

// Lens (modifier): updates snakes, views board and food as context
GameState new_state = over_snakes_viewing_board_and_food(state, moveSnakes);

// Lens (modifier): updates both food and scores, views snakes as context
GameState new_state = over_food_and_scores_viewing_snakes(state, handleEating);

// Traversal (modifier): calls function multiple times (once per alive snake)
GameState new_state = over_each_alive_snake_combining_scores(state, move_snake);

// View (getter): extracts board and snakes for read-only access
Point pos = view_board_and_snakes(state, generateRandomPosition);
```

**Rationale:**
- `over_` prefix clearly signals state modification
- `_viewing_` infix explicitly marks read-only context, preventing confusion about which fields are mutable
- `view_` prefix for standalone getters aligns with lens terminology and makes extractors easily searchable
- This keeps transformations declarative, composable, and self-documenting

### Function Decorators

**Goal:** Consistent naming for higher-order functions that wrap other functions to add behavior.

**Pattern:** Use `with_` prefix for decorators that wrap functions to add cross-cutting concerns like effect handling, logging, retry logic, etc.

**Rules**
- Use `with_X` when wrapping a function to add X behavior
- Common in functional programming for resource management, effect handling, etc.
- The decorator returns a new function with enhanced behavior

**Examples**
```cpp
// Decorator: wraps a function to automatically handle effects
auto process_with_effect_handling = with_effect_handling(process_fn, effect_handler);
state = process_with_effect_handling(state, event);

// Decorator: wraps a function to add logging
auto compute_with_logging = with_logging(compute_fn, logger);
```

**Rationale:**
The `with_` prefix for decorators is a well-established convention in many languages (React HOCs, Python context managers, etc.). It naturally reads as "do X with Y added" and is kept separate from lens/view naming to avoid confusion.

### Template Parameter Naming

**Rule:** Template parameters should start with `T` and use descriptive names. For callable types, append `Fn` suffix.

```cpp
// Correct: template parameters prefixed with T
template <typename TTimer, typename TState, typename TProcessFn>
void processEventWithState(const std::shared_ptr<TTimer>& timer, TState& state, TProcessFn&& process_fn);

template <typename TState, typename TTransformerFn>
void applyToState(TState& state, TTransformerFn&& transformer);

// Incorrect: no T prefix
template <typename State, typename TransformerFn>  // ❌
void applyToState(State& state, TransformerFn&& transformer);
```

**Variable Naming:** Avoid generic names like `handler`. Use descriptive names like `process_fn`, `transformer`, `mapper`:

```cpp
// Correct: descriptive variable names
template <typename TMsg, typename TProcessFn>
void processMessage(const Subscription<TMsg>& sub, TProcessFn&& process_fn);

// Incorrect: generic "handler" name
template <typename TMsg, typename THandlerFn>
void processMessage(const Subscription<TMsg>& sub, THandlerFn&& handler);  // ❌
```

This convention makes template parameters easily identifiable and clarifies the purpose of callable parameters.

### Parameter Ordering for bindFront

**Rule:** Functions used with `bindFront` must have bound parameters **first**, before lens-provided parameters.

```cpp
// Correct: bound params first, lens params last
ReturnType func(BoundParam1, BoundParam2, LensParam1, LensParam2);

// Usage with bindFront
state = over_lens_params(bindFront(func, bound_arg1, bound_arg2))(state);
```

**Example:**
```cpp
// addPlayer: bound (player_id, pos, dir, len) then lens (snakes, scores, dirs)
std::tuple<PerPlayerSnakes, PerPlayerScores, PerPlayerDirectionQueue>
addPlayer(PlayerId player_id, Point pos, Direction dir, int len,
          PerPlayerSnakes snakes, PerPlayerScores scores, PerPlayerDirectionQueue dirs);

state = over_snakes_scores_and_pending_directions(
    bindFront(addPlayer, PlayerId{"p1"}, Point{5, 10}, Direction::RIGHT, 7)
)(state);
```

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
void MyActor::processInputs() {
  while (auto msg = my_sub_->tryTakeMessage()) {
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
