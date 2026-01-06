#ifndef SNAKE_GENERIC_VIEW_HPP
#define SNAKE_GENERIC_VIEW_HPP

#include "snake/generic_lens.hpp"

namespace snake {

// ============================================================================
// Generic View - Read-only field extractor
// ============================================================================
// Views are simpler than lenses - they extract readonly fields from state,
// apply an operation, and return the result directly (no state mutation).
//
// Built on the same tag infrastructure as lenses (read_t, detail helpers)
// but focused solely on read-only access.

/**
 * @brief Generic view for read-only field extraction
 *
 * Creates a function that extracts readonly fields from state and passes
 * them to an operation along with additional arguments. Returns the operation
 * result directly without wrapping in state.
 *
 * Views are simpler than lenses:
 * - No mutation or write-back
 * - Return operation result directly
 * - Take state directly (not returned)
 *
 * @tparam ReadMembers Member pointers to fields to extract
 * @tparam Op Operation type
 *
 * Operation signature:
 *   (readonly_fields..., additional_args...) -> Result
 *
 * Example usage:
 *   // Extract board and snakes for read-only operation
 *   auto extractor = view(read<&GameState::board, &GameState::snakes>,
 *                         generateRandomFoodPosition);
 *   Point pos = extractor(state, random_int);
 *
 * @param read_tag Tag specifying readonly fields
 * @param op Operation to apply
 * @return Function that extracts fields and applies operation
 */
template <auto... ReadMembers, typename Op>
auto view(read_t<ReadMembers...>, Op op) {
  // Deduce State type from first readonly member pointer
  using FirstMemberPtr = decltype(detail::get_member<0>::template value<ReadMembers...>());
  using State = typename detail::member_ptr_to_class<FirstMemberPtr>::type;

  return [op = std::move(op)](const State& state, auto&&... args) {
    return op(std::as_const(state.*ReadMembers)..., std::forward<decltype(args)>(args)...);
  };
}

}  // namespace snake

#endif  // SNAKE_GENERIC_VIEW_HPP
