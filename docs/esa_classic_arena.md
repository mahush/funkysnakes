# Classic Arena Authority — ownership and behavior map

This document maps the classic funkysnakes arena onto the semantic roles of Executable Semantic
Architecture (ESA): who owns which meaningful behavior, what is in and out of the first extracted scope,
and which current behaviors are pinned as-is pending explicit rule decisions.

Extraction baseline: commit `f92928b` (branch `esa-adaption`). Gameplay was preserved during extraction;
see [Pinned behavior](#pinned-behavior).

## Scope

**In scope: the classic arena.** Snakes, scores, food and buffered steering intentions on a board, and the
complete arena step.

**Out of scope (for now): game lifecycle, pause, level progression, tick cadence and game conclusion.**
These currently live in `GameManagerActor` and `GameEngineActor`'s clock handling.

Why the arena is its own ownership scope rather than a part of a larger game owner:

- arena rules never read lifecycle state: movement, collisions, eating and food never consult level,
  pause or phase;
- lifecycle reaches the arena only from outside: it decides *when* steps happen and *requests* a food
  reposition;
- lifecycle reads exactly one arena outcome: which snakes are alive.

The current actor split is consistent with this, but it is not the evidence: actor boundaries do not
determine Authority boundaries. A higher Game Authority that owns lifecycle and includes the arena
remains possible later (Authorities are recursive).

Orbital Orders will be a different arena with partly different rules. It is expected to share code with
the classic arena (snake evolution, geometry, collision checks with equal meaning) while each arena keeps
its own owner for the rules that differ.

## Ownership map

| ESA role | Code | Owns |
|---|---|---|
| **Classic Arena Authority** | [`classic_arena_authority`](../include/snake/classic_arena_authority.hpp) | The complete arena step and its meaningful sequencing; how collisions and eating change scores; initial arena setup |
| Snake Authority (nested) | [`snake_model`](../include/snake/snake_model.hpp) | Evolution of one snake's body and life: move, grow, cut, kill, edge wrapping, ignoring reverse turns |
| Steering Authority (nested) | [`direction_command_filter`](../include/snake/direction_command_filter.hpp) | Evolution of buffered steering intentions: acceptance, cancellation, queue limit, one turn per step |
| Policies | — | None in this scope yet (see [Candidates](#policy-candidates)) |

The game-rule helpers in [`game_logic`](../include/snake/game_logic.hpp) and
[`snake_predicates`](../include/snake/snake_predicates.hpp) are not separate semantic owners. They are
building blocks of the arena step, and the Classic Arena Authority decides how they are composed.

### Authority surface

```cpp
State initial(const RandomIntGeneratorFn& random_int, Board board);
State steer(State state, const DirectionCommand& cmd);
State requestFoodReposition(State state);
State tick(const RandomIntGeneratorFn& random_int, State state);
```

All transitions return the resulting state only (no echo outputs). Consumers read results from the state
(snakes, scores, food; alive states via `extractAliveStates`).

`State` is a plain struct: fields are freely readable, but changes go through these transitions by
convention. The lenses over its fields are private to the Authority implementation, so the arena step can
only be composed in [`classic_arena_authority.cpp`](../src/classic_arena_authority.cpp). Stronger
protection (opaque state with friend queries, as in `snake_model::Snake`) is a possible later step.

### Arena step (`tick`)

The order is meaningful and owned by the Authority:

1. consume at most one buffered turn per player;
2. move every alive snake (grow when the next head is on food);
3. resolve collisions: self-bites first, then snake against snake;
4. drop cut tail segments as food;
5. drop the bodies of snakes that died in this step as food (once, not again in later steps);
6. eat food under alive snake heads;
7. replenish food up to `MIN_FOOD_COUNT`;
8. if requested, reposition one random food item, then clear the request.

Steps 4 and 5 only run in `BITE_DROP_FOOD` mode, which is currently the only mode ever used.

## Inputs and facts

| Input | Origin | Meaning for the arena |
|---|---|---|
| Steering command | Player input (`InputActor`) | A requested turn; acceptance is the Steering Authority's decision |
| Step | Game clock (`GameEngineActor` timer) | Advance the arena once; *when* steps happen is lifecycle-owned |
| Reposition request | `GameManagerActor` (every 5 s, not while paused) | Reposition one food item on the next step; how often is lifecycle-owned |
| Random source | Shell (`makeRandomIntGenerator`) | External fact supplying food positions and the repositioned item; passed into `initial` and `tick` |

## Realization state kept outside the arena

`GameState` in [`game_messages.hpp`](../include/snake/game_messages.hpp) holds the arena next to state
that belongs to the actor's realization, not to arena meaning:

- `game_id`: message routing and filtering stale triggers;
- `interval_ms`: timer configuration, driven by the manager's level-to-speed formula;
- `previous_alive_states`: decides *when* to publish `PlayerAliveStatesMsg`.

## Pinned behavior

Characterized as current behavior in
[`test_classic_arena_authority.cpp`](../tests/test_classic_arena_authority.cpp). Some of these look
unintended. Changing any of them is a separate, explicit gameplay decision, not part of the
refactor.

| Behavior | Notes |
|---|---|
| A biter eats the first cut segment in the same step | An ordering effect (collisions → drops → eating): the victim loses 10 and the biter gains 10, without growing that step |
| Only the bitten snake loses points; a head-on hit or mutual bite kills both (−10 each) | Snake-against-snake checks are hard-wired to Player A and Player B |
| New food may be placed on existing food | Only snake cells are avoided, with up to 100 attempts and then an unchecked position |
| Steering cancellation persists even when the new turn is then rejected | Cancellation (opposite of the first queued turn) happens before the other acceptance checks |
| `BITE_REMOVE_TAIL` is never used | The mode is always `BITE_DROP_FOOD` |

Out of scope, but known: the manager ends the game when **zero** snakes are alive, while its comment
says "last snake standing".

## Policy candidates

Not extracted in this scope, but recorded:

- **Difficulty**: level → step interval (`max(50, 200 − 15·(level − 1))` ms). A clear stateless Policy,
  but owned on the lifecycle side.
- **Food placement**: where new food goes and which item gets repositioned. It could become a Policy once
  random draws are explicit facts rather than a generator function.
- **Scoring** (+10 eat, −10 bitten or dead): currently only meaningful as part of the eating and collision
  transitions, so it stays in the Authority.

## Tests and local execution

Tests target owners through their public transitions only, so they stay valid however the arena step is
implemented internally:

| Test file | Owner under test |
|---|---|
| [`test_classic_arena_authority.cpp`](../tests/test_classic_arena_authority.cpp) | Classic Arena Authority: `initial`, `steer`, `requestFoodReposition`, `tick` |
| [`test_snake_model.cpp`](../tests/test_snake_model.cpp) | Snake Authority (`snake_model`) |
| [`test_direction_command_filter.cpp`](../tests/test_direction_command_filter.cpp) | Steering Authority (`direction_command_filter`) |

The game-rule helpers in `game_logic` are not tested directly; their behavior is pinned through complete
arena steps. Two helper-level cases are unreachable through the Authority and are therefore not pinned:
a shared food cell under both heads (equal heads are a head-on collision first) and repositioning
without food (replenishment always runs first).

The Classic Arena Authority runs without actors, timers or rendering: the arena tests run complete arena
histories directly against `classic_arena_authority`, with a scripted random source. `GameEngineActor`
calls the same transitions in production, so there is no second interpretation of the arena rules.

```bash
./build/test_snake --gtest_filter='ClassicArenaAuthority.*'
```
