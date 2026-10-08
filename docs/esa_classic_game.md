# Classic snake — semantic artifacts

This document maps classic funkysnakes onto the semantic roles of Executable Semantic Architecture (ESA): who
owns which meaningful behavior, how the owners are composed, what the game's boundary is, and which current
behaviors are pinned as-is pending explicit rule decisions.

Extraction baseline: commit `f92928b` (branch `esa-adaption`). Gameplay was preserved during extraction,
except for the deliberate changes listed under [Gameplay changes](#gameplay-changes).

Status: extracted are the Classic Arena Authority with its nested Snake and Steering Authorities, the Classic
Game Lifecycle Authority, the Scoring and Difficulty Policies, the Domain System Boundary and the Classic Game
Domain Application. Production (`GameEngineActor`, `GameManagerActor`) feeds messages and timer events into the
Authorities and carries out their intents.

## Overview

| Artifact | ESA form | Code | Owns |
|---|---|---|---|
| [Classic Arena Authority](#classic-arena-authority) | Authority | [`classic_arena_authority`](../semantic_artifacts/include/classic/classic_arena_authority.hpp) | Snakes, scores, food, steering intentions and the complete arena step |
| Snake Authority | Nested Authority | [`snake_model`](../semantic_artifacts/include/classic/snake_model.hpp) | Evolution of one snake's body and life: move, grow, cut, kill, edge wrapping, ignoring reverse turns |
| Steering Authority | Nested Authority | [`direction_command_filter`](../semantic_artifacts/include/classic/direction_command_filter.hpp) | Evolution of buffered steering intentions: acceptance, cancellation, queue limit, one turn per step |
| [Scoring Policy](#scoring-policy) | Policy, used by the arena | [`scoring_policy`](../semantic_artifacts/include/classic/scoring_policy.hpp) | What arena events are worth |
| [Classic Game Lifecycle Authority](#classic-game-lifecycle-authority) | Authority, beside the arena | [`classic_game_lifecycle_authority`](../semantic_artifacts/include/classic/classic_game_lifecycle_authority.hpp) | Pause, conclusion, level and game-time cadences |
| [Difficulty Policy](#difficulty-policy) | Policy, used by the lifecycle | [`difficulty_policy`](../semantic_artifacts/include/classic/difficulty_policy.hpp) | Level → step interval |
| [Domain System Boundary](#domain-system-boundary) | Boundary semantics | [`game_boundary`](../semantic_artifacts/include/classic/game_boundary.hpp) | The game's external interactions, independent of devices |
| [Domain Application](#domain-application) | Canonical single-process realization | [`classic_game_domain_application`](../semantic_artifacts/include/classic/classic_game_domain_application.hpp) | Nothing of its own: mechanical composition |

Not domain semantics: the [player input adapter](#player-input-adapter) and the
[realization mechanics](#realization-mechanics) between actors.

## Classic Arena Authority

### Scope

**The arena:** snakes, scores, food and buffered steering intentions on a board, and the complete arena
step. Game lifecycle, pause, level progression, step cadence and conclusion belong to the
[lifecycle Authority](#classic-game-lifecycle-authority) beside the arena.

Why the arena is its own ownership scope rather than a part of a larger game owner:

- arena rules never read lifecycle state: movement, collisions, eating and food never consult level,
  pause or conclusion;
- lifecycle reaches the arena only from outside: it decides *when* steps happen and *requests* a food
  reposition;
- lifecycle reads exactly two arena views: alive states (for conclusion) and final scores (for the summary).

The current actor split is consistent with this, but it is not the evidence: actor boundaries do not
determine Authority boundaries.

Orbital Orders will be a different arena with partly different rules. It is expected to share code with
the classic arena (snake evolution, geometry, collision checks with equal meaning) while each arena keeps
its own owner for the rules that differ.

The game-rule helpers in [`game_logic`](../semantic_artifacts/include/classic/game_logic.hpp) and
[`snake_predicates`](../semantic_artifacts/include/classic/snake_predicates.hpp) are not separate semantic owners. They are
reusable building blocks of the arena step, and the Classic Arena Authority decides how they are composed.
They still contain rules (who loses on a bite, one food item per cell), so they live with the semantic
artifacts under `semantic_artifacts/` and are part of the reviewed semantic surface. The semantic artifacts
depend on nothing outside that folder except the generic lens and pipe utilities, which are mechanics.

### Authority surface

```cpp
State initial(const RandomIntGeneratorFn& random_int, Board board);
State steer(State state, const DirectionCommand& cmd);
State requestFoodReposition(State state);
State tick(const RandomIntGeneratorFn& random_int, State state);
```

All transitions return the resulting state only (no echo outputs). Consumers read results from the state
(snakes, scores, food; alive states via `extractAliveStates`; `events` for what happened in the last step,
which the resulting state cannot otherwise show).

`State` is a plain struct: fields are freely readable, but changes go through these transitions by
convention. The lenses over its fields are private to the Authority implementation, so the arena step can
only be composed in [`classic_arena_authority.cpp`](../semantic_artifacts/src/classic/classic_arena_authority.cpp). Stronger
protection (opaque state with friend queries, as in `snake_model::Snake`) is a possible later step.

### Arena step (`tick`)

The order is meaningful and owned by the Authority:

1. start with no events, then consume at most one buffered turn per player;
2. move every alive snake (grow when the next head is on food);
3. resolve collisions: self-bites first, then snake against snake; report them as events and collect the
   segments leaving play (cut tails and the complete bodies of snakes killed in this step);
4. drop those segments as food;
5. eat food under alive snake heads and report it as events;
6. apply the Scoring Policy to the step's events;
7. replenish food up to `MIN_FOOD_COUNT` on free cells (up to 100 random attempts per item; if none is free,
   fewer items are placed this step);
8. if requested, reposition one random food item, then clear the request.

Step 4 only runs in `BITE_DROP_FOOD` mode, which is currently the only mode ever used.

A cell holds at most one food item: placed and dropped food skips cells that already hold food, and placed
food also avoids snake cells.

### Arena events

Arena rules report what happened as [events](../semantic_artifacts/include/classic/arena_events.hpp) instead of deciding their
consequences: `FoodEaten`, `Bitten` (victim and biter), `SelfBitten` and `MutualBite` (head-on or mutual tail
bites). The [Scoring Policy](#scoring-policy) turns them into score changes, so the collision and eating helpers
know nothing about points and can be reused by an arena with different scoring.

### Inputs and facts

| Input | Origin | Meaning for the arena |
|---|---|---|
| Steering command | Players end (`Steer`) | A requested turn; acceptance is the Steering Authority's decision |
| Step | Step clock | Advance the arena once; *when* steps happen is lifecycle-owned |
| Reposition request | Lifecycle Authority (every 5 s of game time) | Reposition one food item on the next step |
| Random source | Realization (`makeRandomIntGenerator`) | External fact supplying food positions and the repositioned item; passed into `initial` and `tick` |

## Scoring Policy

A stateless mapping from the arena events of one step to score changes:

- +10 for each `FoodEaten`;
- −10 for the victim of a `Bitten` (the biter gets nothing from the bite itself);
- −10 for a `SelfBitten`;
- −10 for each snake in a `MutualBite`.

## Classic Game Lifecycle Authority

Owns the evolution of a game as a whole. Its transitions are `start`, `togglePause`, `levelPeriodElapsed`,
`repositionPeriodElapsed`, `observeAliveStates` and `concluded`; each returns the new state plus intents
(clock, step interval, cadences, reposition, conclude) only where the state does not already encode them.
Elapsed cadence periods arrive as events from the realization; the Authority decides what they mean.

- **pause and conclusion** are two independent flags, `paused` and `over`, not one phase. The game can
  therefore still be paused and resumed once it is over (see [Pinned behavior](#pinned-behavior));
- **level**: starts at the requested level and rises once per level period;
- **conclusion**: decides when the game is over, based on the arena's alive states. Currently the game is
  over when **zero** snakes are alive. This check lives inside the Authority rather than in a separate
  Policy for now;
- **game-time cadences**: level-up every 60 s and food reposition every 5 s. Game time stops while the
  game is paused: `togglePause` returns a `FREEZE` or `RESUME` cadence intent, so a period interrupted by
  a pause continues afterwards with its remaining part. Once the game is over, the cadences stay stopped and
  elapsed periods are ignored;
- **same-instant order**: `SAME_INSTANT_ORDER` defines how work due at the same instant counts: the arena
  step first, then the level period, then the reposition period. A step that ends the game therefore counts
  before a level-up at the same moment, and the level-up is then ignored because the game is over.

### Beside the arena, not above it

The lifecycle Authority and the Classic Arena Authority are independent owners connected by plain
composition:

- the arena never reads lifecycle state; it receives "step" and "reposition" as inputs;
- lifecycle reads exactly two arena views: alive states (for conclusion) and final scores (for the
  summary). Both are read-only projections, not a shared state change;
- "game over → stop stepping" is a lifecycle decision that shows up only as which inputs the arena receives.

So no higher Authority owns a combined truth, and no Protocol Contract is needed. The summary
request/response between the actors is a realization round trip; the Domain Application simply reads the
arena's view.

Decoupling also lets the same lifecycle owner sit beside a different arena, such as Orbital Orders.

Reconsider this placement if a rule turns up that needs both truths within one step. No such rule exists
today: the same-instant order only sequences the lifecycle's own inputs and needs no arena truth.

## Difficulty Policy

A stateless decision mapping a level to the arena's step interval:

```text
interval_ms(level) = max(50, 200 − 15 · (level − 1))
```

## Domain System Boundary

The external view of the classic game, expressed as intents rather than devices. The types are defined in
[`game_boundary.hpp`](../semantic_artifacts/include/classic/game_boundary.hpp) and shared by both realizations: the Domain
Application uses them as its interface, and production's edge messages carry them as their payload next to
routing fields such as `game_id`.

**Players end**

| Direction | Interaction | Meaning |
|---|---|---|
| inbound | `Start(starting_level)` | Begin a new game |
| inbound | `Steer(player, direction)` | A player's intended turn |
| inbound | `TogglePause` | Pause or resume the game |
| outbound | `ArenaView` | Board, snakes, food and scores after each step |
| outbound | `Status` | Current level and whether the game is paused |
| outbound | `GameOver` | Final scores and final level |

**External facts**: elapsed time (`TimeElapsed`) and randomness (food placement). Both are supplied by the
realization.

## Domain Application

[`classic_game_domain_application`](../semantic_artifacts/include/classic/classic_game_domain_application.hpp) is the canonical
single-process realization of classic snake. It composes both Authorities and their Policies without actors,
timers or rendering:

- the boundary interactions above are its interface: `apply(state, Start)`, `apply(state, Steer)`,
  `apply(state, TogglePause)` and `apply(random_int, state, TimeElapsed)`, each returning the new state plus
  the observations (arena views, statuses, game-over summary);
- it routes interactions to the Authorities and carries out their intents mechanically: clock, step interval,
  cadences, reposition and conclusion;
- the summary round trip between the actors disappears: on conclusion the final scores are read from the arena.

**Game time** comes in as `TimeElapsed`. A virtual clock accumulates it and runs whatever falls due. The clock
is mechanics: the periods, the cadence intents and the same-instant order come from the lifecycle Authority,
and the step interval from the Difficulty Policy. The Domain Application makes no gameplay decision of its own.

[`test_classic_game_domain_application.cpp`](../tests/test_classic_game_domain_application.cpp) runs complete
histories of boundary interactions against it (the Domain Harness).

### Comparison with production

Production used to lose cadence time during a pause: the level and reposition timers kept running on wall-clock
time, so periods that ended while paused were skipped. Comparing it with the Domain Application exposed this
as a realization bug. `GameManagerActor` now carries out the lifecycle Authority's `FREEZE` and `RESUME`
cadence intents: it cancels the cadence timers and later resumes them with the remaining part of each period.
This is covered by a manual run only, since a test would have to wait for real cadence periods.

Production does not enforce the same-instant order: independent asio timers in two actors fire in whichever
order they happen to run, and the game over reaches the manager only after an alive-states message. A level
period ending between the deciding step and that message can still raise the final level. This is a known
realization gap, visible only at millisecond coincidences.

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
- `GameState` in [`game_messages.hpp`](../include/snake/game_messages.hpp), which holds the arena next to
  realization state of `GameEngineActor`: `game_id`, `interval_ms` (timer configuration, set from the lifecycle
  Authority's step interval intents) and `previous_alive_states` (decides *when* to publish alive states);
- the messages between the manager and the engine, kept separately in
  [`engine_manager_messages.hpp`](../include/snake/engine_manager_messages.hpp): clock commands, step
  interval changes, reposition triggers, alive states and the summary request/response;
- timer commands, restarting the step timer on a tick-rate change, the cadence bookkeeping that keeps the
  remaining part of a period across a pause, shutdown and logging;
- rendering, including the flashing game-over text and dead snakes not being drawn.

## Gameplay changes

Deliberate changes after characterization, each made separately:

- a dead snake's body becomes food once, in the step it dies (it was dropped again on every step);
- at most one food item per cell (new and dropped food could stack);
- the game starts at the speed of its starting level;
- game time stops while paused, also in production;
- cadence periods ending after the game is over are ignored, and the step counts first at the same instant.

## Pinned behavior

Current behavior that looks unintended or inconsistent, preserved during extraction. Changing it is a separate,
explicit gameplay decision.

| Behavior | Notes |
|---|---|
| A biter eats the first cut segment in the same step | An ordering effect (collisions → drops → eating): the victim loses 10 and the biter gains 10, without growing that step |
| Only the bitten snake loses points; a head-on hit or mutual bite kills both (−10 each) | Snake-against-snake checks are hard-wired to Player A and Player B |
| Steering cancellation persists even when the new turn is then rejected | Cancellation (opposite of the first queued turn) happens before the other acceptance checks |
| `BITE_REMOVE_TAIL` is never used | The mode is always `BITE_DROP_FOOD` |
| Toggling pause twice after game over restarts the step clock | Pause and conclusion are independent flags; RESUME restarts the step timer. Level-up and reposition stay stopped |
| The game ends when zero snakes are alive | The surviving snake keeps playing alone until it dies; the code comment says "last snake standing" |

## Policy candidates

Not extracted yet:

- **Arena setup**: board size, start positions, directions and lengths of the snakes.
- **Food placement**: where new food goes and which item gets repositioned. It could become a Policy once
  random draws are explicit facts rather than a generator function.

## Tests

Tests target owners through their public transitions only, so they stay valid however the owners are
implemented internally:

| Test file | Under test |
|---|---|
| [`test_classic_arena_authority.cpp`](../tests/test_classic_arena_authority.cpp) | Classic Arena Authority: `initial`, `steer`, `requestFoodReposition`, `tick` |
| [`test_snake_model.cpp`](../tests/test_snake_model.cpp) | Snake Authority (`snake_model`) |
| [`test_direction_command_filter.cpp`](../tests/test_direction_command_filter.cpp) | Steering Authority (`direction_command_filter`) |
| [`test_scoring_policy.cpp`](../tests/test_scoring_policy.cpp) | Scoring Policy |
| [`test_classic_game_lifecycle_authority.cpp`](../tests/test_classic_game_lifecycle_authority.cpp) | Classic Game Lifecycle Authority |
| [`test_difficulty_policy.cpp`](../tests/test_difficulty_policy.cpp) | Difficulty Policy |
| [`test_classic_game_domain_application.cpp`](../tests/test_classic_game_domain_application.cpp) | Domain Application (Domain Harness) |

The game-rule helpers in `game_logic` are not tested directly; their behavior is pinned through complete
arena steps. Two helper-level cases are unreachable through the Authority and are therefore not pinned:
a shared food cell under both heads (equal heads are a head-on collision first) and repositioning
without food (replenishment always runs first).

```bash
./build/test_snake --gtest_filter='ClassicArenaAuthority.*:ClassicGameLifecycleAuthority.*:ClassicGameDomainApplication.*'
```
