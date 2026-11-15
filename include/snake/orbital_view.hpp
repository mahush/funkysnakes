#pragma once

#include "ftxui/dom/elements.hpp"
#include "snake/orbital_frame.hpp"

namespace snake {
namespace orbital_view {

/**
 * Orbital View - how the Orbital Orders terminal looks
 *
 * Imperative-shell helper without game meaning: it draws an Orbital Frame as FTXUI elements.
 * Layout: the board, titled with the time left or the result; right of it the uplink: two timelines
 * from bottom to top: the whole game, each row shaded by the share of its steps that are opportunities
 * ('░▒▓█', elapsed rows dimmed, '▶' at now), and beside it the coming steps, a step per half row,
 * with '▀▄█' for opportunities and, in the players' colours, a bar '┃' for the remaining execution of
 * their orders up to the expected arrival; below them a panel per player and the key help. Each board cell is two
 * columns wide, so that a square board looks square. Colours come from the Gruvbox dark palette: blue borders, a grey
 * uplink, purple and yellow snakes. Cell symbols, in the colour of their player: '●' snake, '⬤' head, '✖' head of a
 * dead snake, '·' selected route (faded), '·' sent route, '•' active route, '◎' target, '✚' cursor; '◆' food in green.
 */

/**
 * @brief Draw one frame
 *
 * @param frame Frame to draw
 * @return The whole screen: board with uplink, player panels and key help
 */
ftxui::Element draw(const orbital_frame::Frame& frame);

}  // namespace orbital_view
}  // namespace snake
