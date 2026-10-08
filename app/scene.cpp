// The grid, in the original palette: white cells on dark grid lines, black walls, a green
// start, a red end, light-blue visited cells and a yellow path. Mud cells shade towards brown
// with their cost.
#include <algorithm>
#include <cmath>

#include "app.hpp"

namespace app {
namespace {

namespace colors {
const sf::Color Background(40, 40, 40);
const sf::Color GridLine(50, 50, 50);
const sf::Color Empty(255, 255, 255);
const sf::Color Wall(30, 30, 30);
const sf::Color Start(0, 200, 0);
const sf::Color End(200, 0, 0);
const sf::Color Visited(100, 200, 255);
const sf::Color JustVisited(190, 232, 255);  // a cell the moment it is visited
const sf::Color Path(255, 255, 100);
const sf::Color Mud(150, 105, 60);           // the most expensive terrain
const sf::Color Hover(0, 140, 210);
}  // namespace colors

sf::Color mix(sf::Color a, sf::Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto m = [t](sf::Uint8 x, sf::Uint8 y) {
        return static_cast<sf::Uint8>(std::lround(static_cast<float>(x) + t * (static_cast<float>(y) - static_cast<float>(x))));
    };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

// 0 for cost 1, 1 for cost 9
float weight(std::uint8_t cost) { return static_cast<float>(cost - 1) / 8.0f; }

}  // namespace

std::optional<pf::Point> Layout::cell_at(sf::Vector2f pixel, const pf::Grid& grid) const {
    if (cell <= 0.0f) return std::nullopt;
    const pf::Point p{static_cast<int>(std::floor((pixel.x - origin.x) / cell)),
                      static_cast<int>(std::floor((pixel.y - origin.y) / cell))};
    return grid.in_bounds(p) ? std::optional(p) : std::nullopt;
}

Layout layout_for(sf::Vector2f window, const pf::Grid& grid) {
    const float margin = 16.0f;
    const float w = std::max(window.x - kPanelWidth - 2 * margin, 40.0f);
    const float h = std::max(window.y - 2 * margin, 40.0f);
    const float cell = std::max(2.0f, std::floor(std::min(w / static_cast<float>(grid.width()),
                                                          h / static_cast<float>(grid.height()))));
    const float gw = cell * static_cast<float>(grid.width());
    const float gh = cell * static_cast<float>(grid.height());
    return {{std::floor(margin + (w - gw) / 2), std::floor(margin + (h - gh) / 2)}, cell};
}

void draw_scene(sf::RenderTarget& target, const State& s, const Layout& L) {
    target.clear(colors::Background);
    const pf::Grid& g = s.grid;

    // Grid lines are the gaps between cells, on a GridLine-coloured board.
    sf::RectangleShape board({L.cell * static_cast<float>(g.width()) + 1, L.cell * static_cast<float>(g.height()) + 1});
    board.setPosition(L.origin);
    board.setFillColor(colors::GridLine);
    target.draw(board);

    std::vector<sf::Color> fill(static_cast<std::size_t>(g.size()));
    for (int i = 0; i < g.size(); ++i) {
        const std::uint8_t c = g.cost(g.point(i));
        fill[i] = c == pf::Grid::kWall ? colors::Wall : mix(colors::Empty, colors::Mud, weight(c));
    }
    const auto& expanded = s.result.expanded;
    for (std::size_t k = 0; k < static_cast<std::size_t>(s.revealed); ++k) {
        const int i = g.index(expanded[k]);
        // muddy cells keep some brown under the blue, so the cost still shows
        const sf::Color visited = mix(colors::Visited, colors::Mud, 0.6f * weight(g.cost(expanded[k])));
        const float age = k < s.revealed_at.size() ? static_cast<float>(s.clock) - s.revealed_at[k] : 1.0f;
        fill[i] = mix(colors::JustVisited, visited, age / 0.25f);
    }
    const auto drawn = std::min(s.result.path.size(), static_cast<std::size_t>(std::ceil(s.path_shown)));
    // the path is traced back from the end, the way it is reconstructed
    for (std::size_t k = 0; k < drawn; ++k) fill[g.index(s.result.path[s.result.path.size() - 1 - k])] = colors::Path;
    fill[g.index(s.start)] = colors::Start;
    fill[g.index(s.goal)] = colors::End;

    const float gap = L.cell >= 6 ? 1.0f : 0.0f;
    sf::VertexArray cells(sf::Quads, static_cast<std::size_t>(g.size()) * 4);
    for (int i = 0; i < g.size(); ++i) {
        const pf::Point p = g.point(i);
        const float x = L.origin.x + static_cast<float>(p.x) * L.cell + gap;
        const float y = L.origin.y + static_cast<float>(p.y) * L.cell + gap;
        const float size = L.cell - gap;
        sf::Vertex* q = &cells[static_cast<std::size_t>(i) * 4];
        q[0] = {{x, y}, fill[i]};
        q[1] = {{x + size, y}, fill[i]};
        q[2] = {{x + size, y + size}, fill[i]};
        q[3] = {{x, y + size}, fill[i]};
    }
    target.draw(cells);

    if (s.hover) {
        sf::RectangleShape h({L.cell - 2, L.cell - 2});
        h.setPosition(L.origin.x + static_cast<float>(s.hover->x) * L.cell + 1.5f,
                      L.origin.y + static_cast<float>(s.hover->y) * L.cell + 1.5f);
        h.setFillColor(sf::Color::Transparent);
        h.setOutlineThickness(1.5f);
        h.setOutlineColor(colors::Hover);
        target.draw(h);
    }
}

}  // namespace app
