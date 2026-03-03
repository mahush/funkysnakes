# Snake Game - Functional Core, Imperative Shell Architecture in C++

A multiplayer Snake game implementation demonstrating **functional core, imperative shell** architecture with an actor-based concurrency model in modern C++17.

## Overview

This project showcases clean architecture principles through a fully playable two-player Snake game:

- **Functional Core**: Pure game logic with immutable state transformations using lenses and composable pipelines
- **Imperative Shell**: Actor-based message passing with Asio for concurrency and I/O
- **Type-Safe**: Strongly typed messages, compile-time lens composition, effect handling
- **Testable**: Pure functions separated from side effects enable straightforward unit testing

## Architectural Highlights

### Functional Core

The game engine processes state through pure, composable transformations:

```cpp
auto tick_pipeline = makePipe(
    over_direction_command_filter_state(direction_command_filter::try_consume_next),
    over_snakes_viewing_board_and_food(moveSnakes),
    over_snakes_and_scores(handleCollisions),
    over_food_and_scores_viewing_snakes(handleFoodEating),
    over_food_viewing_board_and_snakes(replenishFood)
);

state = tick_pipeline(state);  // Pure state transformation
```

**Key patterns:**
- **Lenses**: `over_*` functions focus on specific state parts (e.g., `over_snakes_viewing_board_and_food`)
- **Immutable transformations**: Functions return new state rather than mutating
- **Effect separation**: Pure logic returns `tuple<State, Effects...>`, effects handled separately
- **Pipeline composition**: Complex game logic built from small, composable functions

### Imperative Shell

The actor system handles all side effects (I/O, timers, rendering):

```cpp
GameEngineActor  → Pure game logic + effect declarations
GameManager → Coordinates timers and game lifecycle
Renderer    → Displays game state to console
InputActor  → Reads keyboard input
```

**Actor features:**
- **Message passing**: Type-safe topic-based pub/sub
- **Thread-safe**: Asio strands eliminate need for explicit locking
- **Pull-based**: Actors control when to process messages
- **Weak references**: Automatic cleanup of dead subscribers

### Effect Handling

Effects are declared by pure functions, then interpreted by the shell:

```cpp
// Pure function declares effects
auto [new_state, score_delta, food_effect] = processTick(state, tick);

// Shell interprets effects
effect_handler.handle(score_delta);  // Update UI
effect_handler.handle(food_effect);  // Spawn particles
```

This separation enables testing game logic without I/O while keeping effects explicit.

## Game Features

- **Two-player local multiplayer** (Player A: WASD, Player B: Arrow keys)
- **Progressive difficulty**: Game speeds up with each level
- **Collision detection**: Snakes can bite each other's tails
- **Score system**: Earn points for eating food, lose points for collisions
- **Food mechanics**: Random food spawning, periodic repositioning
- **Clean terminal UI**: Box-drawing characters with real-time updates

## Project Structure

```
snake/
├── include/
│   └── snake/                   # Game-specific code
│       ├── game_engine_actor.hpp      # Core game logic (functional)
│       ├── game_manager.hpp     # Lifecycle coordinator (imperative)
│       ├── renderer.hpp         # Display output (imperative)
│       ├── input_actor.hpp      # Keyboard input (imperative)
│       ├── game_state_lenses.hpp # Functional state transformers
│       └── process_helpers.hpp  # Effect handling decorators
├── src/                         # Implementation files
├── tests/                       # Unit tests (testing pure functions)
└── CMakeLists.txt
```

## Dependencies

- **CMake 3.12+**
- **C++17 compiler** (GCC 11+, Clang 14+)
- **Funkyactors** (auto-downloaded via FetchContent from [github.com/mahush/funkyactors](https://github.com/mahush/funkyactors))
- **Funkypipes** (auto-downloaded via FetchContent from [github.com/mahush/funkypipes](https://github.com/mahush/funkypipes))
- **Standalone Asio 1.30.2** (bundled with funkyactors)
- **GoogleTest 1.15.2** (optional, for tests, auto-downloaded)

## Build and Run

```bash
# Build
cmake -S . -B build
cmake --build build

# Run the game
./build/snake_actors

# Run tests
./build/test_snake
```

### Controls

- **Player A (Snake A)**: W (up), A (left), S (down), D (right)
- **Player B (Snake B)**: Arrow keys (↑ ↓ ← →)
- **Quit**: Q


See `CLAUDE.md` for detailed architectural documentation and coding conventions.

## License

See LICENSE file.
