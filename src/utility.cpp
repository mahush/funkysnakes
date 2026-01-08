#include "snake/utility.hpp"

#include <random>

namespace snake {

RandomIntGeneratorFn makeRandomIntGenerator() {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  return [](int min, int max) mutable {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
  };
}

}  // namespace snake
