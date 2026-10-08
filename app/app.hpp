// Visualiser state. The search runs in one library call; the app then replays the recorded
// expansion order at a set number of cells per second, so the window stays responsive and
// the timing shown is the search alone.
#pragma once

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>
#include <vector>

#include "pathfinder/grid.hpp"
#include "pathfinder/search.hpp"

namespace app {

inline constexpr float kPanelWidth = 360.0f;

enum class Generator { Empty, Maze, RandomWalls, Terrain };
enum class Tool { Wall, Mud, Erase };
enum class Phase { Idle, Expanding, Tracing, Done };

struct Settings {
    int cols = 61;  // odd, so mazes fill the grid
    int rows = 41;
    Generator generator = Generator::Maze;
    float density = 0.25f;  // share of walls, for random walls and terrain
    int seed = 7;
};

struct Comparison {
    pf::Algorithm algorithm;
    pf::SearchResult result;
    double ms;
};

struct State {
    Settings settings;
    pf::Grid grid{61, 41};
    pf::Point start{1, 1};
    pf::Point goal{59, 39};
    pf::Algorithm algorithm = pf::Algorithm::AStar;

    // the last search and its replay
    pf::SearchResult result;
    double search_ms = 0.0;
    Phase phase = Phase::Idle;
    double revealed = 0.0;      // expanded cells shown so far (fractional, for smooth replay)
    double clock = 0.0;         // seconds since the replay started (keeps running when done)
    std::vector<float> revealed_at;  // when each shown cell appeared, for the fade-in
    double path_shown = 0.0;    // path cells drawn so far
    float speed = 1500.0f;      // cells per second
    bool paused = false;

    std::vector<Comparison> comparison;  // "Compare all" results on the current grid

    Tool tool = Tool::Wall;
    int mud_cost = 5;
    std::optional<pf::Point> hover;
};

// Grid placement inside the window: everything left of the panel, centred, square cells.
struct Layout {
    sf::Vector2f origin;
    float cell = 0.0f;
    [[nodiscard]] std::optional<pf::Point> cell_at(sf::Vector2f pixel, const pf::Grid& grid) const;
    [[nodiscard]] sf::Vector2f centre(pf::Point p) const {
        return {origin.x + (static_cast<float>(p.x) + 0.5f) * cell,
                origin.y + (static_cast<float>(p.y) + 0.5f) * cell};
    }
};

// model.cpp
void generate(State& s);
void run(State& s);
void compare_all(State& s);
void clear_search(State& s);
void advance(State& s, float dt);  // replay; dt in seconds
void skip_to_end(State& s);

// scene.cpp
Layout layout_for(sf::Vector2f window, const pf::Grid& grid);
void draw_scene(sf::RenderTarget& target, const State& s, const Layout& layout);

// panel.cpp (Dear ImGui)
bool load_fonts(const std::string& asset_dir);
void apply_theme();
void draw_panel(State& s, sf::Vector2f window);
void draw_overlay(const State& s, const Layout& layout);  // legend + hover info on the canvas

}  // namespace app
