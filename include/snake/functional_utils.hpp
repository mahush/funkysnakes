#ifndef SNAKE_FUNCTIONAL_UTILS_HPP
#define SNAKE_FUNCTIONAL_UTILS_HPP

#include <functional>
#include <utility>

namespace snake {

/**
 * @brief Conditional function application helper
 *
 * Applies a function only if a predicate is satisfied, otherwise returns the value unchanged.
 * Inspired by ramda's `when` function (see funkypipes roadmap #5).
 *
 * Type signature: (a -> Bool) -> (a -> a) -> a -> a
 *
 * @tparam Predicate Function type: (a) -> bool
 * @tparam Fn Function type: (a) -> a
 * @param pred Predicate to test the value
 * @param fn Function to apply if predicate is true
 * @return Function that conditionally applies fn
 *
 * Example:
 *   auto isEven = [](int x) { return x % 2 == 0; };
 *   auto doubleIt = [](int x) { return x * 2; };
 *   auto doubleIfEven = when(isEven, doubleIt);
 *   doubleIfEven(3);  // returns 3
 *   doubleIfEven(4);  // returns 8
 */
template <typename Predicate, typename Fn>
auto when(Predicate pred, Fn fn) {
  return [pred = std::move(pred), fn = std::move(fn)](auto&& value) {
    if (pred(value)) {
      return fn(std::forward<decltype(value)>(value));
    }
    return std::forward<decltype(value)>(value);
  };
}

/**
 * @brief Binary composition producing f(a, g(a,b))
 *
 * Composes two functions where the second function takes two arguments
 * and its result is passed as the second argument to the first function.
 *
 * @tparam F Function type for f
 * @tparam G Function type for g
 * @param f First function
 * @param g Second function
 * @return Composed function
 */
template <typename F, typename G>
auto compose2(F f, G g) {
  return [=](auto&& a, auto&& b) { return f(a, g(a, b)); };
}

/**
 * @brief Case for conditional composition
 *
 * Represents a predicate-action pair used by the cond combinator.
 *
 * @tparam T Input type
 */
template <typename T>
using Case = std::pair<std::function<bool(const T&)>,  // predicate
                       std::function<T(T)>>;           // transformer

/**
 * @brief Conditional composition combinator (analogous to Ramda's cond)
 *
 * Takes an otherwise/default function and a variadic list of Cases.
 * Returns a function that applies the first matching case's transformer,
 * or the otherwise function if no cases match.
 *
 * @tparam OtherwiseFn Type of default function
 * @tparam Cases Variadic case types
 * @param otherwiseFn Default transformation when no cases match
 * @param cases Variadic list of predicate-action pairs
 * @return Function that applies conditional logic
 */
template <typename OtherwiseFn, typename... Cases>
auto cond(OtherwiseFn&& otherwiseFn, Cases&&... cases) {
  return [=](auto&& x) {
    for (auto&& [predicateFn, action] : {cases...}) {
      if (predicateFn(x)) {
        return action(x);
      }
    }
    return otherwiseFn(x);
  };
}

}  // namespace snake

#endif  // SNAKE_FUNCTIONAL_UTILS_HPP
