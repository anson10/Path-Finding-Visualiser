// The visualiser's state. The search itself runs in one library call; the app then replays
// the recorded expansion order a few cells per frame, so the window never blocks and the
// timing shown is the search alone.
#pragma once

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>

#include "pathfinder/grid.hpp"
#include "pathfinder/search.hpp"

namespace app {

inline constexpr int kCols = 45;  // odd, so a maze fills the grid
inline constexpr int kRows = 35;
inline constexpr int kCell = 20;
inline constexpr int kGridWidth = kCols * kCell;
inline constexpr int kPanelWidth = 300;
inline constexpr int kWidth = kGridWidth + kPanelWidth;
inline constexpr int kHeight = kRows * kCell;
inline constexpr std::uint8_t kMudCost = 5;

enum class Phase { Editing, Expanding, Tracing, Done };

enum class Action { Run, Maze, RandomWalls, Terrain, ClearPath, ClearAll };

struct Button {
    sf::FloatRect bounds;
    std::string label;
    std::optional<pf::Algorithm> algorithm;  // an algorithm selector, or
    std::optional<Action> action;            // a command
};

struct State {
    pf::Grid grid{kCols, kRows};
    pf::Point start{1, 1};
    pf::Point goal{kCols - 2, kRows - 2};
    pf::Algorithm algorithm = pf::Algorithm::AStar;
    pf::SearchResult result;
    double search_ms = 0.0;
    Phase phase = Phase::Editing;
    std::size_t shown_expanded = 0;  // replay cursor into result.expanded
    std::size_t shown_path = 0;      // replay cursor into result.path
    int speed = 4;                   // cells revealed per frame
    std::string status = "Drag walls, shift-drag mud, drag S/G to move them. Space runs.";
};

std::vector<Button> layout_buttons();

// Runs the selected algorithm on the current grid and starts the replay.
void run_search(State& state);
// Advances the replay; finishes instantly when `instant` is set (screenshots).
void step_replay(State& state, bool instant = false);
void clear_search(State& state);

void draw(sf::RenderTarget& target, const State& state, const std::vector<Button>& buttons,
          const sf::Font& font);

}  // namespace app
