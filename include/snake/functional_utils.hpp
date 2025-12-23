#ifndef SNAKE_FUNCTIONAL_UTILS_HPP
#define SNAKE_FUNCTIONAL_UTILS_HPP

#include <functional>
#include <utility>

namespace snake {

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
