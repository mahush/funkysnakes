#pragma once

#include <memory>
#include <vector>

#include "snake/topic_subscription.hpp"

namespace snake {

/**
 * @brief Higher-order function to process all messages from a subscription
 *
 * Drains all pending messages from subscription and applies handler to each.
 *
 * @tparam Msg Message type
 * @tparam Handler Function type (Msg -> void)
 * @param sub Subscription to drain
 * @param handler Function to call with each message
 */
template <typename Msg, typename Handler>
void process_message(const std::shared_ptr<Subscription<Msg>>& sub, Handler&& handler) {
  while (auto msg = sub->tryReceive()) {
    handler(*msg);
  }
}

/**
 * @brief Higher-order function to process all messages with state transformation
 *
 * Drains all pending messages from subscription and applies handler to each,
 * passing state and message, then assigns the result back to state.
 *
 * @tparam Msg Message type
 * @tparam State State type
 * @tparam Handler Function type (State, Msg) -> State
 * @param sub Subscription to drain
 * @param state State reference to pass and update
 * @param handler Function to call with state and each message
 */
template <typename Msg, typename State, typename Handler>
void process_message_with_state(const std::shared_ptr<Subscription<Msg>>& sub, State& state, Handler&& handler) {
  while (auto msg = sub->tryReceive()) {
    state = handler(state, *msg);
  }
}

/**
 * @brief Higher-order function to process all timer events
 *
 * Takes all elapsed events from timer and applies handler to each.
 *
 * @tparam Timer Timer type
 * @tparam Handler Function type (Event -> void)
 * @param timer Timer to take events from
 * @param handler Function to call with each event
 */
template <typename Timer, typename Handler>
void process_event(const std::shared_ptr<Timer>& timer, Handler&& handler) {
  auto timer_events = timer->take_all_elapsed_events();
  for (const auto& event : timer_events) {
    handler(event);
  }
}

}  // namespace snake
