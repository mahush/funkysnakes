#pragma once

#include <map>
#include <optional>
#include <tuple>
#include <variant>

#include "common/game_primitives.hpp"
#include "common/players.hpp"
#include "orbital/game_boundary.hpp"
#include "snake/key.hpp"

namespace snake {
namespace orbital_controls {

/**
 * Orbital Controls - keyboard input of the Orbital Orders terminal shell
 *
 * Imperative-shell helper without game meaning: each player moves a cursor over the board and
 * submits its position as an order target.
 *   Player A: w/a/s/d move the cursor, e submits
 *   Player B: arrow keys move the cursor, Enter submits
 *   n starts a new game, q quits
 */

/**
 * @brief The cursor position of each player
 */
using Cursors = std::map<PlayerId, Point>;

/**
 * @brief Cursors at the start of a game: each player's cursor on their own snake's head
 *
 * @param views Views of a started game
 * @return Cursor per player
 */
Cursors makeCursors(const orbital::game_boundary::Views& views);

/**
 * @brief Start a new game
 */
struct NewGame {};

/**
 * @brief Leave the program
 */
struct Quit {};

/**
 * @brief What a key asks the shell to do besides moving a cursor
 */
using Intent = std::variant<orbital::game_boundary::Submit, NewGame, Quit>;

/**
 * @brief Interpret a key
 *
 * @param board Board the cursors move on; they wrap at its edges like the snakes
 * @param cursors Cursors before the key
 * @param key The pressed key
 * @return Tuple of (cursors after the key, intent of the key if any)
 */
std::tuple<Cursors, std::optional<Intent>> interpretKey(const Board& board, Cursors cursors, const Key& key);

}  // namespace orbital_controls
}  // namespace snake
