// Orbital Orders in the terminal: a thin shell around the Orbital Game Domain Application.
//
// The shell measures real time and feeds it as game time, turns keys into submissions and draws the
// views. Every game decision stays in the Domain Application.

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <tuple>
#include <variant>

#include "orbital/orbital_game_domain_application.hpp"
#include "snake/orbital_controls.hpp"
#include "snake/orbital_frame.hpp"

namespace {

using snake::RandomIntGeneratorFn;
namespace orbital = snake::orbital;
namespace orbital_controls = snake::orbital_controls;
namespace orbital_frame = snake::orbital_frame;
namespace app = orbital::orbital_game_domain_application;
using orbital::game_boundary::Submit;
using orbital::game_boundary::Views;

constexpr std::chrono::milliseconds FRAME_INTERVAL{50};

// Terminal in raw mode for the lifetime of this object: keys arrive unbuffered and are not echoed
class RawTerminal {
 public:
  RawTerminal() {
    if (tcgetattr(STDIN_FILENO, &original_) == 0) {
      termios raw = original_;
      raw.c_lflag &= ~(ICANON | ECHO);
      raw.c_cc[VMIN] = 0;
      raw.c_cc[VTIME] = 0;
      enabled_ = tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0;
    }
    std::cout << "\033[?25l";  // Hide the cursor
  }
  ~RawTerminal() {
    if (enabled_) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
    }
    std::cout << "\033[?25h" << std::flush;
  }
  RawTerminal(const RawTerminal&) = delete;
  RawTerminal& operator=(const RawTerminal&) = delete;
  RawTerminal(RawTerminal&&) = delete;
  RawTerminal& operator=(RawTerminal&&) = delete;

 private:
  termios original_{};
  bool enabled_{false};
};

std::optional<char> tryReadChar() {
  pollfd fd{STDIN_FILENO, POLLIN, 0};
  if (poll(&fd, 1, 0) <= 0) {
    return std::nullopt;
  }
  char ch = 0;
  if (read(STDIN_FILENO, &ch, 1) != 1) {
    return std::nullopt;
  }
  return ch;
}

void draw(const Views& views, const orbital_controls::Cursors& cursors) {
  std::string screen = "\033[H\033[2J";
  screen += "funkysnakes: Orbital Orders\n";
  for (const std::string& line : orbital_frame::render(views, cursors)) {
    screen += line + "\n";
  }
  screen += "A: w/a/s/d cursor, e order   B: arrows cursor, Enter order   n new game   q quit\n";
  std::cout << screen << std::flush;
}

}  // namespace

int main() {
  std::mt19937 engine{std::random_device{}()};
  RandomIntGeneratorFn random_int_fn = [&engine](int min, int max) {
    return std::uniform_int_distribution<int>{min, max}(engine);
  };

  RawTerminal terminal;
  int round = 0;
  auto [state, observations] = app::apply(random_int_fn, app::initial(), app::Start{orbital::RoundId{++round}});
  Views views = *observations.views;
  orbital_controls::Cursors cursors = orbital_controls::makeCursors(views);
  orbital_controls::KeyParseState parse_state = orbital_controls::KeyParseState::NORMAL;
  auto last = std::chrono::steady_clock::now();

  bool quit = false;
  while (!quit) {
    while (auto ch = tryReadChar()) {
      auto [next_parse_state, key] = orbital_controls::parseKey(parse_state, *ch);
      parse_state = next_parse_state;
      if (!key) {
        continue;
      }
      auto [next_cursors, intent] = orbital_controls::interpretKey(views.arena.board, std::move(cursors), *key);
      cursors = std::move(next_cursors);
      if (!intent) {
        continue;
      }
      if (const auto* submit = std::get_if<Submit>(&*intent)) {
        std::tie(state, observations) = app::apply(std::move(state), *submit);
      } else if (std::holds_alternative<orbital_controls::NewGame>(*intent)) {
        std::tie(state, observations) =
            app::apply(random_int_fn, std::move(state), app::Start{orbital::RoundId{++round}});
        cursors = orbital_controls::makeCursors(*observations.views);
      } else {
        quit = true;
      }
      views = *observations.views;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
    last += elapsed;  // Keep the sub-millisecond remainder for the next frame
    std::tie(state, observations) = app::apply(random_int_fn, std::move(state), app::TimeElapsed{elapsed});
    views = *observations.views;

    draw(views, cursors);
    std::this_thread::sleep_for(FRAME_INTERVAL);
  }
  return 0;
}
