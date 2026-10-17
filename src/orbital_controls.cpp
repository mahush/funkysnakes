#include "snake/orbital_controls.hpp"

#include "common/snake_model.hpp"

namespace snake {
namespace orbital_controls {

namespace {

// Cursor movement of a key: the moving player and direction
std::optional<std::tuple<PlayerId, Direction>> tryCursorMove(const Key& key) {
  if (const auto* special = std::get_if<SpecialKey>(&key)) {
    switch (*special) {
      case SpecialKey::ARROW_UP:
        return std::make_tuple(PlayerId{PLAYER_B}, Direction::UP);
      case SpecialKey::ARROW_DOWN:
        return std::make_tuple(PlayerId{PLAYER_B}, Direction::DOWN);
      case SpecialKey::ARROW_LEFT:
        return std::make_tuple(PlayerId{PLAYER_B}, Direction::LEFT);
      case SpecialKey::ARROW_RIGHT:
        return std::make_tuple(PlayerId{PLAYER_B}, Direction::RIGHT);
    }
    return std::nullopt;
  }
  switch (std::get<char>(key)) {
    case 'w':
      return std::make_tuple(PlayerId{PLAYER_A}, Direction::UP);
    case 's':
      return std::make_tuple(PlayerId{PLAYER_A}, Direction::DOWN);
    case 'a':
      return std::make_tuple(PlayerId{PLAYER_A}, Direction::LEFT);
    case 'd':
      return std::make_tuple(PlayerId{PLAYER_A}, Direction::RIGHT);
    default:
      return std::nullopt;
  }
}

// Player whose cursor position a key submits
std::optional<PlayerId> trySubmittingPlayer(const Key& key) {
  if (!std::holds_alternative<char>(key)) {
    return std::nullopt;
  }
  switch (std::get<char>(key)) {
    case 'e':
      return PlayerId{PLAYER_A};
    case '\n':
    case '\r':
      return PlayerId{PLAYER_B};
    default:
      return std::nullopt;
  }
}

Point moveCursor(Point p, Direction direction, const Board& board) {
  switch (direction) {
    case Direction::UP:
      return Point{p.x, (p.y - 1 + board.height) % board.height};
    case Direction::DOWN:
      return Point{p.x, (p.y + 1) % board.height};
    case Direction::LEFT:
      return Point{(p.x - 1 + board.width) % board.width, p.y};
    case Direction::RIGHT:
      return Point{(p.x + 1) % board.width, p.y};
  }
  return p;
}

}  // namespace

Cursors makeCursors(const orbital::game_boundary::Views& views) {
  Cursors cursors;
  for (const auto& [player, snake] : views.arena.snakes) {
    cursors[player] = snake_model::head(snake);
  }
  return cursors;
}

std::tuple<Cursors, std::optional<Intent>> interpretKey(const Board& board, Cursors cursors, const Key& key) {
  if (auto move = tryCursorMove(key)) {
    auto [player, direction] = *move;
    if (auto cursor = cursors.find(player); cursor != cursors.end()) {
      cursor->second = moveCursor(cursor->second, direction, board);
    }
    return {std::move(cursors), std::nullopt};
  }
  if (auto player = trySubmittingPlayer(key)) {
    auto cursor = cursors.find(*player);
    if (cursor == cursors.end()) {
      return {std::move(cursors), std::nullopt};
    }
    Intent submit = orbital::game_boundary::Submit{*player, cursor->second};
    return {std::move(cursors), submit};
  }
  if (key == Key{'n'}) {
    return {std::move(cursors), Intent{NewGame{}}};
  }
  if (key == Key{'q'}) {
    return {std::move(cursors), Intent{Quit{}}};
  }
  return {std::move(cursors), std::nullopt};
}

}  // namespace orbital_controls
}  // namespace snake
