#include <gtest/gtest.h>

#include "classic/classic_arena_authority.hpp"
#include "common/snake_model_evolve.hpp"
#include "funkypipes/bind_front.hpp"
#include "snake/generic_lens.hpp"
#include "snake/generic_view.hpp"

namespace snake {

// Lens machinery is exercised on the arena state as a representative state type
using ArenaState = classic_arena_authority::State;

// Test: Single mutable field
TEST(GenericLensTest, SingleMutableField) {
  auto op = [](PerPlayerSnakes snakes) {
    snakes["PLAYER_A"] = snake_model::initial(Point{5, 5}, Direction::UP, 1);
    return snakes;
  };

  auto transform = lens(mutate<&ArenaState::snakes>, read<>, op);

  ArenaState state;
  state = transform(state);

  EXPECT_EQ(state.snakes.size(), 1u);
  EXPECT_EQ(snake_model::head(state.snakes["PLAYER_A"]).x, 5);
  EXPECT_EQ(snake_model::head(state.snakes["PLAYER_A"]).y, 5);
}

// Test: Two mutable fields
TEST(GenericLensTest, TwoMutableFields) {
  auto op = [](PerPlayerSnakes snakes, std::map<PlayerId, int> scores) {
    snakes["PLAYER_A"] = snake_model::initial(Point{5, 5}, Direction::UP, 1);
    scores["PLAYER_A"] = 100;
    return std::make_tuple(snakes, scores);
  };

  auto transform = lens(mutate<&ArenaState::snakes, &ArenaState::scores>, read<>, op);

  ArenaState state;
  state = transform(state);

  EXPECT_EQ(state.snakes.size(), 1u);
  EXPECT_EQ(state.scores["PLAYER_A"], 100);
}

// Test: Mutable + readonly access
TEST(GenericLensTest, MutableWithReadonly) {
  auto op = [](PerPlayerSnakes snakes, const Board& board, const std::vector<Point>& food) {
    // Verify readonly access works
    EXPECT_EQ(board.width, 60);
    EXPECT_EQ(board.height, 20);
    EXPECT_EQ(food.size(), 2u);

    snakes["PLAYER_A"] = snake_model::initial(Point{10, 10}, Direction::RIGHT, 1);
    return snakes;
  };

  auto transform = lens(mutate<&ArenaState::snakes>, read<&ArenaState::board, &ArenaState::food_items>, op);

  ArenaState state;
  state.board = {60, 20};
  state.food_items = {{5, 5}, {10, 10}};

  state = transform(state);

  EXPECT_EQ(snake_model::head(state.snakes["PLAYER_A"]).x, 10);
}

// Test: Additional output (threaded to next stage)
TEST(GenericLensTest, AdditionalOutput) {
  auto op = [](PerPlayerSnakes snakes, std::map<PlayerId, int> scores) {
    scores["PLAYER_A"] = 50;
    std::vector<Point> cut_tails = {{1, 1}, {2, 2}};
    return std::make_tuple(snakes, scores, cut_tails);
  };

  auto transform = lens(mutate<&ArenaState::snakes, &ArenaState::scores>, read<>, op);

  ArenaState state;
  auto [new_state, cut_tails] = transform(state);

  EXPECT_EQ(new_state.scores["PLAYER_A"], 50);
  EXPECT_EQ(cut_tails.size(), 2u);
  EXPECT_EQ(cut_tails[0].x, 1);
  EXPECT_EQ(cut_tails[0].y, 1);
}

// Test: Pipeline argument forwarding
TEST(GenericLensTest, PipelineArgumentForwarding) {
  auto op = [](std::vector<Point> food, const std::vector<Point>& new_food) {
    food.insert(food.end(), new_food.begin(), new_food.end());
    return food;
  };

  auto transform = lens(mutate<&ArenaState::food_items>, read<>, op);

  ArenaState state;
  state.food_items = {{5, 5}};

  std::vector<Point> pipeline_arg = {{10, 10}, {15, 15}};
  state = transform(state, pipeline_arg);

  EXPECT_EQ(state.food_items.size(), 3u);
  EXPECT_EQ(state.food_items[1].x, 10);
  EXPECT_EQ(state.food_items[2].x, 15);
}

// Test: Complex case - all features combined
TEST(GenericLensTest, ComplexTransformation) {
  auto op = [](PerPlayerSnakes snakes, std::map<PlayerId, int> scores, const Board& board, int bonus) {
    // Use readonly board
    EXPECT_EQ(board.width, 80);

    // Use pipeline argument
    scores["PLAYER_A"] = bonus;

    // Generate additional output
    std::vector<Point> result_data = {{board.width / 2, board.height / 2}};

    return std::make_tuple(snakes, scores, result_data);
  };

  auto transform = lens(mutate<&ArenaState::snakes, &ArenaState::scores>, read<&ArenaState::board>, op);

  ArenaState state;
  state.board = {80, 30};

  int bonus_points = 200;
  auto [new_state, result_data] = transform(state, bonus_points);

  EXPECT_EQ(new_state.scores["PLAYER_A"], 200);
  EXPECT_EQ(result_data.size(), 1u);
  EXPECT_EQ(result_data[0].x, 40);
  EXPECT_EQ(result_data[0].y, 15);
}

// Test: bindFront with generic lens
TEST(GenericLensTest, WithBindFront) {
  // Simulate a function like updateFoodPositions that takes extra param first
  auto updateWithTick = [](int tick, std::vector<Point> food, const Board& board, const PerPlayerSnakes&) {
    // Only update on certain ticks
    if (tick % 3 == 0 && !food.empty()) {
      food[0].x = (food[0].x + 1) % board.width;
    }
    return food;
  };

  // Use bindFront from funkypipes to bind the tick parameter
  int current_tick = 6;
  auto bound_op = funkypipes::bindFront(updateWithTick, current_tick);

  auto transform = lens(mutate<&ArenaState::food_items>, read<&ArenaState::board, &ArenaState::snakes>, bound_op);

  ArenaState state;
  state.board = {20, 20};
  state.food_items = {{5, 5}, {10, 10}};

  state = transform(state);

  // Food should be updated (tick 6 is divisible by 3)
  EXPECT_EQ(state.food_items[0].x, 6);
  EXPECT_EQ(state.food_items[0].y, 5);
  EXPECT_EQ(state.food_items[1].x, 10);
}

// Test: bindFront with mutable lambda
TEST(GenericLensTest, WithBindFrontMutableLambda) {
  int counter = 0;
  auto countingOp = [counter](int multiplier, std::vector<Point> food) mutable {
    counter++;
    if (!food.empty()) {
      food[0].x *= multiplier;
    }
    return food;
  };

  auto bound_op = funkypipes::bindFront(countingOp, 2);

  auto transform = lens(mutate<&ArenaState::food_items>, read<>, bound_op);

  ArenaState state;
  state.food_items = {{5, 5}};

  state = transform(state);

  EXPECT_EQ(state.food_items[0].x, 10);
  EXPECT_EQ(state.food_items[0].y, 5);
}

// ============================================================================
// Generic View Tests
// ============================================================================

TEST(GenericViewTest, SingleReadonlyField) {
  auto get_board_width = view(read<&ArenaState::board>, [](const Board& board) { return board.width; });

  ArenaState state;
  state.board.width = 42;

  int width = get_board_width(state);

  EXPECT_EQ(width, 42);
}

TEST(GenericViewTest, MultipleReadonlyFields) {
  auto count_cells_and_food =
      view(read<&ArenaState::board, &ArenaState::food_items>, [](const Board& board, const FoodItems& food) {
        return board.width * board.height + static_cast<int>(food.size());
      });

  ArenaState state;
  state.board.width = 10;
  state.board.height = 5;
  state.food_items = {{1, 1}, {2, 2}, {3, 3}};

  int total = count_cells_and_food(state);

  EXPECT_EQ(total, 53);  // 10 * 5 + 3
}

TEST(GenericViewTest, WithAdditionalArguments) {
  auto multiply_width =
      view(read<&ArenaState::board>, [](const Board& board, int multiplier) { return board.width * multiplier; });

  ArenaState state;
  state.board.width = 20;

  int result = multiply_width(state, 3);

  EXPECT_EQ(result, 60);
}

TEST(GenericViewTest, ReturningComplexType) {
  auto extract_board_copy =
      view(read<&ArenaState::board>, [](const Board& board) { return Board{board.width, board.height}; });

  ArenaState state;
  state.board.width = 30;
  state.board.height = 15;

  Board result = extract_board_copy(state);

  EXPECT_EQ(result.width, 30);
  EXPECT_EQ(result.height, 15);
}

}  // namespace snake
