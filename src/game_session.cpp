#include "snake/game_session.hpp"

#include <iostream>

namespace snake {

GameSession::GameSession(asio::io_context& io, TopicPtr<Tick> tick_topic, TopicPtr<DirectionChange> direction_topic,
                         TopicPtr<StateUpdate> state_topic)
    : Actor(io),
      state_pub_(create_pub(state_topic)),
      tick_sub_(create_sub(tick_topic)),
      direction_sub_(create_sub(direction_topic)) {
  state_.game_id = "game_001";
  state_.board_width = 60;
  state_.board_height = 20;

  // Initialize snakes for player1 and player2
  initializeSnake("player1");
  initializeSnake("player2");
}

void GameSession::processMessages() {
  // Process ticks
  while (auto tick = tick_sub_->tryReceive()) {
    onTick(*tick);
  }

  // Process direction changes
  while (auto dir = direction_sub_->tryReceive()) {
    onDirectionChange(*dir);
  }
}

void GameSession::onTick(const Tick&) {
  // Move all snakes
  for (auto& snake : state_.snakes) {
    if (snake.alive) {
      moveSnake(snake);
    }
  }

  // Send state update
  StateUpdate update;
  update.state = state_;
  state_pub_->publish(update);
}

void GameSession::onDirectionChange(const DirectionChange& msg) {
  // Find the snake for this player
  for (auto& snake : state_.snakes) {
    if (snake.player_id == msg.player_id) {
      // Prevent 180-degree turns (going back into yourself)
      Direction new_dir = msg.new_direction;
      Direction current = snake.current_direction;

      bool is_opposite = (current == Direction::UP && new_dir == Direction::DOWN) ||
                         (current == Direction::DOWN && new_dir == Direction::UP) ||
                         (current == Direction::LEFT && new_dir == Direction::RIGHT) ||
                         (current == Direction::RIGHT && new_dir == Direction::LEFT);

      if (!is_opposite) {
        snake.current_direction = msg.new_direction;
      }
      break;
    }
  }
}

void GameSession::initializeSnake(const PlayerId& player_id) {
  SnakeState snake;
  snake.player_id = player_id;
  snake.alive = true;
  snake.score = 0;
  snake.current_direction = Direction::RIGHT;

  // Position snakes at different y-levels
  int y_pos = (player_id == "player1") ? 10 : 15;
  int start_x = 5;

  // Create a snake of length 5, moving right
  // Body is stored with head at index 0
  for (int i = 0; i < 7; ++i) {
    snake.body.push_back({start_x - i, y_pos});
  }

  state_.snakes.push_back(snake);
  std::cout << "[GameSession] Initialized snake for '" << player_id << "' at y=" << y_pos << "\n";
}

void GameSession::moveSnake(SnakeState& snake) {
  if (snake.body.empty()) {
    return;
  }

  // Get next head position
  Position new_head = getNextHeadPosition(snake);

  // Wrap around board edges (horizontally)
  if (new_head.x < 0) {
    new_head.x = state_.board_width - 1;
  } else if (new_head.x >= state_.board_width) {
    new_head.x = 0;
  }

  // Wrap around board edges (vertically)
  if (new_head.y < 0) {
    new_head.y = state_.board_height - 1;
  } else if (new_head.y >= state_.board_height) {
    new_head.y = 0;
  }

  // Add new head at front
  snake.body.insert(snake.body.begin(), new_head);

  // Remove tail to keep fixed length
  snake.body.pop_back();
}

Position GameSession::getNextHeadPosition(const SnakeState& snake) const {
  if (snake.body.empty()) {
    return {0, 0};
  }

  Position head = snake.body[0];
  Position next = head;

  switch (snake.current_direction) {
    case Direction::LEFT:
      next.x = head.x - 1;
      break;
    case Direction::RIGHT:
      next.x = head.x + 1;
      break;
    case Direction::UP:
      next.y = head.y - 1;
      break;
    case Direction::DOWN:
      next.y = head.y + 1;
      break;
  }

  return next;
}

}  // namespace snake
