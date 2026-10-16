#include "snake/orbital_food_labels.hpp"

#include <algorithm>

#include "common/players.hpp"

namespace snake {
namespace orbital_food_labels {

namespace {

constexpr int LABEL_COUNT = 10;

}  // namespace

FoodLabels relabel(FoodLabels labels, const FoodItems& food) {
  // Food not yet claimed by a label; one entry per item, as items may share a position
  FoodItems unlabelled = food;
  for (auto label = labels.begin(); label != labels.end();) {
    auto item = std::find(unlabelled.begin(), unlabelled.end(), label->second);
    if (item == unlabelled.end()) {
      label = labels.erase(label);
    } else {
      unlabelled.erase(item);
      ++label;
    }
  }
  auto item = unlabelled.begin();
  for (int label = 0; label < LABEL_COUNT && item != unlabelled.end(); ++label) {
    if (labels.count(label) == 0) {
      labels[label] = *item++;
    }
  }
  return labels;
}

std::optional<orbital::game_boundary::Submit> trySubmitFood(const FoodLabels& labels, const Key& key) {
  const char* character = std::get_if<char>(&key);
  if (character == nullptr || *character < '0' || *character > '9') {
    return std::nullopt;
  }
  auto label = labels.find(*character - '0');
  if (label == labels.end()) {
    return std::nullopt;
  }
  return orbital::game_boundary::Submit{PlayerId{PLAYER_A}, label->second};
}

}  // namespace orbital_food_labels
}  // namespace snake
