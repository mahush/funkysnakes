#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <vector>

#include "common/game_primitives.hpp"
#include "common/players.hpp"
#include "orbital/game_boundary.hpp"

namespace snake {
namespace orbital_frame {

/**
 * Orbital Frame - what the Orbital Orders terminal shows, without how it looks
 *
 * Imperative-shell helper without game meaning: it projects the boundary's views and the players'
 * cursors into a frame of board cells and panel values. The Orbital View decides symbols, colours
 * and layout.
 */

/**
 * @brief What a board cell shows, lowest to highest precedence
 */
enum class CellRole {
  EMPTY,
  PENDING_ROUTE,  // Route a pending order would get if it activated now
  ACTIVE_ROUTE,   // Remaining route of the executing order
  FOOD,
  TARGET,  // Target of a pending or executing order
  SNAKE_BODY,
  SNAKE_HEAD,
  DEAD_HEAD,  // Head of a dead snake
  CURSOR
};

/**
 * @brief One board cell: its role and the player it belongs to, if any
 */
struct Cell {
  CellRole role{CellRole::EMPTY};
  std::optional<PlayerId> player;

  bool operator==(const Cell& other) const noexcept { return role == other.role && player == other.player; }
};

/**
 * @brief Board cells, indexed [y][x]
 */
using CellGrid = std::vector<std::vector<Cell>>;

/**
 * @brief Everything the panel of one player shows
 */
struct PlayerPanel {
  int credited{0};
  int unbanked{0};
  bool alive{true};
  std::optional<Point> executing_target;
  std::optional<Point> pending_target;
  std::chrono::milliseconds pending_remaining{0};  // Time until the pending order may activate
  std::optional<Point> cursor;
};

/**
 * @brief Link delay of a submission issued now, within its range
 */
struct DelayGauge {
  std::chrono::milliseconds now;
  std::chrono::milliseconds min;
  std::chrono::milliseconds max;
};

/**
 * @brief Outcome once the game is over: the winner, or nothing for a draw
 */
struct GameOver {
  std::optional<PlayerId> winner;
};

/**
 * @brief One frame of the Orbital Orders terminal
 */
struct Frame {
  CellGrid cells;
  DelayGauge delay;
  std::chrono::milliseconds remaining;
  std::optional<GameOver> game_over;
  std::map<PlayerId, PlayerPanel> players;  // One per snake in the arena
};

/**
 * @brief Build one frame
 *
 * @param views Views of the Domain Application after the latest interaction
 * @param cursors Cursor position per player
 * @return Board cells and panel values
 */
Frame build(const orbital::game_boundary::Views& views, const std::map<PlayerId, Point>& cursors);

}  // namespace orbital_frame
}  // namespace snake
