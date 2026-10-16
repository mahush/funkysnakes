#pragma once

#include <map>
#include <optional>

#include "common/game_primitives.hpp"
#include "common/game_types.hpp"
#include "orbital/game_boundary.hpp"
#include "snake/key.hpp"

namespace snake {
namespace orbital_food_labels {

/**
 * Orbital Food Labels - digits that name the food on the board
 *
 * Imperative-shell helper without game meaning: Player A orders a route to a food item by pressing
 * its digit, as an alternative to moving the cursor there. The target stays a position; the label
 * only spares aiming. A label sticks to its food item until the item is gone, so that a digit
 * read a moment ago still names the same food. Food beyond ten items has no label.
 */

/**
 * @brief Board position of each label, labels 0 to 9
 */
using FoodLabels = std::map<int, Point>;

/**
 * @brief Labels for the food now on the board
 *
 * Food that kept its position keeps its label; newly placed food takes the lowest free labels.
 *
 * @param labels Labels before
 * @param food Food now on the board
 * @return Labels after
 */
FoodLabels relabel(FoodLabels labels, const FoodItems& food);

/**
 * @brief Interpret a key as Player A ordering a route to labelled food
 *
 * @param labels Current labels
 * @param key The pressed key
 * @return Submission of the labelled food's position, if the key is the digit of a label
 */
std::optional<orbital::game_boundary::Submit> trySubmitFood(const FoodLabels& labels, const Key& key);

}  // namespace orbital_food_labels
}  // namespace snake
