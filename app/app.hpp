// Visualiser state. The search runs in one library call; the app then replays the recorded
// expansion order at a set number of cells per second, so the window stays responsive and
// the time shown is the search alone.
#pragma once

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>
#include <vector>

#include "pathfinder/grid.hpp"
#include "pathfinder/search.hpp"

namespace app {

inline constexpr float kPanelWidth = 300.0f;

enum class Generator { Empty, Maze, RandomWalls, Terrain };
enum class Phase { Idle, Expanding, Tracing, Done };

struct Comparison {
    pf::Algorithm algorithm;
    pf::SearchResult result;
    double ms;
};

struct State {
    int cols = 41;
    int rows = 41;
    int seed = 1;
    pf::Grid grid{41, 41};
    pf::Point start{1, 1};
    pf::Point goal{39, 39};
    pf::Algorithm algorithm = pf::Algorithm::BFS;

    // the last search and its replay
    pf::SearchResult result;
    double search_ms = 0.0;
    Phase phase = Phase::Idle;
    double revealed = 0.0;           // expanded cells shown so far (fractional: smooth replay)
    double path_shown = 0.0;         // path cells shown so far
    double clock = 0.0;              // seconds since the replay started
    std::vector<float> revealed_at;  // when each shown cell appeared, for the fade-in
    float speed = 600.0f;            // cells per second
    bool paused = false;

    std::vector<Comparison> comparison;  // "Compare all" on the current grid
    std::string status = "Draw walls, then pick an algorithm";
    std::optional<pf::Point> hover;
};

// Where the grid sits in the window: left of the panel, centred, square cells.
struct Layout {
    sf::Vector2f origin;
    float cell = 0.0f;
    [[nodiscard]] std::optional<pf::Point> cell_at(sf::Vector2f pixel, const pf::Grid& grid) const;
};

// model.cpp
void generate(State& s, Generator generator);
void run(State& s, pf::Algorithm algorithm);
void compare_all(State& s);
void clear_search(State& s);
void reset_grid(State& s);
void advance(State& s, float dt);  // dt in seconds
void skip_to_end(State& s);

// scene.cpp
Layout layout_for(sf::Vector2f window, const pf::Grid& grid);
void draw_scene(sf::RenderTarget& target, const State& s, const Layout& layout);

// panel.cpp: the side panel, drawn with SFML. `clicked` is true on the frame the left
// button went down; buttons act on it directly.
void draw_panel(sf::RenderTarget& target, State& s, const sf::Font& font, sf::Vector2f window,
                sf::Vector2f mouse, bool clicked);

}  // namespace app
