# Classic snake — semantic artifacts of the whole game

This document lists the semantic artifacts of classic funkysnakes beyond the arena, following Executable
Semantic Architecture (ESA). The arena itself is covered in [`esa_classic_arena.md`](esa_classic_arena.md).

Status: the Classic Arena Authority, the Classic Game Lifecycle Authority and the Difficulty Policy are
extracted. `GameManagerActor` feeds messages and cadence timer events into the lifecycle Authority and
carries out its intents. The behaviors listed under [Behavior to pin](#pinned-behavior)
are preserved as-is.

## Overview

| Artifact | ESA form | Owns |
|---|---|---|
| [Classic Arena Authority](esa_classic_arena.md) | Authority | Snakes, scores, food, steering intentions and the complete arena step |
| [Classic Game Lifecycle Authority](#classic-game-lifecycle-authority) ([`classic_game_lifecycle_authority`](../include/snake/classic_game_lifecycle_authority.hpp)) | Authority, beside the arena | Game phase, level, conclusion and game-time cadences |
| [Difficulty Policy](#difficulty-policy) ([`difficulty_policy`](../include/snake/difficulty_policy.hpp)) | Policy | Level → step interval |
| [Domain System Boundary](#domain-system-boundary) | Boundary semantics | The game's external interactions, independent of devices |

Not domain semantics: the [player input adapter](#player-input-adapter) and the
[realization mechanics](#realization-mechanics) between actors.

## Classic Game Lifecycle Authority

Owns the evolution of a game as a whole. Its transitions are `start`, `togglePause`, `levelPeriodElapsed`,
`repositionPeriodElapsed`, `observeAliveStates` and `concluded`; each returns the new state plus intents
(clock, step interval, cadences, reposition, conclude) only where the state does not already encode them.
Elapsed cadence periods arrive as events from the actor's timers; the Authority decides what they mean.

- **phase**: running ↔ paused → over;
- **level**: starts at the requested level and increases over game time;
- **conclusion**: decides when the game is over, based on the arena's alive states. Currently the game is
  over when **zero** snakes are alive. This check lives inside the Authority rather than in a separate
  Policy for now;
- **game-time cadences**: level-up every 60 s and food reposition every 5 s, neither while paused. These are
  game rules about time, not timer configuration.

### Beside the arena, not above it

The lifecycle Authority and the Classic Arena Authority are independent owners connected by plain
composition:

- the arena never reads lifecycle state; it receives "step" and "reposition" as inputs;
- lifecycle reads exactly two arena views: alive states (for conclusion) and final scores (for the
  summary). Both are read-only projections, not a shared state change;
- "game over → stop stepping" is a lifecycle decision that shows up only as which inputs the arena receives.

So no higher Authority owns a combined truth, and no Protocol Contract is needed. The summary
request/response between the actors is a realization round trip; a single-process Domain Application would
simply read the arena's view.

Decoupling also lets the same lifecycle owner sit beside a different arena, such as Orbital Orders.

Reconsider this placement if a rule turns up that needs both truths within one step, for example "the step
that kills the last snake must not also level up". No such rule exists today.

## Difficulty Policy

A stateless decision mapping a level to the arena's step interval:

```text
interval_ms(level) = max(50, 200 − 15 · (level − 1))
```

## Domain System Boundary

The external view of the classic game, expressed as intents rather than devices.

**Players end**

| Direction | Interaction | Meaning |
|---|---|---|
| inbound | `Start(starting_level, players)` | Begin a new game |
| inbound | `Steer(player, direction)` | A player's intended turn |
| inbound | `TogglePause` | Pause or resume the game |
| outbound | arena view | Board, snakes, food and scores after each step |
| outbound | status | Current level and whether the game is paused |
| outbound | game-over summary | Final scores and final level |

**External facts**: elapsed time and randomness (food placement). Both are supplied by the realization.

## Player input adapter

`InputActor` adapts terminal input to the players end of the boundary:

- **device mechanics**: decoding raw bytes and ESC sequences into keys;
- **binding**: WASD → Player A; IJKL and arrow keys → Player B; `p` → `TogglePause`.

The binding is a deliberate choice, but a realization choice: a gamepad or a network client would bind
differently while the game means the same. It would become domain semantics only if the game rules
defined it.

`q` (quit) is not a game interaction. It is application control that stops the process.

## Realization mechanics

Not domain semantics:

- game ID routing (`game_id`, hard-coded as `"game_001"`) and filtering of stale messages;
- publishing alive states only when they change;
- the summary request/response between the manager and the engine;
- timer commands, restarting the step timer on a tick-rate change, shutdown and logging;
- rendering, including the flashing game-over text and dead snakes not being drawn.

## Pinned behavior

Current lifecycle behavior that looks unintended or inconsistent, preserved during extraction. Changing it is a
separate, explicit gameplay decision. The lifecycle-level items are pinned in
[`test_classic_game_lifecycle_authority.cpp`](../tests/test_classic_game_lifecycle_authority.cpp).

| Behavior | Notes |
|---|---|
| Toggling pause twice after game over restarts the step clock | Pause handling does not check for game over; RESUME restarts the step timer. Level-up and reposition stay stopped |
| The starting level does not affect the initial speed | START uses the engine's stored 200 ms instead of the Difficulty Policy for `starting_level`. Hidden today because the game always starts at level 1 |
| `Start.players` is ignored | The arena always creates Player A and Player B |
| The 200 ms base interval is defined twice | Once as a `GameState` default, once in the Difficulty Policy |
| Pause skips cadence periods instead of freezing them | Level and reposition timers keep running on wall-clock time while paused; a period that ends during the pause is lost |
| The game ends when zero snakes are alive | The surviving snake keeps playing alone until it dies; the code comment says "last snake standing" |
