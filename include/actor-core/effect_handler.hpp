#pragma once

#include <tuple>
#include <utility>

namespace actor_core {

/**
 * @brief Dispatches effects from a tuple to an effect handler
 *
 * Applies handler.handle(element) for each element in the tuple.
 * The handler must provide overloads for all tuple element types.
 * Compile-time error if handler lacks an overload for any effect type.
 *
 * This pattern models effects in a functional-programming sense:
 * - Tuple of values → batch of pure results or operations
 * - EffectHandler → concrete interpreter of what those values mean
 * - dispatch_effect() → executes the "effect" by sending each value to handler
 *
 * @tparam EffectHandler Type with handle() overloads for each effect type
 * @tparam Tuple Tuple type (forwarding reference for flexibility)
 * @param handler Effect handler instance
 * @param effects Tuple of effects to dispatch
 */
template <typename EffectHandler, typename Tuple>
void dispatch_effect(EffectHandler& handler, Tuple&& effects) {
  std::apply(
      [&](auto&&... elems) {
        // C++17 fold expression: expands to handler.handle(elem1), handler.handle(elem2), ...
        (handler.handle(std::forward<decltype(elems)>(elems)), ...);
      },
      std::forward<Tuple>(effects));
}

}  // namespace actor_core
