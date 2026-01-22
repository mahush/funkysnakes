#ifndef SNAKE_GENERIC_LENS_HPP
#define SNAKE_GENERIC_LENS_HPP

#include <functional>
#include <tuple>
#include <utility>

namespace snake {

// ============================================================================
// Generic Lens Library - Configurable field access patterns
// ============================================================================
// Enables declarative composition of state transformations with:
// - Mutable fields (extracted, passed by value, returned, written back)
// - Readonly fields (passed by const reference)
// - Pipeline arguments (forwarded through)
// - Additional outputs (threaded to next stage)
//
// C++17 compliant implementation

// Tag types for configuration
template <auto... Members>
struct mutate_t {};

template <auto... Members>
struct read_t {};

// Helper aliases for clean syntax
template <auto... Members>
inline constexpr mutate_t<Members...> mutate{};

template <auto... Members>
inline constexpr read_t<Members...> read{};

// ============================================================================
// Implementation Details
// ============================================================================

namespace detail {

// Helper: Extract class type from member pointer type
template <typename T>
struct member_ptr_to_class;

template <typename Class, typename Member>
struct member_ptr_to_class<Member Class::*> {
  using type = Class;
};

// Helper: Get member pointer at index I from parameter pack
template <std::size_t I>
struct get_member {
  template <auto First, auto... Rest>
  static constexpr auto value() {
    if constexpr (I == 0) {
      return First;
    } else {
      return get_member<I - 1>::template value<Rest...>();
    }
  }
};

// Helper: Write tuple elements back to state members
template <auto... Members>
struct write_back_helper {
  template <typename State, typename Tuple, std::size_t... Is>
  static void apply(State& state, Tuple&& result, std::index_sequence<Is...>) {
    ((state.*(get_member<Is>::template value<Members...>()) = std::move(std::get<Is>(result))), ...);
  }
};

// Helper: Extract additional outputs (beyond mutated fields)
template <std::size_t Offset, std::size_t... Is, typename Tuple>
auto extract_additional(Tuple&& result, std::index_sequence<Is...>) {
  return std::make_tuple(std::move(std::get<Offset + Is>(result))...);
}

// Helper: Check if a type is a tuple
template <typename T>
struct is_tuple : std::false_type {};

template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
inline constexpr bool is_tuple_v = is_tuple<std::decay_t<T>>::value;

// Tag types for result dispatching (workaround for clang 16 bug)
struct non_tuple_tag {};
struct tuple_tag {};

template <typename T>
using result_category = std::conditional_t<is_tuple_v<T>, tuple_tag, non_tuple_tag>;

// Process non-tuple result: single value must match single mutable field
template <auto... MutableMembers, typename State, typename Result>
State processLensResult(State&& state, Result&& result, non_tuple_tag) {
  constexpr std::size_t num_mutable = sizeof...(MutableMembers);
  static_assert(num_mutable == 1, "Single return requires exactly one mutable field");
  state.*(get_member<0>::template value<MutableMembers...>()) = std::move(result);
  return state;
}

// Process tuple result: write back mutable fields, handle additional outputs
template <auto... MutableMembers, typename State, typename Result>
decltype(auto) processLensResult(State&& state, Result&& result, tuple_tag) {
  constexpr std::size_t num_mutable = sizeof...(MutableMembers);
  constexpr std::size_t result_size = std::tuple_size_v<std::decay_t<Result>>;

  static_assert(result_size >= num_mutable,
                "Operation must return at least the mutated fields");

  // Write back mutated fields
  write_back_helper<MutableMembers...>::apply(
      state, result, std::make_index_sequence<num_mutable>{});

  if constexpr (result_size == num_mutable) {
    // Simple case: only mutated fields returned
    return state;
  } else {
    // Extended case: mutated fields + additional outputs
    auto additional = extract_additional<num_mutable>(
        std::move(result), std::make_index_sequence<result_size - num_mutable>{});

    if constexpr (result_size == num_mutable + 1) {
      // Single additional output - unwrap from tuple
      return std::make_tuple(std::move(state), std::move(std::get<0>(additional)));
    } else {
      // Multiple additional outputs - keep as tuple
      return std::tuple_cat(std::make_tuple(std::move(state)), std::move(additional));
    }
  }
}

}  // namespace detail

// ============================================================================
// Generic Lens - Main Implementation
// ============================================================================

/**
 * @brief Generic lens for flexible state transformations
 *
 * Creates a state transformer that:
 * 1. Extracts specified mutable fields from state
 * 2. Passes them with readonly fields and pipeline args to operation
 * 3. Writes back the mutated fields
 * 4. Threads through any additional outputs to the next stage
 *
 * @tparam MutableMembers Member pointers to fields that will be mutated
 * @tparam ReadMembers Member pointers to fields that are read-only
 * @tparam Op Operation type
 *
 * Operation signature:
 *   (mutable_fields..., readonly_fields..., pipeline_args...)
 *     -> (new_mutable_fields...) or (new_mutable_fields..., additional_outputs...)
 *
 * Example usage:
 *   // Updates snakes & scores, reads board, threads cut_tails output
 *   auto op = lens(mutate<&GameState::snakes, &GameState::scores>,
 *                  read<>,
 *                  handleCollisions);
 *
 *   // Updates snakes, reads board & food
 *   auto op = lens(mutate<&GameState::snakes>,
 *                  read<&GameState::board, &GameState::food_items>,
 *                  moveSnakes);
 *
 * @param mutate_tag Tag specifying mutable fields
 * @param read_tag Tag specifying readonly fields
 * @param op Operation to apply
 * @return State transformer compatible with funkypipes
 */
template <auto... MutableMembers, auto... ReadMembers, typename Op>
auto lens(mutate_t<MutableMembers...>, read_t<ReadMembers...>, Op op) {
  // Helper struct to deduce State type from first member pointer
  using FirstMemberPtr = decltype(detail::get_member<0>::template value<MutableMembers...>());
  using State = typename detail::member_ptr_to_class<FirstMemberPtr>::type;

  return [op = std::move(op)](State state, auto&&... pipeline_args) mutable {
    // Extract mutable fields and call operation
    auto result = std::invoke(op,                                                     // Function/functor
                              std::move(state.*MutableMembers)...,                    // Mutable fields
                              std::as_const(state.*ReadMembers)...,                   // Readonly fields
                              std::forward<decltype(pipeline_args)>(pipeline_args)...);  // Pipeline args

    // Use tag dispatch to handle tuple vs non-tuple results
    // (workaround for clang 16 bug with if constexpr on decltype(result))
    using ResultType = decltype(result);
    return detail::processLensResult<MutableMembers...>(
        std::move(state), std::move(result), detail::result_category<ResultType>{});
  };
}

}  // namespace snake

#endif  // SNAKE_GENERIC_LENS_HPP
