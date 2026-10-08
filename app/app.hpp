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

inline constexpr float kPanelWidth = 320.0f;

enum class Generator { Empty, RandomWalls, Maze, Terrain };
enum class Phase { Idle, Expanding, Tracing, Done };
enum class Tool { Wall, Mud, Erase };

struct Comparison {
    pf::Algorithm algorithm;
    pf::SearchResult result;
    double ms;
};

struct State {
    // grid settings
    int size = 41;  // cells per side; odd, so mazes fill the grid
    int cols = 41;
    int rows = 41;
    int seed = 1;
    Generator generator = Generator::Empty;
    float density = 0.30f;  // share of walls for random mazes and terrain
    Tool tool = Tool::Wall;
    int mud_cost = 6;
    pf::Grid grid{41, 41};
    pf::Point start{1, 1};
    pf::Point goal{39, 39};
    pf::Algorithm algorithm = pf::Algorithm::BFS;
    float astar_weight = 1.0f;  // > 1: weighted A*, faster, at most this many times the optimum

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
    bool wavefront = true;           // shade visited cells by the order they were reached

    std::vector<Comparison> comparison;  // "Compare all" on the current grid
    std::string status = "Draw walls, then pick an algorithm";
    std::optional<pf::Point> hover;

    // panel
    int tab = 0;
    int dragging_slider = -1;
};

// Where the grid sits in the window: left of the panel, centred, square cells.
struct Layout {
    sf::Vector2f origin;
    float cell = 0.0f;
    [[nodiscard]] std::optional<pf::Point> cell_at(sf::Vector2f pixel, const pf::Grid& grid) const;
};

// model.cpp
void generate(State& s);  // a new grid from the settings, same seed
void run(State& s, pf::Algorithm algorithm);
void compare_all(State& s);
void clear_search(State& s);
void reset_grid(State& s);
void advance(State& s, float dt);  // dt in seconds
void skip_to_end(State& s);

// scene.cpp
Layout layout_for(sf::Vector2f window, const pf::Grid& grid);
void draw_scene(sf::RenderTarget& target, const State& s, const Layout& layout);

void draw_canvas_overlay(sf::RenderTarget& target, const State& s, const Layout& layout, const sf::Font& font);

// panel.cpp: the side panel, drawn with the ui widgets.
struct PanelInput {
    sf::Vector2f mouse;
    bool pressed = false;  // left button went down this frame, over the panel
    bool down = false;     // left button held
};
void draw_panel(sf::RenderTarget& target, State& s, const sf::Font& font, sf::Vector2f window, const PanelInput& in);

}  // namespace app
