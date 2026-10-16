#pragma once

#include "ftxui/dom/elements.hpp"
#include "snake/orbital_frame.hpp"

namespace snake {
namespace orbital_view {

/**
 * Orbital View - how the Orbital Orders terminal looks
 *
 * Imperative-shell helper without game meaning: it draws an Orbital Frame as FTXUI elements.
 * Layout: the board, titled with the time left or the result; right of it the link delay as a
 * wedge, narrow and fast at the bottom, wide and slow at the top, filled up to the current delay;
 * below them a panel per player and the key help. Each board cell is two columns wide, so that a
 * square board looks square.
 * Colours come from the Gruvbox dark palette: blue borders, a grey link delay, purple and yellow snakes.
 * Cell symbols, in the colour of their player: '●' snake, '⬤' head, '✖' head of a dead snake,
 * '·' pending route (faded), '•' active route, '◎' target, '✚' cursor; '◆' food in green, or its label digit if it has
 * one.
 */

/**
 * @brief Draw one frame
 *
 * @param frame Frame to draw
 * @return The whole screen: board with link delay, player panels and key help
 */
ftxui::Element draw(const orbital_frame::Frame& frame);

}  // namespace orbital_view
}  // namespace snake
