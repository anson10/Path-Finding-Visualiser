// The side panel, laid out like the original: algorithm buttons (click one to run it), the
// maze buttons, results, and Reset Grid pinned to the bottom.
#include <algorithm>
#include <cstdio>
#include <string>

#include "app.hpp"

namespace app {
namespace {

namespace colors {
const sf::Color Panel(50, 50, 50);
const sf::Color Button(70, 70, 70);
const sf::Color ButtonHover(88, 88, 88);
const sf::Color ButtonActive(0, 140, 210);  // selected algorithm
const sf::Color Text(255, 255, 255);
const sf::Color Dim(175, 175, 175);
const sf::Color Good(120, 220, 120);
const sf::Color Warning(255, 180, 0);
}  // namespace colors

constexpr float kButtonHeight = 40.0f;
constexpr float kSpacing = 10.0f;

std::string thousands(long long v) {
    std::string s = std::to_string(v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
    return s;
}

// Immediate-mode drawing: each call draws one element at the cursor and moves it down.
struct Panel {
    sf::RenderTarget& target;
    const sf::Font& font;
    sf::Vector2f mouse;
    bool clicked;
    float x;
    float y;
    float width;

    void text(const std::string& s, unsigned size, sf::Color colour, float dx = 0, float dy = 0) {
        sf::Text t(s, font, size);
        t.setPosition(x + dx, y + dy);
        t.setFillColor(colour);
        target.draw(t);
    }

    void title(const std::string& s) {
        text(s, 22, colors::Text);
        y += 35;
    }

    bool button(const std::string& label, bool active = false, unsigned size = 20, float w = 0, float h = kButtonHeight) {
        const sf::FloatRect bounds(x, y, w > 0 ? w : width, h);
        const bool over = bounds.contains(mouse);
        sf::RectangleShape rect({bounds.width, bounds.height});
        rect.setPosition(bounds.left, bounds.top);
        rect.setFillColor(active ? colors::ButtonActive : over ? colors::ButtonHover : colors::Button);
        target.draw(rect);
        text(label, size, colors::Text, 10, (h - static_cast<float>(size)) / 2 - 3);
        y += h + kSpacing;
        return over && clicked;
    }
};

}  // namespace

void draw_panel(sf::RenderTarget& target, State& s, const sf::Font& font, sf::Vector2f window,
                sf::Vector2f mouse, bool clicked) {
    const float left = window.x - kPanelWidth;
    sf::RectangleShape background({kPanelWidth, window.y});
    background.setPosition(left, 0);
    background.setFillColor(colors::Panel);
    target.draw(background);

    Panel p{target, font, mouse, clicked, left + 20, 20, kPanelWidth - 40};

    // Algorithms: clicking one runs it, as before.
    p.title("Pathfinding Algorithms");
    for (pf::Algorithm a : pf::kAlgorithms) {
        if (p.button(std::string(pf::name(a)), a == s.algorithm && s.phase != Phase::Idle, 20, 0, 36)) run(s, a);
    }

    // Grids
    p.y += 5;
    if (p.button("Generate Random Maze", false, 17, 0, 34)) {
        generate(s, Generator::RandomWalls);
        s.status = "Random maze generated";
    }
    if (p.button("Perfect Maze", false, 17, 0, 34)) {
        generate(s, Generator::Maze);
        s.status = "Perfect maze generated";
    }
    if (p.button("Weighted Terrain", false, 17, 0, 34)) {
        generate(s, Generator::Terrain);
        s.status = "Browner cells cost more";
    }
    if (p.button("Compare All", false, 17, 0, 34)) compare_all(s);

    // Results
    p.y += 5;
    p.title("Results");
    char line[96];
    if (!s.comparison.empty()) {
        p.text("cost", 15, colors::Dim, 110);
        p.text("visited", 15, colors::Dim, 175);
        p.y += 22;
        long long best = -1;
        for (const auto& c : s.comparison)
            if (c.result.found && (best < 0 || c.result.cost < best)) best = c.result.cost;
        for (const auto& c : s.comparison) {
            const bool selected = c.algorithm == s.algorithm;
            p.text(std::string(pf::name(c.algorithm)), 17, selected ? colors::ButtonActive : colors::Text);
            p.text(c.result.found ? thousands(c.result.cost) : "-", 17,
                   !c.result.found ? colors::Dim : c.result.cost == best ? colors::Good : colors::Warning, 110);
            p.text(thousands(static_cast<long long>(c.result.nodes_expanded())), 17, colors::Text, 175);
            p.y += 22;
        }
        p.y += 4;
    } else {
        std::snprintf(line, sizeof line, "Time:   %.3f ms", s.search_ms);
        p.text(line, 18, colors::Text);
        p.y += 24;
        p.text("Status: " + s.status, 18, colors::Text);
        p.y += 24;
        if (s.phase != Phase::Idle && s.result.found) {
            p.text("Cost:   " + thousands(s.result.cost) + "  (" + thousands(static_cast<long long>(s.result.path.size())) + " cells)", 18, colors::Text);
            p.y += 24;
        }
        if (s.phase != Phase::Idle) {
            p.text("Visited: " + thousands(static_cast<long long>(s.result.nodes_expanded())) + " cells", 18, colors::Text);
            p.y += 24;
        }
    }

    // Replay speed: small - and + buttons around the number
    p.y += 4;
    const float row = p.y;
    if (p.button("-", false, 20, 34, 30)) s.speed = std::max(25.0f, s.speed / 2);
    p.y = row;
    std::snprintf(line, sizeof line, "%s%.0f cells/s", s.paused ? "paused  " : "", s.speed);
    p.text(line, 16, colors::Dim, 46, 5);
    p.x += p.width - 34;
    if (p.button("+", false, 20, 34, 30)) s.speed = std::min(50000.0f, s.speed * 2);
    p.x -= p.width - 34;

    // Reset Grid and the controls, pinned to the bottom (or below the results if the
    // window is too short for both).
    p.y = std::max(p.y + 6, window.y - kButtonHeight - 20 - 44);
    p.text("Left: wall   Shift: mud   Right: erase", 14, colors::Dim);
    p.y += 18;
    p.text("Space: run again   P: pause   E: skip", 14, colors::Dim);
    p.y = std::max(p.y + 24, window.y - kButtonHeight - 20);
    if (p.button("Reset Grid")) reset_grid(s);
}

}  // namespace app
