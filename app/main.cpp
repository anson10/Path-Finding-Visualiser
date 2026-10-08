// Pathfinding visualiser: edit a grid, pick an algorithm, watch the recorded search replay.
//
//   pathfinder-app
//   pathfinder-app --screenshot out.png [--scene maze|walls|terrain] [--algorithm astar] [--seed 3]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "app.hpp"

namespace {

using app::Action;
using app::State;

std::optional<pf::Point> cell_at(sf::Vector2i pixel) {
    const pf::Point p{pixel.x / app::kCell, pixel.y / app::kCell};
    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= app::kGridWidth || p.y >= app::kRows) return std::nullopt;
    return p;
}

void keep_endpoints_open(State& s) {
    s.grid.set_cost(s.start, pf::Grid::kOpen);
    s.grid.set_cost(s.goal, pf::Grid::kOpen);
}

void apply(State& s, Action action, std::uint64_t seed) {
    switch (action) {
        case Action::Run:
            app::clear_search(s);
            app::run_search(s);
            return;
        case Action::Maze:
            s.grid = pf::perfect_maze(app::kCols, app::kRows, seed);
            s.status = "Perfect maze: exactly one path between any two open cells.";
            break;
        case Action::RandomWalls:
            s.grid = pf::random_walls(app::kCols, app::kRows, 0.3, seed);
            s.status = "30% random walls, every open cell costs 1.";
            break;
        case Action::Terrain:
            s.grid = pf::random_terrain(app::kCols, app::kRows, 0.15, 9, seed);
            s.status = "Weighted terrain (darker = costlier): compare BFS with Dijkstra.";
            break;
        case Action::ClearPath:
            break;
        case Action::ClearAll:
            s.grid.fill(pf::Grid::kOpen);
            s.status = "Cleared.";
            break;
    }
    keep_endpoints_open(s);
    app::clear_search(s);
}

std::optional<pf::Algorithm> parse_algorithm(std::string_view name) {
    if (name == "bfs") return pf::Algorithm::BFS;
    if (name == "dfs") return pf::Algorithm::DFS;
    if (name == "dijkstra") return pf::Algorithm::Dijkstra;
    if (name == "astar") return pf::Algorithm::AStar;
    if (name == "greedy") return pf::Algorithm::Greedy;
    return std::nullopt;
}

const char* option(int argc, char** argv, const char* flag, const char* fallback) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    return fallback;
}

bool load_font(sf::Font& font) {
    for (const char* dir : {"assets", PATHFINDER_ASSETS}) {
        if (font.loadFromFile(std::string(dir) + "/fonts/arvo.ttf")) return true;
    }
    std::fprintf(stderr, "could not load assets/fonts/arvo.ttf\n");
    return false;
}

// Renders a finished search off screen and saves it: reproducible README images.
int screenshot(int argc, char** argv, const sf::Font& font) {
    State s;
    const std::string_view scene = option(argc, argv, "--scene", "maze");
    const auto seed = static_cast<std::uint64_t>(std::strtoull(option(argc, argv, "--seed", "3"), nullptr, 10));
    apply(s, scene == "walls" ? Action::RandomWalls : scene == "terrain" ? Action::Terrain : Action::Maze, seed);
    const auto algorithm = parse_algorithm(option(argc, argv, "--algorithm", "astar"));
    if (!algorithm) {
        std::fprintf(stderr, "unknown --algorithm (bfs, dfs, dijkstra, astar, greedy)\n");
        return EXIT_FAILURE;
    }
    s.algorithm = *algorithm;
    app::run_search(s);
    app::step_replay(s, /*instant=*/true);

    sf::RenderTexture texture;
    if (!texture.create(app::kWidth, app::kHeight)) return EXIT_FAILURE;
    app::draw(texture, s, app::layout_buttons(), font);
    texture.display();
    const char* out = option(argc, argv, "--screenshot", "screenshot.png");
    if (!texture.getTexture().copyToImage().saveToFile(out)) return EXIT_FAILURE;
    std::printf("wrote %s\n", out);
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
    sf::Font font;
    if (!load_font(font)) return EXIT_FAILURE;
    if (std::string_view(option(argc, argv, "--screenshot", "")) != "") return screenshot(argc, argv, font);

    sf::RenderWindow window(sf::VideoMode(app::kWidth, app::kHeight), "Pathfinding Visualiser");
    window.setFramerateLimit(60);
    const auto buttons = app::layout_buttons();
    State s;
    std::uint64_t seed = 1;
    enum class Drag { None, Paint, Mud, Erase, Start, Goal } drag = Drag::None;

    auto paint = [&](sf::Vector2i pixel) {
        const auto p = cell_at(pixel);
        if (!p || s.phase == app::Phase::Expanding || s.phase == app::Phase::Tracing) return;
        if (drag == Drag::Start || drag == Drag::Goal) {
            if (!s.grid.passable(*p) || *p == (drag == Drag::Start ? s.goal : s.start)) return;
            (drag == Drag::Start ? s.start : s.goal) = *p;
        } else if (*p != s.start && *p != s.goal) {
            s.grid.set_cost(*p, drag == Drag::Paint ? pf::Grid::kWall
                                : drag == Drag::Mud ? app::kMudCost
                                                    : pf::Grid::kOpen);
        }
        app::clear_search(s);
    };

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            if (e.type == sf::Event::KeyPressed) {
                const auto key = e.key.code;
                if (key >= sf::Keyboard::Num1 && key <= sf::Keyboard::Num5)
                    s.algorithm = pf::kAlgorithms[key - sf::Keyboard::Num1];
                else if (key == sf::Keyboard::Space) apply(s, Action::Run, seed);
                else if (key == sf::Keyboard::M) apply(s, Action::Maze, ++seed);
                else if (key == sf::Keyboard::R) apply(s, Action::RandomWalls, ++seed);
                else if (key == sf::Keyboard::T) apply(s, Action::Terrain, ++seed);
                else if (key == sf::Keyboard::C) apply(s, Action::ClearPath, seed);
                else if (key == sf::Keyboard::X) apply(s, Action::ClearAll, seed);
                else if (key == sf::Keyboard::Add || key == sf::Keyboard::Equal) s.speed = std::min(s.speed * 2, 256);
                else if (key == sf::Keyboard::Subtract || key == sf::Keyboard::Hyphen) s.speed = std::max(s.speed / 2, 1);
            }
            if (e.type == sf::Event::MouseButtonPressed) {
                const sf::Vector2f at{static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y)};
                bool on_button = false;
                for (const auto& b : buttons) {
                    if (!b.bounds.contains(at)) continue;
                    on_button = true;
                    if (b.algorithm) s.algorithm = *b.algorithm;
                    if (b.action) apply(s, *b.action, *b.action == Action::Run ? seed : ++seed);
                }
                const auto cell = cell_at({e.mouseButton.x, e.mouseButton.y});
                if (on_button || !cell) continue;
                const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ||
                                   sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
                if (e.mouseButton.button == sf::Mouse::Right) drag = Drag::Erase;
                else if (*cell == s.start) drag = Drag::Start;
                else if (*cell == s.goal) drag = Drag::Goal;
                else drag = shift ? Drag::Mud : Drag::Paint;
                paint({e.mouseButton.x, e.mouseButton.y});
            }
            if (e.type == sf::Event::MouseMoved && drag != Drag::None) paint({e.mouseMove.x, e.mouseMove.y});
            if (e.type == sf::Event::MouseButtonReleased) drag = Drag::None;
        }
        app::step_replay(s);
        app::draw(window, s, buttons, font);
        window.display();
    }
    return EXIT_SUCCESS;
}
