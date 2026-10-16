// Tests for the Orbital Food Labels: digits that name the food on the board.

#include <gtest/gtest.h>

#include <optional>

#include "snake/orbital_food_labels.hpp"

namespace snake {

namespace {

using orbital::game_boundary::Submit;
using orbital_food_labels::FoodLabels;

}  // namespace

/// @section orbital_food_labels::relabel

TEST(OrbitalFoodLabels, LabelsFoodInOrderFromZero) {
  // When: three food items are labelled for the first time
  FoodLabels labels = orbital_food_labels::relabel({}, {Point{1, 1}, Point{2, 2}, Point{3, 3}});

  // Then: they get the labels 0, 1 and 2
  EXPECT_EQ(labels, (FoodLabels{{0, Point{1, 1}}, {1, Point{2, 2}}, {2, Point{3, 3}}}));
}

/// @brief A digit read a moment ago still names the same food after other food is eaten
TEST(OrbitalFoodLabels, KeepsLabelOfRemainingFood) {
  // When: the food labelled 0 is eaten
  FoodLabels labels =
      orbital_food_labels::relabel({{0, Point{1, 1}}, {1, Point{2, 2}}, {2, Point{3, 3}}}, {Point{2, 2}, Point{3, 3}});

  // Then: the remaining food keeps its labels
  EXPECT_EQ(labels, (FoodLabels{{1, Point{2, 2}}, {2, Point{3, 3}}}));
}

TEST(OrbitalFoodLabels, GivesLowestFreeLabelToNewFood) {
  // When: new food appears while label 0 is free
  FoodLabels labels =
      orbital_food_labels::relabel({{1, Point{2, 2}}, {2, Point{3, 3}}}, {Point{2, 2}, Point{3, 3}, Point{7, 7}});

  // Then: the new food takes label 0
  EXPECT_EQ(labels, (FoodLabels{{0, Point{7, 7}}, {1, Point{2, 2}}, {2, Point{3, 3}}}));
}

/// @brief There are only ten digits
TEST(OrbitalFoodLabels, LeavesFoodBeyondTenUnlabelled) {
  // When: eleven food items are labelled
  FoodLabels labels = orbital_food_labels::relabel({},
                                                   {Point{0, 0},
                                                    Point{1, 0},
                                                    Point{2, 0},
                                                    Point{3, 0},
                                                    Point{4, 0},
                                                    Point{5, 0},
                                                    Point{6, 0},
                                                    Point{7, 0},
                                                    Point{8, 0},
                                                    Point{9, 0},
                                                    Point{10, 0}});

  // Then: the first ten are labelled 0 to 9, the eleventh is not
  EXPECT_EQ(labels.size(), 10U);
  EXPECT_EQ(labels.at(9), (Point{9, 0}));
}

/// @section orbital_food_labels::trySubmitFood

TEST(OrbitalFoodLabels, SubmitsPositionOfLabelledFoodForPlayerA) {
  // When: the digit of a labelled food is pressed
  std::optional<Submit> submit = orbital_food_labels::trySubmitFood({{3, Point{7, 4}}}, Key{'3'});

  // Then: A submits the position of that food
  ASSERT_TRUE(submit.has_value());
  EXPECT_EQ(submit->player, PLAYER_A);
  EXPECT_EQ(submit->target, (Point{7, 4}));
}

TEST(OrbitalFoodLabels, IgnoresDigitWithoutFood) {
  // When: a digit without food is pressed
  std::optional<Submit> submit = orbital_food_labels::trySubmitFood({{3, Point{7, 4}}}, Key{'4'});

  // Then: nothing is submitted
  EXPECT_FALSE(submit.has_value());
}

TEST(OrbitalFoodLabels, IgnoresKeyOtherThanDigit) {
  // When: a letter is pressed
  std::optional<Submit> submit = orbital_food_labels::trySubmitFood({{3, Point{7, 4}}}, Key{'e'});

  // Then: nothing is submitted
  EXPECT_FALSE(submit.has_value());
}

}  // namespace snake
