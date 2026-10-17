#pragma once

#include "ftxui/dom/elements.hpp"
#include "snake/orbital_frame.hpp"

namespace snake {
namespace orbital_view {

/**
 * Orbital View - how the Orbital Orders terminal looks
 *
 * Imperative-shell helper without game meaning: it draws an Orbital Frame as FTXUI elements.
 * Cell symbols: '.' pending route, '+' active route, '*' food, '?' pending target, '@' executing
 * target, 'a'/'b' tail, 'A'/'B' head and cursor of Player A/B, 'x' head of a dead snake.
 * Right of the board: the link delay as a wedge, narrow and fast at the bottom, wide and slow at
 * the top, filled up to the current delay. Below the board: time left and a line per player.
 */

/**
 * @brief Draw one frame
 *
 * @param frame Frame to draw
 * @return The whole screen: title, board with link delay, status lines and key help
 */
ftxui::Element draw(const orbital_frame::Frame& frame);

}  // namespace orbital_view
}  // namespace snake
