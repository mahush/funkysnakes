// Tests for the Steering Authority (direction_command_filter): evolution of buffered steering intentions.

#include <gtest/gtest.h>

#include <vector>

#include "snake/direction_command_filter.hpp"
#include "snake/game_boundary.hpp"
#include "snake/game_messages.hpp"
#include "snake/snake_model_evolve.hpp"

namespace snake {

namespace {

DirectionCommand steer(const PlayerId& player_id, Direction dir) { return DirectionCommand{player_id, dir}; }

PerPlayerSnakes oneSnake(Direction heading) { return {{PLAYER_A, snake_model::initial(Point{10, 10}, heading, 5)}}; }

/**
 * @brief Read all queued turns of a player by draining a copy of the filter state
 */
std::vector<Direction> queued(direction_command_filter::State state, const PlayerId& player_id) {
  std::vector<Direction> result;
  while (true) {
    auto [next_state, consumed] = direction_command_filter::try_consume_next(state);
    state = next_state;
    auto it = consumed.find(player_id);
    if (it == consumed.end()) {
      break;
    }
    result.push_back(it->second);
  }
  return result;
}

}  // namespace

/// @section direction_command_filter::try_add
///
/// Acceptance and cancellation of buffered steering intentions.

/// @brief Ensures that a perpendicular turn is buffered
TEST(DirectionCommandFilter, QueuesPerpendicularTurn) {
  // When: a perpendicular turn is added
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // Then: it is queued
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP}));
}

/// @brief Ensures that a turn into the current heading is not buffered
TEST(DirectionCommandFilter, RejectsTurnIntoCurrentHeading) {
  // When: a turn into the current heading is added
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::RIGHT));

  // Then: it is rejected
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

/// @brief Ensures that a reverse turn of the current heading is not buffered
TEST(DirectionCommandFilter, RejectsReverseOfHeading) {
  // When: the reverse of the heading is added
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // Then: it is rejected
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

/// @brief Ensures that a new turn is checked against the last queued turn, not the snake's heading
TEST(DirectionCommandFilter, ChecksTurnAgainstLastQueuedTurn) {
  // Given: a turn is queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // When: the reverse of the heading is added
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // Then: it is queued
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP, Direction::LEFT}));
}

/// @brief Ensures that at most two turns are buffered per player
TEST(DirectionCommandFilter, BuffersAtMostTwoTurns) {
  // Given: two turns are queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // When: another valid turn is added
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // Then: it is rejected
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::UP, Direction::LEFT}));
}

/// @brief Ensures that a player can change their mind: cancellation happens before the full-queue check
TEST(DirectionCommandFilter, ReplacesQueueWithOppositeOfFirstQueuedTurn) {
  // Given: two turns are queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::LEFT));

  // When: the opposite of the first queued turn is added
  state = direction_command_filter::try_add(state, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::DOWN));

  // Then: the queue holds only the new turn
  EXPECT_EQ(queued(state, PLAYER_A), (std::vector<Direction>{Direction::DOWN}));
}

/// @brief Ensures that cancellation persists even when the cancelling turn itself is then rejected
TEST(DirectionCommandFilter, KeepsCancellationWhenNewTurnIsRejected) {
  // Given: a turn is queued
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_A, Direction::UP));

  // When: its opposite is added, which reverses the heading
  state = direction_command_filter::try_add(state, oneSnake(Direction::UP), steer(PLAYER_A, Direction::DOWN));

  // Then: the queue is cleared and the new turn is not queued
  EXPECT_TRUE(queued(state, PLAYER_A).empty());
}

/// @brief Ensures that turns of players without a snake are ignored
TEST(DirectionCommandFilter, RejectsTurnOfUnknownPlayer) {
  // When: a turn is added for an unknown player
  direction_command_filter::State state =
      direction_command_filter::try_add({}, oneSnake(Direction::RIGHT), steer(PLAYER_B, Direction::UP));

  // Then: it is rejected
  EXPECT_TRUE(queued(state, PLAYER_B).empty());
}

/// @section direction_command_filter::try_consume_next
///
/// One buffered turn per player is applied per arena step.

namespace {

PerPlayerSnakes twoSnakes() {
  return {{PLAYER_A, snake_model::initial(Point{10, 5}, Direction::RIGHT, 5)},
          {PLAYER_B, snake_model::initial(Point{10, 15}, Direction::RIGHT, 5)}};
}

}  // namespace

/// @brief Ensures that each player's first buffered turn is consumed in one step
TEST(DirectionCommandFilter, ConsumesOneTurnPerPlayer) {
  // Given: turns are queued for both players
  direction_command_filter::State state =
      direction_command_filter::try_add({}, twoSnakes(), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_A, Direction::LEFT));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_B, Direction::DOWN));

  // When: the next turns are consumed
  auto [new_state, consumed] = direction_command_filter::try_consume_next(state);

  // Then: the first turn of each player is consumed
  EXPECT_EQ(consumed, (PerPlayerDirection{{PLAYER_A, Direction::UP}, {PLAYER_B, Direction::DOWN}}));
}

/// @brief Ensures that a second buffered turn is kept for the next step
TEST(DirectionCommandFilter, ConsumesQueuedTurnsInOrder) {
  // Given: two turns are queued and the first was consumed
  direction_command_filter::State state =
      direction_command_filter::try_add({}, twoSnakes(), steer(PLAYER_A, Direction::UP));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_A, Direction::LEFT));
  state = direction_command_filter::try_add(state, twoSnakes(), steer(PLAYER_B, Direction::DOWN));
  state = std::get<0>(direction_command_filter::try_consume_next(state));

  // When: the next turns are consumed
  auto [new_state, consumed] = direction_command_filter::try_consume_next(state);

  // Then: the second turn is consumed
  EXPECT_EQ(consumed, (PerPlayerDirection{{PLAYER_A, Direction::LEFT}}));
}

/// @brief Ensures that nothing is consumed once all buffered turns are applied
TEST(DirectionCommandFilter, ConsumesNothingOnceAllTurnsAreConsumed) {
  // Given: all queued turns were consumed
  direction_command_filter::State state =
      direction_command_filter::try_add({}, twoSnakes(), steer(PLAYER_A, Direction::UP));
  state = std::get<0>(direction_command_filter::try_consume_next(state));

  // When: the next turns are consumed
  auto [new_state, consumed] = direction_command_filter::try_consume_next(state);

  // Then: nothing is consumed
  EXPECT_TRUE(consumed.empty());
}

}  // namespace snake
