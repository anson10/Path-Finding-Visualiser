// The grid: cells as one vertex array, the expansion coloured by when each cell was reached
// (a wavefront), fresh cells fading in, the path as a thick rounded line, round markers.
#include <algorithm>
#include <cmath>

#include "app.hpp"

namespace app {
namespace {

namespace palette {
const sf::Color kBackground{11, 15, 25};
const sf::Color kBoard{17, 24, 39};
const sf::Color kOpen{36, 46, 66};
const sf::Color kMud{146, 84, 30};        // the most expensive terrain
const sf::Color kWaveEarly{79, 70, 229};  // first cells expanded (indigo)
const sf::Color kWaveLate{34, 211, 238};  // last cells expanded (cyan)
const sf::Color kFresh{224, 242, 254};    // a cell the moment it is expanded
const sf::Color kPath{251, 191, 36};
const sf::Color kStart{16, 185, 129};
const sf::Color kGoal{244, 63, 94};
}  // namespace palette

sf::Color mix(sf::Color a, sf::Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto m = [t](sf::Uint8 x, sf::Uint8 y) {
        return static_cast<sf::Uint8>(std::lround(static_cast<float>(x) + t * (static_cast<float>(y) - static_cast<float>(x))));
    };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

sf::Color terrain(std::uint8_t cost) {
    return cost <= 1 ? palette::kOpen : mix(palette::kOpen, palette::kMud, static_cast<float>(cost - 1) / 8.0f);
}

void add_quad(sf::VertexArray& v, sf::Vector2f pos, float size, sf::Color c) {
    v.append({pos, c});
    v.append({{pos.x + size, pos.y}, c});
    v.append({{pos.x + size, pos.y + size}, c});
    v.append({{pos.x, pos.y + size}, c});
}

void marker(sf::RenderTarget& target, sf::Vector2f centre, float radius, sf::Color fill) {
    sf::CircleShape ring(radius + radius * 0.28f, 40);
    ring.setOrigin(ring.getRadius(), ring.getRadius());
    ring.setPosition(centre);
    ring.setFillColor({fill.r, fill.g, fill.b, 70});
    target.draw(ring);
    sf::CircleShape dot(radius, 40);
    dot.setOrigin(radius, radius);
    dot.setPosition(centre);
    dot.setFillColor(fill);
    dot.setOutlineThickness(std::max(1.0f, radius * 0.18f));
    dot.setOutlineColor({255, 255, 255, 230});
    target.draw(dot);
}

}  // namespace

std::optional<pf::Point> Layout::cell_at(sf::Vector2f pixel, const pf::Grid& grid) const {
    if (cell <= 0.0f) return std::nullopt;
    const pf::Point p{static_cast<int>(std::floor((pixel.x - origin.x) / cell)),
                      static_cast<int>(std::floor((pixel.y - origin.y) / cell))};
    return grid.in_bounds(p) ? std::optional(p) : std::nullopt;
}

Layout layout_for(sf::Vector2f window, const pf::Grid& grid) {
    const float margin = 28.0f;  // left and right
    const float band = 52.0f;    // top (hover info) and bottom (legend)
    const float w = std::max(window.x - kPanelWidth - 2 * margin, 50.0f);
    const float h = std::max(window.y - 2 * band, 50.0f);
    const float cell = std::floor(std::min(w / static_cast<float>(grid.width()), h / static_cast<float>(grid.height())));
    const float gw = cell * static_cast<float>(grid.width());
    const float gh = cell * static_cast<float>(grid.height());
    return {{std::floor(margin + (w - gw) / 2), std::floor(band + (h - gh) / 2)}, std::max(cell, 2.0f)};
}

void draw_scene(sf::RenderTarget& target, const State& s, const Layout& L) {
    target.clear(palette::kBackground);
    const pf::Grid& g = s.grid;
    const float gap = L.cell >= 8 ? 1.0f : 0.0f;

    sf::RectangleShape board({L.cell * static_cast<float>(g.width()) + 12, L.cell * static_cast<float>(g.height()) + 12});
    board.setPosition(L.origin.x - 6, L.origin.y - 6);
    board.setFillColor(palette::kBoard);
    target.draw(board);

    // Base colour per cell; walls stay the board colour, so open space reads as carved.
    std::vector<sf::Color> colour(static_cast<std::size_t>(g.size()));
    for (int i = 0; i < g.size(); ++i) {
        const std::uint8_t c = g.cost(g.point(i));
        colour[i] = c == pf::Grid::kWall ? palette::kBoard : terrain(c);
    }

    // The replayed expansion: hue by order, brightness kept lower on costly terrain so the
    // cost still shows through, and a short fade from white for the newest cells.
    const auto& expanded = s.result.expanded;
    const auto shown = static_cast<std::size_t>(s.revealed);
    const float n = static_cast<float>(std::max<std::size_t>(expanded.size(), 1));
    for (std::size_t k = 0; k < shown; ++k) {
        const int i = g.index(expanded[k]);
        sf::Color wave = mix(palette::kWaveEarly, palette::kWaveLate, static_cast<float>(k) / n);
        // costly cells stay darker, so terrain still reads under the wavefront
        const std::uint8_t c = g.cost(expanded[k]);
        if (c > 1) wave = mix(wave, palette::kBoard, 0.55f * static_cast<float>(c - 1) / 8.0f);
        const float age = k < s.revealed_at.size() ? static_cast<float>(s.clock) - s.revealed_at[k] : 1.0f;
        colour[i] = mix(palette::kFresh, wave, age / 0.35f);
    }

    sf::VertexArray cells(sf::Quads);
    cells.resize(0);
    for (int i = 0; i < g.size(); ++i) {
        if (g.cost(g.point(i)) == pf::Grid::kWall) continue;
        const pf::Point p = g.point(i);
        add_quad(cells, {L.origin.x + static_cast<float>(p.x) * L.cell + gap / 2, L.origin.y + static_cast<float>(p.y) * L.cell + gap / 2},
                 L.cell - gap, colour[i]);
    }
    target.draw(cells);

    // Path: a thick line through cell centres with rounded joints, drawn progressively.
    const auto& path = s.result.path;
    const auto drawn = std::min(path.size(), static_cast<std::size_t>(std::ceil(s.path_shown)));
    if (drawn > 0) {
        const float width = std::max(2.0f, L.cell * 0.36f);
        sf::CircleShape joint(width / 2, 16);
        joint.setOrigin(width / 2, width / 2);
        joint.setFillColor(palette::kPath);
        for (std::size_t k = 0; k < drawn; ++k) {
            const sf::Vector2f c = L.centre(path[k]);
            joint.setPosition(c);
            target.draw(joint);
            if (k + 1 < drawn) {
                const sf::Vector2f d = L.centre(path[k + 1]);
                sf::RectangleShape seg({std::abs(d.x - c.x) + width, std::abs(d.y - c.y) + width});
                seg.setPosition(std::min(c.x, d.x) - width / 2, std::min(c.y, d.y) - width / 2);
                seg.setFillColor(palette::kPath);
                target.draw(seg);
            }
        }
    }

    // Hover outline.
    if (s.hover) {
        sf::RectangleShape h({L.cell - 1, L.cell - 1});
        h.setPosition(L.origin.x + static_cast<float>(s.hover->x) * L.cell, L.origin.y + static_cast<float>(s.hover->y) * L.cell);
        h.setFillColor(sf::Color::Transparent);
        h.setOutlineThickness(1.5f);
        h.setOutlineColor({255, 255, 255, 150});
        target.draw(h);
    }

    const float r = std::max(3.0f, L.cell * 0.34f);
    marker(target, L.centre(s.start), r, palette::kStart);
    marker(target, L.centre(s.goal), r, palette::kGoal);
}

}  // namespace app
