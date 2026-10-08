// Pathfinding visualiser: edit a grid, pick an algorithm, watch the recorded search replay.
//
//   pathfinder-app
//   pathfinder-app --screenshot out.png [--scene maze|walls|terrain|empty] [--algorithm astar]
//                  [--seed 7] [--size 61] [--compare] [--width 1440 --height 900]
#include <imgui-SFML.h>
#include <imgui.h>

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

bool flag(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], name) == 0) return true;
    return false;
}

std::string asset_dir() {
    for (const char* dir : {"assets", PATHFINDER_ASSETS}) {
        if (std::FILE* f = std::fopen((std::string(dir) + "/fonts/Inter-Regular.ttf").c_str(), "rb")) {
            std::fclose(f);
            return dir;
        }
    }
    return "assets";
}

std::optional<pf::Algorithm> parse_algorithm(std::string_view name) {
    if (name == "bfs") return pf::Algorithm::BFS;
    if (name == "dfs") return pf::Algorithm::DFS;
    if (name == "dijkstra") return pf::Algorithm::Dijkstra;
    if (name == "astar") return pf::Algorithm::AStar;
    if (name == "greedy") return pf::Algorithm::Greedy;
    return std::nullopt;
}

// Every cell on the straight line between two cells (Bresenham), so a fast drag paints a
// continuous stroke instead of a dotted one.
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

enum class Drag { None, Paint, Erase, Start, Goal };

void paint_cell(State& s, Drag drag, pf::Point p) {
    if (p == s.start || p == s.goal) return;
    if (drag == Drag::Erase) s.grid.set_cost(p, pf::Grid::kOpen);
    else if (s.tool == app::Tool::Wall) s.grid.set_wall(p);
    else if (s.tool == app::Tool::Mud) s.grid.set_cost(p, static_cast<std::uint8_t>(s.mud_cost));
    else s.grid.set_cost(p, pf::Grid::kOpen);
}

