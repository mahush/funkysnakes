// Tests for the Orbital Controls: keyboard input of the Orbital Orders terminal shell.

#include <gtest/gtest.h>

#include <tuple>
#include <variant>

#include "snake/orbital_controls.hpp"

namespace snake {

namespace {

using orbital::game_boundary::Submit;
using orbital_controls::Cursors;
using orbital_controls::Intent;

const Board BOARD{20, 10};

Cursors makeStartCursors() { return Cursors{{PLAYER_A, Point{4, 2}}, {PLAYER_B, Point{4, 7}}}; }

}  // namespace

/// @section orbital_controls::interpretKey
/// @subsection cursor moves

struct CursorMoveCase {
  Key key;
  PlayerId player;
  Point expected;
};

class OrbitalControlsCursorMove : public ::testing::TestWithParam<CursorMoveCase> {};

TEST_P(OrbitalControlsCursorMove, MovesOwnCursorOnlyWithoutIntent) {
  // When: a movement key is pressed
  auto [cursors, intent] = orbital_controls::interpretKey(BOARD, makeStartCursors(), GetParam().key);

  // Then: only that player's cursor moved, and nothing else is asked for
  Cursors expected = makeStartCursors();
  expected[GetParam().player] = GetParam().expected;
  EXPECT_EQ(cursors, expected);
  EXPECT_FALSE(intent.has_value());
}

INSTANTIATE_TEST_SUITE_P(OrbitalControls,
                         OrbitalControlsCursorMove,
                         ::testing::Values(CursorMoveCase{Key{'w'}, PLAYER_A, Point{4, 1}},
                                           CursorMoveCase{Key{'s'}, PLAYER_A, Point{4, 3}},
                                           CursorMoveCase{Key{'a'}, PLAYER_A, Point{3, 2}},
                                           CursorMoveCase{Key{'d'}, PLAYER_A, Point{5, 2}},
                                           CursorMoveCase{Key{SpecialKey::ARROW_UP}, PLAYER_B, Point{4, 6}},
                                           CursorMoveCase{Key{SpecialKey::ARROW_DOWN}, PLAYER_B, Point{4, 8}},
                                           CursorMoveCase{Key{SpecialKey::ARROW_LEFT}, PLAYER_B, Point{3, 7}},
                                           CursorMoveCase{Key{SpecialKey::ARROW_RIGHT}, PLAYER_B, Point{5, 7}}));

/// @brief The cursor wraps at the board edges like the snakes do
TEST(OrbitalControls, WrapsCursorAtBoardEdge) {
  // When: A's cursor at the top row moves up
  auto [cursors, intent] = orbital_controls::interpretKey(BOARD, Cursors{{PLAYER_A, Point{4, 0}}}, Key{'w'});

  // Then: it appears in the bottom row
  EXPECT_EQ(cursors.at(PLAYER_A), (Point{4, 9}));
}

/// @subsection intents

TEST(OrbitalControls, SubmitsCursorPositionOfPlayerA) {
  // When: A presses the submit key
  auto [cursors, intent] = orbital_controls::interpretKey(BOARD, makeStartCursors(), Key{'e'});

  // Then: A submits the position of their cursor
  ASSERT_TRUE(intent.has_value());
  const Submit& submit = std::get<Submit>(*intent);
  EXPECT_EQ(submit.player, PLAYER_A);
  EXPECT_EQ(submit.target, (Point{4, 2}));
}

TEST(OrbitalControls, SubmitsCursorPositionOfPlayerB) {
  // When: B presses Enter
  auto [cursors, intent] = orbital_controls::interpretKey(BOARD, makeStartCursors(), Key{'\n'});

  // Then: B submits the position of their cursor
  ASSERT_TRUE(intent.has_value());
  const Submit& submit = std::get<Submit>(*intent);
  EXPECT_EQ(submit.player, PLAYER_B);
  EXPECT_EQ(submit.target, (Point{4, 7}));
}

TEST(OrbitalControls, AsksForNewGame) {
  // When: 'n' is pressed
  auto [cursors, intent] = orbital_controls::interpretKey(BOARD, makeStartCursors(), Key{'n'});

  // Then: a new game is asked for
  ASSERT_TRUE(intent.has_value());
  EXPECT_TRUE(std::holds_alternative<orbital_controls::NewGame>(*intent));
}

TEST(OrbitalControls, AsksToQuit) {
  // When: 'q' is pressed
  auto [cursors, intent] = orbital_controls::interpretKey(BOARD, makeStartCursors(), Key{'q'});

  // Then: quitting is asked for
  ASSERT_TRUE(intent.has_value());
  EXPECT_TRUE(std::holds_alternative<orbital_controls::Quit>(*intent));
}

TEST(OrbitalControls, IgnoresUnboundKey) {
  // When: an unbound key is pressed
  auto [cursors, intent] = orbital_controls::interpretKey(BOARD, makeStartCursors(), Key{'x'});

  // Then: nothing changes and nothing is asked for
  EXPECT_EQ(cursors, makeStartCursors());
  EXPECT_FALSE(intent.has_value());
}

}  // namespace snake
