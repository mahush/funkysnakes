#pragma once

#include "orbital/arena_authority.hpp"
#include "orbital/orbital_game_domain_application.hpp"

namespace snake {
namespace orbital {
namespace orbital_game_domain_application {

/**
 * @brief Test setup seam of the Domain Application: start the game in a hand-picked world
 *
 * Part of neither interface. It is no interaction, since no player chooses the world, and no
 * execution input, since no production run supplies one: a hand-picked world replaces the Board
 * Setup Policy's answer instead of supplying what the owners depend on but no counterpart controls.
 * It lives with the Domain Application only because it has to set the game's internal state.
 *
 * Histories that start through this seam are reference-only: their starting world (board size, snake
 * starts, food) cannot be reached by driving the game, so they cannot be replayed against a
 * realization, which always starts in its initial state. Histories meant for comparison with a
 * realization start the game through the boundary, script the random source and use only the two
 * interfaces. After the start, a history through this seam uses the game only through its interfaces
 * as well.
 */
struct TestAccess {
  /**
   * @brief Begin the game in a given world instead of the Board Setup Policy's start layout
   *
   * Admitted as the Admission Policy decides, like the players' start: only before a game has
   * started. Reported through the boundary like the players' start: the views of the started game,
   * or StartRejected with the views of the game that already started, which stays unchanged.
   *
   * @param game The game to start
   * @param setup World at round start; its round identity names the game
   */
  static void startWithSetup(DomainApplication& game, const arena_authority::RoundSetup& setup) { game.take(setup); }
};

}  // namespace orbital_game_domain_application
}  // namespace orbital
}  // namespace snake