// Renders a finished search, panel included, off screen: reproducible README images.
int screenshot(int argc, char** argv) {
    const sf::Vector2u size{static_cast<unsigned>(std::atoi(option(argc, argv, "--width", "1440"))),
                            static_cast<unsigned>(std::atoi(option(argc, argv, "--height", "900")))};
    State s;
    const std::string_view scene = option(argc, argv, "--scene", "maze");
    s.settings.generator = scene == "walls"     ? app::Generator::RandomWalls
                           : scene == "terrain" ? app::Generator::Terrain
                           : scene == "empty"   ? app::Generator::Empty
                                                : app::Generator::Maze;
    s.settings.seed = std::atoi(option(argc, argv, "--seed", "7"));
    s.settings.cols = std::atoi(option(argc, argv, "--size", "61")) | 1;
    s.settings.rows = (s.settings.cols * 2 / 3) | 1;
    app::generate(s);
    const auto algorithm = parse_algorithm(option(argc, argv, "--algorithm", "astar"));
    if (!algorithm) {
        std::fprintf(stderr, "unknown --algorithm (bfs, dfs, dijkstra, astar, greedy)\n");
        return EXIT_FAILURE;
    }
    s.algorithm = *algorithm;
    if (flag(argc, argv, "--compare")) app::compare_all(s);
    else app::run(s);
    app::skip_to_end(s);

    sf::RenderTexture texture;
    sf::ContextSettings settings;
    settings.antialiasingLevel = 4;
    if (!texture.create(size.x, size.y, settings)) return EXIT_FAILURE;
    const sf::Vector2f view{static_cast<float>(size.x), static_cast<float>(size.y)};
    sf::Window hidden(sf::VideoMode(64, 64), "", sf::Style::None);  // ImGui-SFML wants a window
    hidden.setVisible(false);
    if (!ImGui::SFML::Init(hidden, view, false)) return EXIT_FAILURE;
    if (!texture.setActive(true)) return EXIT_FAILURE;
    if (!app::load_fonts(asset_dir()) || !ImGui::SFML::UpdateFontTexture()) return EXIT_FAILURE;
    app::apply_theme();
    ImGui::GetIO().IniFilename = nullptr;
    for (int frame = 0; frame < 3; ++frame) {  // a few frames so tables settle their widths
        ImGui::SFML::Update(sf::Vector2i{-100, -100}, view, sf::milliseconds(16));
        app::draw_scene(texture, s, app::layout_for(view, s.grid));
        app::draw_panel(s, view);
        app::draw_overlay(s, app::layout_for(view, s.grid));
        ImGui::SFML::Render(texture);
    }
    texture.display();
    ImGui::SFML::Shutdown();
    const char* out = option(argc, argv, "--screenshot", "screenshot.png");
    if (!texture.getTexture().copyToImage().saveToFile(out)) return EXIT_FAILURE;
    std::printf("wrote %s\n", out);
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
    if (std::string_view(option(argc, argv, "--screenshot", "")) != "") return screenshot(argc, argv);

    sf::ContextSettings settings;
    settings.antialiasingLevel = 4;
    sf::RenderWindow window(sf::VideoMode(1440, 900), "Pathfinding Visualiser", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    if (!ImGui::SFML::Init(window, false)) return EXIT_FAILURE;
    if (!app::load_fonts(asset_dir()) || !ImGui::SFML::UpdateFontTexture()) {
        std::fprintf(stderr, "could not load the fonts in assets/fonts\n");
        return EXIT_FAILURE;
    }
    app::apply_theme();
    ImGui::GetIO().IniFilename = nullptr;

    State s;
    app::generate(s);
    Drag drag = Drag::None;
    pf::Point last{0, 0};  // the previous cell of the current stroke
    bool stroking = false;
    sf::Clock clock;

    while (window.isOpen()) {
        const sf::Vector2f size{static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)};
        const app::Layout layout = app::layout_for(size, s.grid);
        sf::Event e;
        while (window.pollEvent(e)) {
            ImGui::SFML::ProcessEvent(window, e);
            if (e.type == sf::Event::Closed) window.close();
            if (e.type == sf::Event::Resized) {
                window.setView(sf::View(sf::FloatRect(0, 0, static_cast<float>(e.size.width),
                                                      static_cast<float>(e.size.height))));
            }

            const ImGuiIO& io = ImGui::GetIO();
            if (e.type == sf::Event::KeyPressed && !io.WantCaptureKeyboard) {
                const auto key = e.key.code;
                if (key >= sf::Keyboard::Num1 && key <= sf::Keyboard::Num5)
                    s.algorithm = pf::kAlgorithms[static_cast<std::size_t>(key - sf::Keyboard::Num1)];
                else if (key == sf::Keyboard::Space) app::run(s);
                else if (key == sf::Keyboard::A) app::compare_all(s);
                else if (key == sf::Keyboard::P) s.paused = !s.paused;
                else if (key == sf::Keyboard::E) app::skip_to_end(s);
                else if (key == sf::Keyboard::C) app::clear_search(s);
            }
            if (e.type == sf::Event::MouseButtonPressed && !io.WantCaptureMouse) {
                const sf::Vector2f at{static_cast<float>(e.mouseButton.x), static_cast<float>(e.mouseButton.y)};
                const auto cell = layout.cell_at(at, s.grid);
                if (!cell) continue;
                if (e.mouseButton.button == sf::Mouse::Right) drag = Drag::Erase;
                else if (*cell == s.start) drag = Drag::Start;
                else if (*cell == s.goal) drag = Drag::Goal;
                else drag = Drag::Paint;
                stroking = false;
            }
            if (e.type == sf::Event::MouseButtonReleased) {
                drag = Drag::None;
                stroking = false;
            }
        }

        // Hover and drag, from the current mouse position.
        const sf::Vector2i mouse = sf::Mouse::getPosition(window);
        const sf::Vector2f at{static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
        s.hover = ImGui::GetIO().WantCaptureMouse ? std::nullopt : layout.cell_at(at, s.grid);
        if (drag != Drag::None && s.hover) {
            const pf::Point p = *s.hover;
            if (drag == Drag::Start || drag == Drag::Goal) {
                const pf::Point other = drag == Drag::Start ? s.goal : s.start;
                const pf::Point current = drag == Drag::Start ? s.start : s.goal;
                if (s.grid.passable(p) && p != other && p != current) {
                    if (drag == Drag::Start) s.start = p;
                    else s.goal = p;
                    app::clear_search(s);
                }
            } else if (!stroking || last != p) {
                line(stroking ? last : p, p, [&](pf::Point q) { paint_cell(s, drag, q); });
                last = p;
                stroking = true;
                app::clear_search(s);
                s.comparison.clear();
            }
        }

        const sf::Time dt = clock.restart();
        app::advance(s, dt.asSeconds());
        ImGui::SFML::Update(window, dt);
        app::draw_panel(s, size);
        app::draw_overlay(s, app::layout_for(size, s.grid));
        app::draw_scene(window, s, app::layout_for(size, s.grid));
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();
    return EXIT_SUCCESS;
}
