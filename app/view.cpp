#include <chrono>
#include <cstdio>

#include "app.hpp"

namespace app {
namespace {

namespace color {
const sf::Color kBackground{32, 34, 38};
const sf::Color kOpen{245, 245, 240};
const sf::Color kWall{40, 42, 48};
const sf::Color kExpanded{120, 190, 235};
const sf::Color kPath{255, 205, 60};
const sf::Color kStart{40, 170, 90};
const sf::Color kGoal{215, 60, 60};
const sf::Color kPanel{48, 50, 56};
const sf::Color kButton{70, 73, 80};
const sf::Color kSelected{30, 120, 200};
const sf::Color kText{235, 235, 235};
const sf::Color kMuted{170, 172, 178};
}  // namespace color

// Open cells shade from white (cost 1) towards brown (expensive terrain).
sf::Color terrain(std::uint8_t cost) {
    const float t = static_cast<float>(cost - 1) / 8.0f;
    auto mix = [t](int a, int b) { return static_cast<sf::Uint8>(static_cast<float>(a) + t * static_cast<float>(b - a)); };
    return {mix(245, 160), mix(245, 120), mix(240, 80)};
}

sf::Color blend(sf::Color a, sf::Color b, float t) {
    auto mix = [t](sf::Uint8 x, sf::Uint8 y) {
        return static_cast<sf::Uint8>(static_cast<float>(x) + t * static_cast<float>(y - x));
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

void quad(sf::VertexArray& v, std::size_t cell, float x, float y, float size, sf::Color c) {
    sf::Vertex* q = &v[cell * 4];
    q[0] = {{x, y}, c};
    q[1] = {{x + size, y}, c};
    q[2] = {{x + size, y + size}, c};
    q[3] = {{x, y + size}, c};
}

void text(sf::RenderTarget& target, const sf::Font& font, const std::string& s, float x, float y,
          unsigned size, sf::Color c) {
    sf::Text t(s, font, size);
    t.setPosition(x, y);
    t.setFillColor(c);
    target.draw(t);
}

}  // namespace

std::vector<Button> layout_buttons() {
    std::vector<Button> buttons;
    const float x = kGridWidth + 20.0f;
    const float w = kPanelWidth - 40.0f;
    float y = 48.0f;
    for (pf::Algorithm a : pf::kAlgorithms) {
        buttons.push_back({{x, y, w, 30.0f}, std::string(pf::name(a)), a, std::nullopt});
        y += 36.0f;
    }
    y += 34.0f;
    const std::pair<Action, const char*> actions[] = {
        {Action::Run, "Run  (Space)"},          {Action::Maze, "Maze  (M)"},
        {Action::RandomWalls, "Random walls  (R)"}, {Action::Terrain, "Weighted terrain  (T)"},
        {Action::ClearPath, "Clear search  (C)"}, {Action::ClearAll, "Clear all  (X)"},
    };
    for (const auto& [action, label] : actions) {
        buttons.push_back({{x, y, w, 30.0f}, label, std::nullopt, action});
        y += 36.0f;
    }
    return buttons;
}

void run_search(State& state) {
    const auto t0 = std::chrono::steady_clock::now();
    state.result = pf::search(state.grid, state.start, state.goal, state.algorithm);
    state.search_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    state.shown_expanded = 0;
    state.shown_path = 0;
    state.phase = Phase::Expanding;
    state.status = "Searching...";
}

void step_replay(State& state, bool instant) {
    const auto& r = state.result;
    if (state.phase == Phase::Expanding) {
        state.shown_expanded = instant ? r.expanded.size()
                                       : std::min(r.expanded.size(), state.shown_expanded + state.speed);
        if (state.shown_expanded == r.expanded.size()) state.phase = Phase::Tracing;
    }
    if (state.phase == Phase::Tracing) {
        state.shown_path = instant ? r.path.size() : std::min(r.path.size(), state.shown_path + 1);
        if (state.shown_path == r.path.size()) {
            state.phase = Phase::Done;
            state.status = r.found ? "Path found." : "No path: the goal is walled off.";
        }
    }
}

void clear_search(State& state) {
    state.result = {};
    state.search_ms = 0.0;
    state.shown_expanded = state.shown_path = 0;
    state.phase = Phase::Editing;
}

void draw(sf::RenderTarget& target, const State& state, const std::vector<Button>& buttons,
          const sf::Font& font) {
    target.clear(color::kBackground);

    // Cell colours: terrain, then the replayed expansion, then the path, then start/goal.
    const pf::Grid& g = state.grid;
    std::vector<sf::Color> fill(static_cast<std::size_t>(g.size()));
    for (int i = 0; i < g.size(); ++i) {
        const std::uint8_t c = g.cost(g.point(i));
        fill[i] = c == pf::Grid::kWall ? color::kWall : terrain(c);
    }
    for (std::size_t k = 0; k < state.shown_expanded; ++k) {
        const auto i = static_cast<std::size_t>(g.index(state.result.expanded[k]));
        fill[i] = blend(fill[i], color::kExpanded, 0.5f);
    }
    for (std::size_t k = 0; k < state.shown_path; ++k)
        fill[g.index(state.result.path[k])] = color::kPath;
    fill[g.index(state.start)] = color::kStart;
    fill[g.index(state.goal)] = color::kGoal;

    sf::VertexArray cells(sf::Quads, fill.size() * 4);
    for (int i = 0; i < g.size(); ++i) {
        const pf::Point p = g.point(i);
        quad(cells, i, static_cast<float>(p.x * kCell) + 1, static_cast<float>(p.y * kCell) + 1,
             kCell - 1.0f, fill[i]);
    }
    target.draw(cells);
    auto at = [](int cell) { return static_cast<float>(cell * kCell); };
    text(target, font, "S", at(state.start.x) + 5, at(state.start.y) + 1, 15, color::kText);
    text(target, font, "G", at(state.goal.x) + 4, at(state.goal.y) + 1, 15, color::kText);

    // Side panel.
    sf::RectangleShape panel({static_cast<float>(kPanelWidth), static_cast<float>(kHeight)});
    panel.setPosition(kGridWidth, 0);
    panel.setFillColor(color::kPanel);
    target.draw(panel);
    const float x = kGridWidth + 20.0f;
    text(target, font, "Algorithm  (1-5)", x, 16, 18, color::kText);
    for (const Button& b : buttons) {
        sf::RectangleShape rect({b.bounds.width, b.bounds.height});
        rect.setPosition(b.bounds.left, b.bounds.top);
        rect.setFillColor(b.algorithm == state.algorithm ? color::kSelected : color::kButton);
        target.draw(rect);
        text(target, font, b.label, b.bounds.left + 10, b.bounds.top + 5, 16, color::kText);
    }
    char line[96];
    std::snprintf(line, sizeof line, "Actions   (replay %d/frame, +/-)", state.speed);
    text(target, font, line, x, buttons[pf::kAlgorithms.size()].bounds.top - 30, 18, color::kText);

    const auto& r = state.result;
    const float y = buttons.back().bounds.top + 50;
    text(target, font, "Result", x, y, 18, color::kText);
    if (state.phase == Phase::Editing) {
        text(target, font, "No search yet", x, y + 30, 15, color::kMuted);
    } else {
        std::snprintf(line, sizeof line, "%s   %s", std::string(pf::name(state.algorithm)).c_str(),
                      r.found ? "path found" : "no path");
        text(target, font, line, x, y + 30, 15, color::kText);
        std::snprintf(line, sizeof line, "Path cost   %lld   (%zu cells)", r.cost, r.path.size());
        text(target, font, line, x, y + 52, 15, color::kText);
        std::snprintf(line, sizeof line, "Expanded    %zu cells", r.nodes_expanded());
        text(target, font, line, x, y + 74, 15, color::kText);
        std::snprintf(line, sizeof line, "Search      %.3f ms", state.search_ms);
        text(target, font, line, x, y + 96, 15, color::kText);
        text(target, font, pf::optimal_on(state.algorithm, false) ? "Guaranteed least cost"
                                                                  : "Not guaranteed least cost",
             x, y + 118, 14, color::kMuted);
    }
    // Status, wrapped to the panel width.
    std::string row;
    float sy = y + 146;
    for (std::size_t pos = 0; pos <= state.status.size();) {
        const std::size_t end = std::min(state.status.find(' ', pos), state.status.size());
        const std::string word = state.status.substr(pos, end - pos);
        if (!row.empty() && row.size() + word.size() + 1 > 34) {
            text(target, font, row, x, sy, 14, color::kMuted);
            sy += 18;
            row.clear();
        }
        row += (row.empty() ? "" : " ") + word;
        pos = end + 1;
    }
    if (!row.empty()) text(target, font, row, x, sy, 14, color::kMuted);
}

}  // namespace app
