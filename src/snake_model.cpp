#include <algorithm>

#include "snake/snake_model_evolve.hpp"

namespace snake {
namespace snake_model {

namespace {

bool isOpposite(Direction a, Direction b) {
  return (a == Direction::UP && b == Direction::DOWN) || (a == Direction::DOWN && b == Direction::UP) ||
         (a == Direction::LEFT && b == Direction::RIGHT) || (a == Direction::RIGHT && b == Direction::LEFT);
}

Direction resolveDirection(const Snake& s, Direction requested) {
  if (length(s) >= 2 && isOpposite(requested, currentDirection(s))) {
    return currentDirection(s);
  }
  return requested;
}

Point advancePoint(Point p, Direction dir) {
  switch (dir) {
    case Direction::LEFT:
      p.x -= 1;
      break;
    case Direction::RIGHT:
      p.x += 1;
      break;
    case Direction::UP:
      p.y -= 1;
      break;
    case Direction::DOWN:
      p.y += 1;
      break;
  }
  return p;
}

Point wrapPoint(Point p, const Board& board) {
  if (p.x < 0) {
    p.x = board.width - 1;
  } else if (p.x >= board.width) {
    p.x = 0;
  }

  if (p.y < 0) {
    p.y = board.height - 1;
  } else if (p.y >= board.height) {
    p.y = 0;
  }

  return p;
}

}  // namespace

Point nextHead(const Snake& s, Direction dir, const Board& board) {
  Direction effective = resolveDirection(s, dir);
  return wrapPoint(advancePoint(head(s), effective), board);
}

Snake initial(Point head, Direction dir, int length) {
  Snake s;
  s.head_ = head;
  s.current_direction_ = dir;
  s.alive_ = true;

  for (int i = 1; i < length; ++i) {
    Point tail_segment = head;
    switch (dir) {
      case Direction::RIGHT:
        tail_segment.x = head.x - i;
        break;
      case Direction::LEFT:
        tail_segment.x = head.x + i;
        break;
      case Direction::DOWN:
        tail_segment.y = head.y - i;
        break;
      case Direction::UP:
        tail_segment.y = head.y + i;
        break;
    }
    s.tail_.push_back(tail_segment);
  }

  return s;
}

Snake move(Snake s, Direction dir, const Board& board) {
  if (!s.alive_) return s;
  s = grow(std::move(s), dir, board);
  s.tail_.pop_back();
  return s;
}

Snake grow(Snake s, Direction dir, const Board& board) {
  if (!s.alive_) return s;

  Direction effective = resolveDirection(s, dir);
  Point new_head = wrapPoint(advancePoint(s.head_, effective), board);

  std::vector<Point> new_tail;
  new_tail.reserve(s.tail_.size() + 1);
  new_tail.push_back(s.head_);
  new_tail.insert(new_tail.end(), s.tail_.begin(), s.tail_.end());

  s.head_ = new_head;
  s.tail_ = std::move(new_tail);
  s.current_direction_ = effective;
  return s;
}

std::tuple<Snake, std::vector<Point>> cutAt(Snake s, Point cut_point) {
  std::vector<Point> cut_segments;

  if (s.head_ == cut_point) {
    return {s, cut_segments};
  }

  auto it = std::find(s.tail_.begin(), s.tail_.end(), cut_point);
  if (it != s.tail_.end()) {
    cut_segments = std::vector<Point>(it, s.tail_.end());
    s.tail_ = std::vector<Point>(s.tail_.begin(), it);
  }

  return {s, cut_segments};
}

Snake kill(Snake s) {
  s.alive_ = false;
  return s;
}

}  // namespace snake_model
}  // namespace snake
