// Orbital Orders in the terminal: a thin shell around the Orbital Game Domain Application.
//
// The shell measures real time and feeds it as game time, turns keys into submissions and draws the
// views. Every game decision stays in the Domain Application.

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <random>
#include <streambuf>
#include <string>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/loop.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "orbital/orbital_game_domain_application.hpp"
#include "snake/key.hpp"
#include "snake/orbital_controls.hpp"
#include "snake/orbital_frame.hpp"

namespace {

using snake::Key;
using snake::RandomIntGeneratorFn;
using snake::SpecialKey;
namespace orbital = snake::orbital;
namespace orbital_controls = snake::orbital_controls;
namespace orbital_frame = snake::orbital_frame;
namespace app = orbital::orbital_game_domain_application;
using orbital::game_boundary::Submit;
using orbital::game_boundary::Views;

constexpr std::chrono::milliseconds FRAME_INTERVAL{50};

// Drops the NUL byte FTXUI appends to every frame as a flush marker for the web. Terminals ignore it,
// but vim's :terminal prints it in the bottom right corner and scrolls the frame by a line.
class NulDroppingBuffer : public std::streambuf {
 public:
  explicit NulDroppingBuffer(std::streambuf* target) : target_{target} {}

 protected:
  int_type overflow(int_type ch) override {
    if (traits_type::eq_int_type(ch, traits_type::eof()) || traits_type::to_char_type(ch) == '\0') {
      return traits_type::not_eof(ch);
    }
    return target_->sputc(traits_type::to_char_type(ch));
  }

  std::streamsize xsputn(const char* text, std::streamsize count) override {
    const char* begin = text;
    const char* const end = text + count;
    while (begin != end) {
      const char* nul = std::find(begin, end, '\0');
      if (target_->sputn(begin, nul - begin) != nul - begin) {
        return begin - text;
      }
      begin = nul == end ? end : nul + 1;
    }
    return count;
  }

  int sync() override { return target_->pubsync(); }

 private:
  std::streambuf* target_;
};

// Routes std::cout through a NulDroppingBuffer for the lifetime of this object
class NulDroppingCout {
 public:
  NulDroppingCout() : original_{std::cout.rdbuf()}, buffer_{original_} { std::cout.rdbuf(&buffer_); }
  ~NulDroppingCout() {
    std::cout.flush();
    std::cout.rdbuf(original_);
  }
  NulDroppingCout(const NulDroppingCout&) = delete;
  NulDroppingCout& operator=(const NulDroppingCout&) = delete;
  NulDroppingCout(NulDroppingCout&&) = delete;
  NulDroppingCout& operator=(NulDroppingCout&&) = delete;

 private:
  std::streambuf* original_;
  NulDroppingBuffer buffer_;
};

// Key of a terminal event, if the shell has a use for it
std::optional<Key> tryKey(const ftxui::Event& event) {
  if (event == ftxui::Event::ArrowUp) {
    return Key{SpecialKey::ARROW_UP};
  }
  if (event == ftxui::Event::ArrowDown) {
    return Key{SpecialKey::ARROW_DOWN};
  }
  if (event == ftxui::Event::ArrowLeft) {
    return Key{SpecialKey::ARROW_LEFT};
  }
  if (event == ftxui::Event::ArrowRight) {
    return Key{SpecialKey::ARROW_RIGHT};
  }
  if (event == ftxui::Event::Return) {
    return Key{'\n'};
  }
  if (event.is_character() && event.character().size() == 1) {
    return Key{event.character().front()};
  }
  return std::nullopt;
}

ftxui::Element draw(const Views& views, const orbital_controls::Cursors& cursors) {
  std::vector<ftxui::Element> lines;
  lines.push_back(ftxui::text("funkysnakes: Orbital Orders"));
  for (const std::string& line : orbital_frame::render(views, cursors)) {
    lines.push_back(ftxui::text(line));
  }
  lines.push_back(ftxui::text("A: w/a/s/d cursor, e order   B: arrows cursor, Enter order   n new game   q quit"));
  return ftxui::vbox(std::move(lines));
}

}  // namespace

int main() {
  std::mt19937 engine{std::random_device{}()};
  RandomIntGeneratorFn random_int_fn = [&engine](int min, int max) {
    return std::uniform_int_distribution<int>{min, max}(engine);
  };

  int round = 0;
  // Named variables, not a structured binding: lambdas may only capture bindings since C++20
  app::State state;
  orbital::game_boundary::Observations observations;
  std::tie(state, observations) = app::apply(random_int_fn, app::initial(), app::Start{orbital::RoundId{++round}});
  Views views = *observations.views;
  orbital_controls::Cursors cursors = orbital_controls::makeCursors(views);

  const NulDroppingCout nul_dropping_cout;  // Outlives the screen, which writes on teardown too
  auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
  auto on_key = [&](const Key& key) {
    auto [next_cursors, intent] = orbital_controls::interpretKey(views.arena.board, std::move(cursors), key);
    cursors = std::move(next_cursors);
    if (!intent) {
      return;
    }
    if (const auto* submit = std::get_if<Submit>(&*intent)) {
      std::tie(state, observations) = app::apply(std::move(state), *submit);
    } else if (std::holds_alternative<orbital_controls::NewGame>(*intent)) {
      std::tie(state, observations) =
          app::apply(random_int_fn, std::move(state), app::Start{orbital::RoundId{++round}});
      cursors = orbital_controls::makeCursors(*observations.views);
    } else {
      screen.Exit();
    }
    views = *observations.views;
  };
  auto component =
      ftxui::CatchEvent(ftxui::Renderer([&] { return draw(views, cursors); }), [&](const ftxui::Event& event) {
        if (auto key = tryKey(event)) {
          on_key(*key);
          return true;
        }
        return false;
      });

  ftxui::Loop loop{&screen, component};
  auto last = std::chrono::steady_clock::now();
  while (!loop.HasQuitted()) {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
    last += elapsed;  // Keep the sub-millisecond remainder for the next frame
    std::tie(state, observations) = app::apply(random_int_fn, std::move(state), app::TimeElapsed{elapsed});
    views = *observations.views;

    screen.PostEvent(ftxui::Event::Custom);  // Redraw even without keys
    loop.RunOnce();                          // Handles the pending keys, then draws
    std::this_thread::sleep_for(FRAME_INTERVAL);
  }
  return 0;
}
