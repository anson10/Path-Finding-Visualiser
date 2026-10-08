// The side panel: a Search tab (run, compare, replay) and a Grid tab (generate, draw).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "app.hpp"
#include "ui.hpp"

namespace app {
namespace {

using ui::colors::Dim;
using ui::colors::Good;
using ui::colors::Text;
using ui::colors::Warning;

std::string thousands(long long v) {
    std::string s = std::to_string(v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
    return s;
}

std::string fixed(double v, int digits) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", digits, v);
    return buf;
}

const char* hint(pf::Algorithm a) {
    switch (a) {
        case pf::Algorithm::BFS: return "Fewest steps; ignores mud";
        case pf::Algorithm::DFS: return "Dives deep first; any path";
        case pf::Algorithm::Dijkstra: return "Cheapest path; spreads evenly";
        case pf::Algorithm::AStar: return "Cheapest path, aimed at the end";
        case pf::Algorithm::Greedy: return "Rushes at the end; any path";
    }
    return "";
}

void results(ui::Ui& u, State& s) {
    u.heading("RESULTS");
    if (!s.comparison.empty()) {
        u.text("cost", 15, Dim, 120);
        u.text("visited", 15, Dim, 190);
        u.gap(22);
        long long best = -1;
        for (const auto& c : s.comparison)
            if (c.result.found && (best < 0 || c.result.cost < best)) best = c.result.cost;
        for (const auto& c : s.comparison) {
            const bool sel = c.algorithm == s.algorithm;
            std::string label(pf::name(c.algorithm));
            if (c.algorithm == pf::Algorithm::AStar && s.astar_weight > 1.05f) label += " w" + fixed(s.astar_weight, 1);
            u.text(label, 17, sel ? ui::colors::Accent : Text);
            u.text(c.result.found ? thousands(c.result.cost) : "-", 17,
                   !c.result.found ? Dim : c.result.cost == best ? Good : Warning, 120);
            u.text(thousands(static_cast<long long>(c.result.nodes_expanded())), 17, Text, 190);
            u.gap(23);
        }
        u.text("green: cheapest   amber: costlier", 14, Dim);
        u.gap(24);
        return;
    }
    u.line("Status:   " + s.status);
    if (s.phase == Phase::Idle) return;
    if (s.result.found)
        u.line("Cost:      " + thousands(s.result.cost) + "  (" + thousands(static_cast<long long>(s.result.path.size())) + " cells)");
    u.line("Visited:  " + thousands(static_cast<long long>(s.result.nodes_expanded())) + " cells");
    if (s.algorithm == pf::Algorithm::AStar && s.astar_weight > 1.05f)
        u.line("Bound:    at most " + fixed(s.astar_weight, 1) + "x the cheapest", Dim, 16);
    u.line("Time:      " + fixed(s.search_ms, 3) + " ms");
}

void search_tab(ui::Ui& u, State& s) {
    u.heading("ALGORITHM   (click to run)");
    for (std::size_t i = 0; i < pf::kAlgorithms.size(); ++i) {
        const pf::Algorithm a = pf::kAlgorithms[i];
        const std::string label = std::to_string(i + 1) + "   " + std::string(pf::name(a));
        if (u.button(label, a == s.algorithm && s.phase != Phase::Idle, 0, 31)) run(s, a);
        u.tooltip(hint(a));
    }
    const float before = s.astar_weight;
    u.slider("A* weight", s.astar_weight, 1.0f, 6.0f, s.astar_weight < 1.05f ? std::string("1  (optimal)") : fixed(s.astar_weight, 1));
    s.astar_weight = std::round(s.astar_weight * 10.0f) / 10.0f;
    u.tooltip("Above 1: weighted A*. Faster; path at most w times the cheapest");
    if (s.astar_weight != before && s.algorithm == pf::Algorithm::AStar && s.phase != Phase::Idle) run(s, s.algorithm);
    if (u.button("Compare All", false, 0, 31)) compare_all(s);
    u.tooltip("Run all five on this grid  (A)");

    results(u, s);

    u.heading("REPLAY");
    u.slider("Speed", s.speed, 25.0f, 50000.0f, thousands(static_cast<long long>(s.speed)) + " cells/s", true);
    switch (u.button_row({s.paused ? "Resume" : "Pause", "Skip", "Clear"})) {
        case 0: s.paused = !s.paused; break;
        case 1: skip_to_end(s); break;
        case 2: clear_search(s); break;
        default: break;
    }
    u.toggle("Shade by visit order", s.wavefront);
    u.tooltip("Dark blue first, light blue last");
}

void grid_tab(ui::Ui& u, State& s) {
    u.heading("GENERATE");
    const int picked = u.button_row({"Empty", "Random", "Maze", "Terrain"}, static_cast<int>(s.generator), 34, 15);
    if (picked >= 0) {
        s.generator = static_cast<Generator>(picked);
        if (s.generator != Generator::Empty) ++s.seed;  // a fresh layout each click
        generate(s);
        s.status = "Grid generated";
    }
    if (u.slider("Size", s.size, 11, 101, 2)) generate(s);
    u.tooltip("Cells per side");
    if (s.generator == Generator::RandomWalls || s.generator == Generator::Terrain) {
        if (u.slider("Walls", s.density, 0.0f, 0.5f, fixed(100.0 * s.density, 0) + "%")) generate(s);
    }
    if (u.slider("Seed", s.seed, 1, 999)) generate(s);
    if (u.button("New seed", false, 0, 32, 16)) {
        s.seed = 1 + (s.seed * 7919 + 13) % 999;
        generate(s);
    }
    u.gap(6);

    u.heading("DRAW");
    int tool = static_cast<int>(s.tool);
    if (u.segmented({"Wall", "Mud", "Erase"}, tool)) s.tool = static_cast<Tool>(tool);
    if (s.tool == Tool::Mud) u.slider("Mud cost", s.mud_cost, 2, 9);
    u.line("Drag on the grid to draw.", Dim, 15);
    u.line("Right-drag erases; drag the green", Dim, 15);
    u.line("or red cell to move start or end.", Dim, 15);
}

}  // namespace

void draw_panel(sf::RenderTarget& target, State& s, const sf::Font& font, sf::Vector2f window, const PanelInput& in) {
    const float left = window.x - kPanelWidth;
    sf::RectangleShape background({kPanelWidth, window.y});
    background.setPosition(left, 0);
    background.setFillColor(ui::colors::Panel);
    target.draw(background);

    const ui::Input input{in.mouse, in.pressed, in.down};
    ui::Ui u(target, font, input, s.dragging_slider, {left + 20, 16}, kPanelWidth - 40);
    u.text("Pathfinding Visualizer", 22, Text);
    u.gap(38);
    u.tabs({"Search", "Grid"}, s.tab);
    if (s.tab == 0) search_tab(u, s);
    else grid_tab(u, s);

    // Reset Grid stays at the bottom, as before (or below the content in a short window).
    u.set_y(std::max(u.y() + 8, window.y - 56));
    if (u.button("Reset Grid", false, 0, 40, 20)) reset_grid(s);
    u.finish();
}

}  // namespace app
