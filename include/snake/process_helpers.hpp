#pragma once

#include <memory>
#include <tuple>
#include <vector>

#include "actor-core/effect_handler.hpp"
#include "actor-core/input_source.hpp"
#include "actor-core/subscription.hpp"
#include "funkypipes/details/tuple/separate_tuple_elements.hpp"

namespace snake {

// Import actor_core types into snake namespace for convenience
using actor_core::InputSource;
using actor_core::Subscription;

/**
 * @brief Higher-order function to process all messages from a subscription
 *
 * Drains all pending messages from subscription and applies handler to each.
 *
 * @tparam TMsg Message type
 * @tparam THandlerFn Function type (TMsg -> void)
 * @param sub Subscription to drain
 * @param handler Function to call with each message
 */
template <typename TMsg, typename THandlerFn>
void processMessage(const std::shared_ptr<Subscription<TMsg>>& sub, THandlerFn&& handler) {
  while (auto msg = sub->tryTakeMessage()) {
    handler(*msg);
  }
}

/**
 * @brief Higher-order function to process all messages with state transformation
 *
 * Drains all pending messages from subscription and applies handler to each,
 * passing state and message, then assigns the result back to state.
 *
 * @tparam TMsg Message type
 * @tparam TState State type
 * @tparam THandlerFn Function type (TState, TMsg) -> TState
 * @param sub Subscription to drain
 * @param state State reference to pass and update
 * @param handler Function to call with state and each message
 */
template <typename TMsg, typename TState, typename THandlerFn>
void processMessageWithState(const std::shared_ptr<Subscription<TMsg>>& sub, TState& state, THandlerFn&& handler) {
  while (auto msg = sub->tryTakeMessage()) {
    state = handler(state, *msg);
  }
}

/**
 * @brief Higher-order function to process all timer events
 *
 * Drains all pending events from timer and applies handler to each.
 *
 * @tparam TTimer Timer type
 * @tparam THandlerFn Function type (Event -> void)
 * @param timer Timer to take events from
 * @param handler Function to call with each event
 */
template <typename TTimer, typename THandlerFn>
void processEvent(const std::shared_ptr<TTimer>& timer, THandlerFn&& handler) {
  while (auto event = timer->tryTakeElapsedEvent()) {
    handler(*event);
  }
}

/**
 * @brief Decorator that wraps a pure function with effect handling logic
 *
 * Takes a pure function that returns tuple<TState, Effects...> and wraps it
 * into a state transformer (TState, Arg) -> TState that automatically handles effects.
 *
 * @tparam TState State type
 * @tparam TEffectHandler Effect handler type with handle() overloads
 * @tparam TProcessFn Function type (TState, Arg) -> tuple<TState, Effects...>
 * @param process_fn Pure function returning state and effects
 * @param effect_handler Interpreter for effects
 * @return State transformer function with automatic effect handling
 */
template <typename TState, typename TEffectHandler, typename TProcessFn>
auto with_effect_handling(TProcessFn&& process_fn, TEffectHandler& effect_handler) {
  return [&process_fn, &effect_handler](TState state_arg, const auto& arg) -> TState {
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
 * Drains all pending events from timer and applies process_fn to each,
 * passing state and event, then assigns the result back to state.
 *
 * @tparam TTimer Timer type
 * @tparam TState State type
 * @tparam TProcessFn Function type (TState, Event) -> TState
 * @param timer Timer to take events from
 * @param state State reference to pass and update
 * @param process_fn Function to call with state and each event
 */
template <typename TTimer, typename TState, typename TProcessFn>
void processEventWithState(const std::shared_ptr<TTimer>& timer, TState& state, TProcessFn&& process_fn) {
  while (auto event = timer->tryTakeElapsedEvent()) {
    state = process_fn(state, *event);
  }
}

/**
 * @brief Process timer events with effect handler pattern
 *
 * Drains all pending events from timer and applies process_fn to each.
 * Process function returns tuple<TState, Effects...> where:
 * - First element (TState) updates the state
 * - Remaining elements (Effects) are dispatched through effect_handler
 *
 * This separates pure business logic (process_fn) from effect interpretation (effect_handler).
 *
 * @tparam TTimer Timer type
 * @tparam TState State type
 * @tparam TEffectHandler Effect handler type with handle() overloads
 * @tparam TProcessFn Function type (TState, Event) -> tuple<TState, Effects...>
 * @param timer Timer to take events from
 * @param state State reference to pass and update
 * @param process_fn Pure function returning state and effects
 * @param effect_handler Interpreter for effects (publishes messages, etc.)
 */
template <typename TTimer, typename TState, typename TEffectHandler, typename TProcessFn>
void processEventWithState(const std::shared_ptr<TTimer>& timer, TState& state, TProcessFn&& process_fn,
                           TEffectHandler& effect_handler) {
  auto process_with_effect_handling =
      with_effect_handling<TState>(std::forward<TProcessFn>(process_fn), effect_handler);
  processEventWithState(timer, state, process_with_effect_handling);
}

/**
 * @brief Process messages with effect handler pattern
 *
 * Drains all pending messages from subscription and applies process_fn to each.
 * Process function returns tuple<TState, Effects...> where:
 * - First element (TState) updates the state
 * - Remaining elements (Effects) are dispatched through effect_handler
 *
 * @tparam Msg Message type
 * @tparam TState State type
 * @tparam TEffectHandler Effect handler type with handle() overloads
 * @tparam TProcessFn Function type (TState, TMsg) -> tuple<TState, Effects...>
 * @param sub Subscription to drain
 * @param state State reference to pass and update
 * @param process_fn Pure function returning state and effects
 * @param effect_handler Interpreter for effects
 */
template <typename TMsg, typename TState, typename TEffectHandler, typename TProcessFn>
void processMessageWithState(const std::shared_ptr<Subscription<TMsg>>& sub, TState& state, TProcessFn&& process_fn,
                             TEffectHandler& effect_handler) {
  auto process_with_effect_handling =
      with_effect_handling<TState>(std::forward<TProcessFn>(process_fn), effect_handler);
  processMessageWithState(sub, state, process_with_effect_handling);
}

/**
 * @brief Higher-order function to apply state transformation
 *
 * Applies a state transformer (e.g., lens) to state and assigns result back.
 * Simplifies the pattern: state = transformer(state)
 *
 * @tparam TState State type
 * @tparam TTransformerFn Function type (TState) -> TState
 * @param state State reference to transform and update
 * @param transformer Function to apply to state
 */
template <typename TState, typename TTransformerFn>
void applyToState(TState& state, TTransformerFn&& transformer) {
  state = transformer(state);
}

// ============================================================================
// Unified InputSource<T> Processing Functions
// ============================================================================
// These generic functions work with ANY InputSource<T> (Subscription, Timer, StdinReader, etc.)

/**
 * @brief Generic function to process all items from any input source
 *
 * Works with ANY type that implements InputSource<T> interface:
 * - Subscription<TMessage> → processes messages
 * - Timer<TEvent, TCommand> → processes timer events
 * - StdinReader → processes characters
 * - Custom input sources
 *
 * @tparam T Item type from the source
 * @tparam THandlerFn Function type (T -> void)
 * @param source Any InputSource<T> implementation
 * @param handler Function to call with each item
 */
template <typename T, typename HandlerFn>
void process(const std::shared_ptr<InputSource<T>>& source, HandlerFn&& handler) {
  while (auto item = source->tryTake()) {
    handler(*item);
  }
}

/**
 * @brief Generic function to process items with state transformation
 *
 * Works with ANY input source. Drains all pending items and applies handler to each,
 * passing state and item, then assigns the result back to state.
 *
 * @tparam T Item type from the source
 * @tparam TState State type
 * @tparam TProcessFn Function type (TState, T) -> TState
 * @param source Any InputSource<T> implementation
 * @param state State reference to pass and update
 * @param process_fn Function to call with state and each item
 */
template <typename T, typename TState, typename TProcessFn>
void processWithState(const std::shared_ptr<InputSource<T>>& source, TState& state, TProcessFn&& process_fn) {
  while (auto item = source->tryTake()) {
    state = process_fn(state, *item);
  }
}

/**
 * @brief Generic function to process items with effect handler pattern
 *
 * Works with ANY input source. Drains all pending items and applies process_fn to each.
 * Process function returns tuple<TState, Effects...> where:
 * - First element (TState) updates the state
 * - Remaining elements (Effects) are dispatched through effect_handler
 *
 * This separates pure business logic (process_fn) from effect interpretation (effect_handler).
 *
 * @tparam T Item type from the source
 * @tparam TState State type
 * @tparam TEffectHandler Effect handler type with handle() overloads
 * @tparam TProcessFn Function type (TState, T) -> tuple<TState, Effects...>
 * @param source Any InputSource<T> implementation
 * @param state State reference to pass and update
 * @param process_fn Pure function returning state and effects
 * @param effect_handler Interpreter for effects
 */
template <typename T, typename TState, typename TEffectHandler, typename TProcessFn>
void processWithState(const std::shared_ptr<InputSource<T>>& source, TState& state, TProcessFn&& process_fn,
                      TEffectHandler& effect_handler) {
  auto process_with_effect_handling =
      with_effect_handling<TState>(std::forward<TProcessFn>(process_fn), effect_handler);
  processWithState(source, state, process_with_effect_handling);
}

}  // namespace snake
