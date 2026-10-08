// Pathfinding visualiser: draw walls, pick an algorithm, watch the recorded search replay.
//
//   pathfinder-app
//   pathfinder-app --screenshot out.png [--scene maze|walls|terrain] [--algorithm bfs]
//                  [--seed 7] [--weight 1.5] [--compare] [--grid-tab]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "app.hpp"

namespace {

using app::State;

const char* option(int argc, char** argv, const char* flag, const char* fallback) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    return fallback;
}

bool has_flag(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return true;
    return false;
}

bool load_font(sf::Font& font) {
    for (const char* dir : {"assets", PATHFINDER_ASSETS})
        if (font.loadFromFile(std::string(dir) + "/fonts/arvo.ttf")) return true;
    std::fprintf(stderr, "Failed to load font: assets/fonts/arvo.ttf\n");
    return false;
}

std::optional<pf::Algorithm> parse_algorithm(std::string_view name) {
    if (name == "bfs") return pf::Algorithm::BFS;
    if (name == "dfs") return pf::Algorithm::DFS;
    if (name == "dijkstra") return pf::Algorithm::Dijkstra;
    if (name == "astar") return pf::Algorithm::AStar;
    if (name == "greedy") return pf::Algorithm::Greedy;
    return std::nullopt;
}

// Every cell on the line between two cells (Bresenham), so a fast drag draws a solid wall
// instead of a dotted one.
template <class F>
void line(pf::Point a, pf::Point b, F&& f) {
    const int dx = std::abs(b.x - a.x), sx = a.x < b.x ? 1 : -1;
    const int dy = -std::abs(b.y - a.y), sy = a.y < b.y ? 1 : -1;
    int err = dx + dy;
    while (true) {
        f(a);
        if (a == b) return;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            a.x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            a.y += sy;
        }
    }
}

enum class Drag { None, Wall, Mud, Erase, Start, Goal };

// Renders a finished search off screen: the README images, reproducibly.
int screenshot(int argc, char** argv, const sf::Font& font) {
    State s;
    const std::string_view scene = option(argc, argv, "--scene", "walls");
    s.seed = std::atoi(option(argc, argv, "--seed", "1"));
    s.generator = scene == "maze" ? app::Generator::Maze : scene == "terrain" ? app::Generator::Terrain : app::Generator::RandomWalls;
    s.tab = has_flag(argc, argv, "--grid-tab") ? 1 : 0;
    app::generate(s);
    const auto algorithm = parse_algorithm(option(argc, argv, "--algorithm", "bfs"));
    if (!algorithm) {
        std::fprintf(stderr, "unknown --algorithm (bfs, dfs, dijkstra, astar, greedy)\n");
        return EXIT_FAILURE;
    }
    s.algorithm = *algorithm;
    s.astar_weight = static_cast<float>(std::atof(option(argc, argv, "--weight", "1")));
    if (has_flag(argc, argv, "--compare")) app::compare_all(s);
    else app::run(s, *algorithm);
    app::skip_to_end(s);

    sf::RenderTexture texture;
    const sf::Vector2f size{1280, 840};
    if (!texture.create(1280, 840)) return EXIT_FAILURE;
    const app::Layout layout = app::layout_for(size, s.grid);
    app::draw_scene(texture, s, layout);
    app::draw_canvas_overlay(texture, s, layout, font);
    app::draw_panel(texture, s, font, size, app::PanelInput{{-1, -1}, false, false});
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

    sf::RenderWindow window(sf::VideoMode(1280, 840), "Pathfinding Visualizer");
    window.setVerticalSyncEnabled(true);

    State s;
    app::reset_grid(s);
    s.status = "Draw walls, then pick an algorithm";
    Drag drag = Drag::None;
    pf::Point last{0, 0};  // previous cell of the current stroke
    bool stroking = false;
    sf::Clock clock;

    while (window.isOpen()) {
        const sf::Vector2f size{static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)};
        const app::Layout layout = app::layout_for(size, s.grid);
        bool panel_pressed = false;

        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            if (e.type == sf::Event::Resized) {
                window.setView(sf::View(sf::FloatRect(0, 0, static_cast<float>(e.size.width),
                                                      static_cast<float>(e.size.height))));
            }
            if (e.type == sf::Event::KeyPressed) {
                const auto key = e.key.code;
                if (key >= sf::Keyboard::Num1 && key <= sf::Keyboard::Num5)
                    app::run(s, pf::kAlgorithms[static_cast<std::size_t>(key - sf::Keyboard::Num1)]);
                else if (key == sf::Keyboard::Space) app::run(s, s.algorithm);
                else if (key == sf::Keyboard::A) app::compare_all(s);
                else if (key == sf::Keyboard::P) s.paused = !s.paused;
                else if (key == sf::Keyboard::E) app::skip_to_end(s);
                else if (key == sf::Keyboard::C) app::clear_search(s);
            }
            if (e.type == sf::Event::MouseButtonPressed) {
                const sf::Vector2f at{static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y)};
                if (at.x >= size.x - app::kPanelWidth) {
                    panel_pressed = e.mouseButton.button == sf::Mouse::Left;  // the panel handles it
                    continue;
                }
                const auto cell = layout.cell_at(at, s.grid);
                if (!cell) continue;
                const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) ||
                                   sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
                if (e.mouseButton.button == sf::Mouse::Right) drag = Drag::Erase;
                else if (*cell == s.start) drag = Drag::Start;
                else if (*cell == s.goal) drag = Drag::Goal;
                else if (shift || s.tool == app::Tool::Mud) drag = Drag::Mud;
                else drag = s.tool == app::Tool::Erase ? Drag::Erase : Drag::Wall;
                stroking = false;
            }
            if (e.type == sf::Event::MouseButtonReleased) {
                drag = Drag::None;
                stroking = false;
            }
        }

        // Hover and drag follow the mouse every frame.
        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        const sf::Vector2f at{static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
        s.hover = s.dragging_slider >= 0 ? std::nullopt : layout.cell_at(at, s.grid);
        if (drag != Drag::None && s.hover) {
            const pf::Point p = *s.hover;
            if (drag == Drag::Start || drag == Drag::Goal) {
                const pf::Point other = drag == Drag::Start ? s.goal : s.start;
                if (s.grid.passable(p) && p != other) {
                    if (drag == Drag::Start) s.start = p;
                    else s.goal = p;
                    app::clear_search(s);
                }
            } else if (!stroking || last != p) {
                line(stroking ? last : p, p, [&](pf::Point q) {
                    if (q == s.start || q == s.goal) return;
                    s.grid.set_cost(q, drag == Drag::Wall  ? pf::Grid::kWall
                                       : drag == Drag::Mud ? static_cast<std::uint8_t>(s.mud_cost)
                                                           : pf::Grid::kOpen);
                });
                last = p;
                stroking = true;
                app::clear_search(s);
                s.comparison.clear();
            }
        }

        app::advance(s, clock.restart().asSeconds());
        const bool held = window.hasFocus() && sf::Mouse::isButtonPressed(sf::Mouse::Left);
        app::draw_scene(window, s, layout);
        app::draw_canvas_overlay(window, s, layout, font);
        app::draw_panel(window, s, font, size, app::PanelInput{at, panel_pressed, held});
        window.display();
    }
    return EXIT_SUCCESS;
}
