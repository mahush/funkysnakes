#pragma once

#include <memory>
#include <tuple>
#include <vector>

#include "actor-core/effect_handler.hpp"
#include "actor-core/topic_subscription.hpp"
#include "funkypipes/details/tuple/separate_tuple_elements.hpp"

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
 * @brief Decorator that wraps a pure function with effect handling logic
 *
 * Takes a pure function that returns tuple<State, Effects...> and wraps it
 * into a state transformer (State, Arg) -> State that automatically handles effects.
 *
 * @tparam State State type
 * @tparam TEffectHandler Effect handler type with handle() overloads
 * @tparam TProcessFn Function type (State, Arg) -> tuple<State, Effects...>
 * @param process_fn Pure function returning state and effects
 * @param effect_handler Interpreter for effects
 * @return State transformer function with automatic effect handling
 */
template <typename State, typename TEffectHandler, typename TProcessFn>
auto with_effect_handling(TProcessFn&& process_fn, TEffectHandler& effect_handler) {
  return [&process_fn, &effect_handler](State state_arg, const auto& arg) -> State {
    // Call pure process function
    auto result = process_fn(state_arg, arg);

    // Separate state (index 0) from effects (remaining indices)
    auto [state_tuple, effects_tuple] = funkypipes::details::separateTupleElements<0>(std::move(result));

    // Dispatch effects through effect handler
    actor_core::dispatch_effect(effect_handler, effects_tuple);

    // Return new state
    return std::move(std::get<0>(state_tuple));
  };
}

/**
 * @brief Higher-order function to process all timer events with state transformation
 *
 * Takes all elapsed events from timer and applies process_fn to each,
 * passing state and event, then assigns the result back to state.
 *
 * @tparam Timer Timer type
 * @tparam State State type
 * @tparam TProcessFn Function type (State, Event) -> State
 * @param timer Timer to take events from
 * @param state State reference to pass and update
 * @param process_fn Function to call with state and each event
 */
template <typename Timer, typename State, typename TProcessFn>
void process_event_with_state(const std::shared_ptr<Timer>& timer, State& state, TProcessFn&& process_fn) {
  auto timer_events = timer->take_all_elapsed_events();
  for (const auto& event : timer_events) {
    state = process_fn(state, event);
  }
}

/**
 * @brief Process timer events with effect handler pattern
 *
 * Takes all elapsed events from timer and applies process_fn to each.
 * Process function returns tuple<State, Effects...> where:
 * - First element (State) updates the state
 * - Remaining elements (Effects) are dispatched through effect_handler
 *
 * This separates pure business logic (process_fn) from effect interpretation (effect_handler).
 *
 * @tparam Timer Timer type
 * @tparam State State type
 * @tparam TEffectHandler Effect handler type with handle() overloads
 * @tparam TProcessFn Function type (State, Event) -> tuple<State, Effects...>
 * @param timer Timer to take events from
 * @param state State reference to pass and update
 * @param process_fn Pure function returning state and effects
 * @param effect_handler Interpreter for effects (publishes messages, etc.)
 */
template <typename Timer, typename State, typename TEffectHandler, typename TProcessFn>
void process_event_with_state(const std::shared_ptr<Timer>& timer, State& state, TProcessFn&& process_fn,
                               TEffectHandler& effect_handler) {
  auto process_with_effect_handling = with_effect_handling<State>(std::forward<TProcessFn>(process_fn), effect_handler);
  process_event_with_state(timer, state, process_with_effect_handling);
}

/**
 * @brief Process messages with effect handler pattern
 *
 * Drains all pending messages from subscription and applies process_fn to each.
 * Process function returns tuple<State, Effects...> where:
 * - First element (State) updates the state
 * - Remaining elements (Effects) are dispatched through effect_handler
 *
 * @tparam Msg Message type
 * @tparam State State type
 * @tparam TEffectHandler Effect handler type with handle() overloads
 * @tparam TProcessFn Function type (State, Msg) -> tuple<State, Effects...>
 * @param sub Subscription to drain
 * @param state State reference to pass and update
 * @param process_fn Pure function returning state and effects
 * @param effect_handler Interpreter for effects
 */
template <typename Msg, typename State, typename TEffectHandler, typename TProcessFn>
void process_message_with_state(const std::shared_ptr<Subscription<Msg>>& sub, State& state, TProcessFn&& process_fn,
                                 TEffectHandler& effect_handler) {
  auto process_with_effect_handling = with_effect_handling<State>(std::forward<TProcessFn>(process_fn), effect_handler);
  process_message_with_state(sub, state, process_with_effect_handling);
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
