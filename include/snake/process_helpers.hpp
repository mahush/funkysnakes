#pragma once

#include <memory>
#include <vector>

#include "actor-core/topic_subscription.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::Subscription;

/**
 * @brief Higher-order function to process all messages from a subscription
 *
 * Drains all pending messages from subscription and applies handler to each.
 *
 * @tparam Msg Message type
 * @tparam HandlerFn Function type (Msg -> void)
 * @param sub Subscription to drain
 * @param handler Function to call with each message
 */
template <typename Msg, typename HandlerFn>
void process_message(const std::shared_ptr<Subscription<Msg>>& sub, HandlerFn&& handler) {
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
 * @tparam HandlerFn Function type (State, Msg) -> State
 * @param sub Subscription to drain
 * @param state State reference to pass and update
 * @param handler Function to call with state and each message
 */
template <typename Msg, typename State, typename HandlerFn>
void process_message_with_state(const std::shared_ptr<Subscription<Msg>>& sub, State& state, HandlerFn&& handler) {
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
 * @tparam HandlerFn Function type (Event -> void)
 * @param timer Timer to take events from
 * @param handler Function to call with each event
 */
template <typename Timer, typename HandlerFn>
void process_event(const std::shared_ptr<Timer>& timer, HandlerFn&& handler) {
  auto timer_events = timer->take_all_elapsed_events();
  for (const auto& event : timer_events) {
    handler(event);
  }
}

/**
 * @brief Higher-order function to process all timer events with state transformation
 *
 * Takes all elapsed events from timer and applies handler to each,
 * passing state and event, then assigns the result back to state.
 *
 * @tparam Timer Timer type
 * @tparam State State type
 * @tparam HandlerFn Function type (State, Event) -> State
 * @param timer Timer to take events from
 * @param state State reference to pass and update
 * @param handler Function to call with state and each event
 */
template <typename Timer, typename State, typename HandlerFn>
void process_event_with_state(const std::shared_ptr<Timer>& timer, State& state, HandlerFn&& handler) {
  auto timer_events = timer->take_all_elapsed_events();
  for (const auto& event : timer_events) {
    state = handler(state, event);
  }
}

/**
 * @brief Higher-order function to apply state transformation
 *
 * Applies a state transformer (e.g., lens) to state and assigns result back.
 * Simplifies the pattern: state = transformer(state)
 *
 * @tparam State State type
 * @tparam TransformerFn Function type (State) -> State
 * @param state State reference to transform and update
 * @param transformer Function to apply to state
 */
template <typename State, typename TransformerFn>
void apply_to_state(State& state, TransformerFn&& transformer) {
  state = transformer(state);
}

}  // namespace snake
