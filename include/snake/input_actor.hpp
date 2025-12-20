#pragma once

#include <asio.hpp>
#include <map>
#include <memory>

#include "snake/game_messages.hpp"
#include "snake/input_actor_sink.hpp"

namespace snake {

// Forward declaration
class GameSessionSink;

/**
 * @brief Captures and processes user input
 *
 * InputActor receives input events, tags them with player ID,
 * and translates them into DirectionChange commands.
 * Supports multiple players (shared controller/keyboard).
 */
class InputActor : public InputActorSink, public std::enable_shared_from_this<InputActor> {
 public:
  /**
   * @brief Construct a new Input Actor
   * @param io The io_context for async operations
   * @param session The game session that receives direction changes
   * @param game_id The current game ID
   */
  InputActor(asio::io_context& io, std::shared_ptr<GameSessionSink> session, GameId game_id);

  void post(UserInputEvent msg) override;

 private:
  void onUserInputEvent(const UserInputEvent& msg);
  Direction charToDirection(char key) const;

  asio::strand<asio::io_context::executor_type> strand_;
  std::shared_ptr<GameSessionSink> session_;
  GameId game_id_;
};

}  // namespace snake
