// Orbital Orders in the terminal: a thin shell around the Orbital Game Domain Application.
//
// The shell uses both interfaces of the Domain Application: through the execution interface it supplies
// a seeded random engine and real time as passing game time; at the boundary it turns keys into
// submissions and, as the players' side, draws the views it receives. Every game decision stays in the
// Domain Application.

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
#include "snake/orbital_view.hpp"

namespace {

using snake::Key;
using snake::RandomIntGeneratorFn;
using snake::SpecialKey;
namespace orbital = snake::orbital;
namespace orbital_controls = snake::orbital_controls;
namespace orbital_frame = snake::orbital_frame;
namespace orbital_view = snake::orbital_view;
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

// The players' side in the terminal: it shows the game through its views, which carry every outcome
class TerminalPlayers : public orbital::game_boundary::PlayersEndOutbound {
 public:
  void startRejected(const orbital::game_boundary::StartRejected& /* rejected */) override {}
  void submissionOutcome(const orbital::game_boundary::SubmissionOutcome& /* outcome */) override {}
  void orderCommitted(const orbital::OrderCommitted& /* committed */) override {}
  void stepReported(const orbital::game_boundary::StepReport& /* step */) override {}
  void ordersEndedWithRound(const orbital::ArenaEvents& /* events */) override {}
  void concluded(const orbital::game_authority::GameConcluded& /* concluded */) override {}
  void viewsUpdated(const Views& views) override { views_ = views; }

  // Views of the current game; the game reports them once it started
  const Views& views() const { return *views_; }

 private:
  std::optional<Views> views_;
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

}  // namespace

int main() {
  std::mt19937 engine{std::random_device{}()};
  RandomIntGeneratorFn random_int_fn = [&engine](int min, int max) {
    return std::uniform_int_distribution<int>{min, max}(engine);
  };

  TerminalPlayers players;
  int round = 0;
  std::optional<app::DomainApplication> game;
  // One Domain Application plays one game, so every game starts in a fresh one
  auto start_new_game = [&] {
    game.emplace(players, random_int_fn);
    game->start(app::Start{orbital::RoundId{++round}});
  };
  start_new_game();
  orbital_controls::Cursors cursors = orbital_controls::makeCursors(players.views());

  const NulDroppingCout nul_dropping_cout;  // Outlives the screen, which writes on teardown too
  auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
  auto on_key = [&](const Key& key) {
    auto [next_cursors, intent] = orbital_controls::interpretKey(players.views().arena.board, std::move(cursors), key);
    cursors = std::move(next_cursors);
    if (!intent) {
      return;
    }
    if (const auto* submit = std::get_if<Submit>(&*intent)) {
      game->submit(*submit);
    } else if (std::holds_alternative<orbital_controls::NewGame>(*intent)) {
      start_new_game();
      cursors = orbital_controls::makeCursors(players.views());
    } else {
      screen.Exit();
    }
  };
  auto component = ftxui::CatchEvent(
      ftxui::Renderer([&] { return orbital_view::draw(orbital_frame::build(players.views(), cursors)); }),
      [&](const ftxui::Event& event) {
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
    game->elapse(app::TimeElapsed{elapsed});

    screen.PostEvent(ftxui::Event::Custom);  // Redraw even without keys
    loop.RunOnce();                          // Handles the pending keys, then draws
    std::this_thread::sleep_for(FRAME_INTERVAL);
  }
  return 0;
}
