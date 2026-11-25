#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "domain/common/game_primitives.hpp"
#include "domain/common/players.hpp"
#include "domain/orbital/game_boundary.hpp"

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
  SELECTED_ROUTE,  // Route the selected order would get if it activated now
  SENT_ROUTE,      // Route the transmitted order would get if it activated now
  ACTIVE_ROUTE,    // Remaining route of the executing order
  FOOD,
  TARGET,  // Target of a selected, transmitted or executing order
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
 * @brief What choosing a successor would earn the player right now, as the Game Authority decides it
 */
using HandoverHint = orbital::game_authority::HandoverProspect;

/**
 * @brief Everything the panel of one player shows
 */
struct PlayerPanel {
  int banked{0};
  int held{0};
  int chain{0};          // Consecutive handovers of the current chain
  int next_handover{0};  // Bonus the next handover would earn
  bool alive{true};
  std::optional<Point> executing_target;
  std::optional<Point> transmitted_target;  // Sent, activates at the next step
  std::optional<Point> selected_target;     // Waits for the next opportunity
  HandoverHint handover_hint{HandoverHint::NONE};
  std::optional<int> handover_flash;  // Bonus of a handover just granted, while it is highlighted
  std::optional<Point> cursor;
};

/**
 * @brief One stretch of the whole game in the uplink overview
 */
struct OverviewRow {
  int opportunities{0};  // Transmission opportunities within the stretch
  int steps{0};          // Steps within the stretch: the most opportunities it could hold
  bool elapsed{false};   // The stretch lies wholly in the past and is not marked now
  bool now{false};       // The next step to run lies within the stretch, or the last step once the game concluded

  bool operator==(const OverviewRow& other) const noexcept {
    return opportunities == other.opportunities && steps == other.steps && elapsed == other.elapsed && now == other.now;
  }
};

/**
 * @brief One coming step in the uplink lane
 */
struct LaneSlot {
  bool opportunity{false};       // A transmission opportunity falls on the step
  std::set<PlayerId> executing;  // Players whose executing order is expected to still run in the step
};

/**
 * @brief The uplink: when transmission opportunities come, at two scales
 *
 * The overview spans the whole game in equal stretches, oldest first; the lane spans the coming
 * steps one by one, the next step first. An executing order's remaining steps are a prediction from
 * its remaining route: it holds if the order is neither interrupted nor its snake dies on the way.
 */
struct UplinkTimeline {
  std::chrono::milliseconds lane_span{0};  // Time the lane covers
  std::vector<OverviewRow> overview;
  std::vector<LaneSlot> lane;
};

// Rows of the uplink beside the board that are not timeline: the top and bottom labels
constexpr int UPLINK_LABEL_ROWS = 2;

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
  UplinkTimeline uplink;  // One overview row and two lane slots per board row the labels leave
  std::chrono::milliseconds remaining;
  std::optional<GameOver> game_over;
  std::map<PlayerId, PlayerPanel> players;  // One per snake in the arena
};

/**
 * @brief Build one frame
 *
 * @param views Views of the Domain Application after the latest interaction
 * @param cursors Cursor position per player
 * @param handover_flashes Bonus per player whose recently granted handover is to be highlighted
 * @return Board cells and panel values
 */
Frame build(const orbital::game_boundary::Views& views,
            const std::map<PlayerId, Point>& cursors,
            const std::map<PlayerId, int>& handover_flashes = {});

}  // namespace orbital_frame
}  // namespace snake
