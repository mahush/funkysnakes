#ifndef SNAKE_FUNCTIONAL_UTILS_HPP
#define SNAKE_FUNCTIONAL_UTILS_HPP

#include <cstddef>
#include <functional>
#include <tuple>
#include <utility>

namespace snake {

namespace impl {

// Helper to get the Nth argument from a parameter pack
template <std::size_t N>
struct GetArgAt {
  template <typename... Args>
  static decltype(auto) get(Args&&... args) {
    return std::get<N>(std::forward_as_tuple(std::forward<Args>(args)...));
  }
};

}  // namespace impl

/**
 * @brief Conditional function application helper
 *
 * Applies a function only if a predicate is satisfied, otherwise returns the first argument.
 * Inspired by ramda's `when` function (see funkypipes roadmap #5).
 *
 * Can be parameterized by argument index to check a specific argument with the predicate.
 *
 * Type signatures:
 * - when():      (a... -> Bool) -> (a... -> a0) -> a... -> a0
 * - when<N>():   (aN -> Bool) -> (a... -> a0) -> a... -> a0
 *
 * @tparam ArgIndex Optional: which argument to test with predicate (default: all args)
 * @tparam TPredicate Function type: (a) -> bool or (a...) -> bool
 * @tparam TFn Function type: (a...) -> a0
 * @param pred Predicate to test the value(s)
 * @param fn Function to apply if predicate is true
 * @return Function that conditionally applies fn
 *
 * Examples:
 *   // All arguments to predicate:
 *   auto bothEven = [](int x, int y) { return x % 2 == 0 && y % 2 == 0; };
 *   auto sumIfBothEven = when(bothEven, [](int x, int y) { return x + y; });
 *   sumIfBothEven(2, 4);  // returns 6
 *   sumIfBothEven(2, 3);  // returns 2 (first arg)
 *
 *   // Check specific argument:
 *   auto isEven = [](int x) { return x % 2 == 0; };
 *   auto processIfFirstEven = when<0>(isEven, [](int x, int y) { return x + y; });
 *   processIfFirstEven(2, 3);  // returns 5
 *   processIfFirstEven(3, 2);  // returns 3 (first arg)
 */
template <std::size_t ArgIndex = std::size_t(-1), typename TPredicate, typename TFn>
auto when(TPredicate pred, TFn fn) {
  return [pred = std::move(pred), fn = std::move(fn)](auto&&... args) mutable {
    // Get first argument for return value when predicate is false
    auto getFirst = [](auto&& first, auto&&...) -> decltype(auto) { return std::forward<decltype(first)>(first); };

    // Check the predicate
    bool condition;
    if constexpr (ArgIndex == std::size_t(-1)) {
      // No index specified: pass all args to predicate
      condition = pred(std::forward<decltype(args)>(args)...);
    } else {
      // Index specified: pass only that arg to predicate
      condition = pred(impl::GetArgAt<ArgIndex>::get(std::forward<decltype(args)>(args)...));
    }

    if (condition) {
      return fn(std::forward<decltype(args)>(args)...);
    }
    return getFirst(std::forward<decltype(args)>(args)...);
  };
}

/**
 * @brief Binary composition producing f(a, g(a,b))
 *
 * Composes two functions where the second function takes two arguments
 * and its result is passed as the second argument to the first function.
 *
 * @tparam TF Function type for f
 * @tparam TG Function type for g
 * @param f First function
 * @param g Second function
 * @return Composed function
 */
template <typename TF, typename TG>
auto compose2(TF f, TG g) {
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
 * @tparam TOtherwiseFn Type of default function
 * @tparam TCases Variadic case types
 * @param otherwiseFn Default transformation when no cases match
 * @param cases Variadic list of predicate-action pairs
 * @return Function that applies conditional logic
 */
template <typename TOtherwiseFn, typename... TCases>
auto cond(TOtherwiseFn&& otherwiseFn, TCases&&... cases) {
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
