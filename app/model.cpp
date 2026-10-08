#include <algorithm>
#include <chrono>

#include "app.hpp"

namespace app {
namespace {

double timed_search(const State& s, pf::Algorithm algorithm, pf::SearchResult& out) {
    const auto t0 = std::chrono::steady_clock::now();
    out = pf::search(s.grid, s.start, s.goal, algorithm);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

void clear_search(State& s) {
    s.result = {};
    s.search_ms = 0.0;
    s.phase = Phase::Idle;
    s.revealed = s.path_shown = s.clock = 0.0;
    s.revealed_at.clear();
}

void generate(State& s) {
    auto& g = s.settings;
    const auto seed = static_cast<std::uint64_t>(g.seed);
    switch (g.generator) {
        case Generator::Empty: s.grid = pf::Grid(g.cols, g.rows); break;
        case Generator::Maze: s.grid = pf::perfect_maze(g.cols, g.rows, seed); break;
        case Generator::RandomWalls: s.grid = pf::random_walls(g.cols, g.rows, g.density, seed); break;
        case Generator::Terrain:
            s.grid = pf::random_terrain(g.cols, g.rows, g.density, 9, seed);
            break;
    }
    s.start = {1, 1};
    s.goal = {g.cols - 2, g.rows - 2};
    s.grid.set_cost(s.start, pf::Grid::kOpen);
    s.grid.set_cost(s.goal, pf::Grid::kOpen);
    s.comparison.clear();
    clear_search(s);
}

void run(State& s) {
    clear_search(s);
    s.search_ms = timed_search(s, s.algorithm, s.result);
    s.phase = Phase::Expanding;
    s.paused = false;
}

void compare_all(State& s) {
    s.comparison.clear();
    for (pf::Algorithm a : pf::kAlgorithms) {
        Comparison c{a, {}, 0.0};
        c.ms = timed_search(s, a, c.result);
        s.comparison.push_back(std::move(c));
    }
    run(s);  // and replay the selected algorithm
}

void advance(State& s, float dt) {
    if (s.paused || s.phase == Phase::Idle) return;
    s.clock += dt;
    if (s.phase == Phase::Expanding) {
        s.revealed += static_cast<double>(s.speed) * dt;
        if (s.revealed >= static_cast<double>(s.result.expanded.size())) {
            s.revealed = static_cast<double>(s.result.expanded.size());
            s.phase = s.result.found ? Phase::Tracing : Phase::Done;
        }
        while (s.revealed_at.size() < static_cast<std::size_t>(s.revealed))
            s.revealed_at.push_back(static_cast<float>(s.clock));
    } else if (s.phase == Phase::Tracing) {
        // draw the path in about 0.6 s whatever its length
        const double length = static_cast<double>(s.result.path.size());
        s.path_shown += std::max(length / 0.6, 30.0) * dt;
        if (s.path_shown >= length) {
            s.path_shown = length;
            s.phase = Phase::Done;
        }
    }
}

void skip_to_end(State& s) {
    if (s.phase == Phase::Idle) return;
    s.revealed = static_cast<double>(s.result.expanded.size());
    s.path_shown = static_cast<double>(s.result.path.size());
    s.revealed_at.resize(s.result.expanded.size(), static_cast<float>(s.clock - 1.0));  // already settled
    s.phase = Phase::Done;
}

}  // namespace app
