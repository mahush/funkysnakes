#pragma once

#include <map>
#include <string>
#include <vector>

#include "common/game_primitives.hpp"
#include "common/players.hpp"
#include "orbital/game_boundary.hpp"

namespace snake {
namespace orbital_frame {

/**
 * Orbital Frame - terminal presentation of the Orbital Orders views
 *
 * Imperative-shell helper without game meaning: it projects the boundary's views and the players'
 * cursors into text lines. Cell symbols, lowest to highest precedence:
 *   '.' route a pending order would get now, '+' remaining route of the executing order, '*' food,
 *   '?' pending target, '@' executing target, 'a'/'b' tail, 'A'/'B' head and cursor of Player A/B,
 *   'x' head of a dead snake
 * Right of the board: the link delay as a wedge, narrow and fast at the bottom, wide and slow at
 * the top, filled up to the current delay. Below the board: time left and a line per player.
 */

/**
 * @brief Render one frame
 *
 * @param views Views of the Domain Application after the latest interaction
 * @param cursors Cursor position per player
 * @return Lines of the frame: board with border, then the status panel
 */
std::vector<std::string> render(const orbital::game_boundary::Views& views, const std::map<PlayerId, Point>& cursors);

}  // namespace orbital_frame
}  // namespace snake
