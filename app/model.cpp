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

void keep_endpoints_open(State& s) {
    s.grid.set_cost(s.start, pf::Grid::kOpen);
    s.grid.set_cost(s.goal, pf::Grid::kOpen);
}

}  // namespace

void clear_search(State& s) {
    s.result = {};
    s.search_ms = 0.0;
    s.phase = Phase::Idle;
    s.revealed = s.path_shown = s.clock = 0.0;
    s.revealed_at.clear();
}

void generate(State& s, Generator generator) {
    const auto seed = static_cast<std::uint64_t>(++s.seed);
    switch (generator) {
        case Generator::Empty: s.grid = pf::Grid(s.cols, s.rows); break;
        case Generator::Maze: s.grid = pf::perfect_maze(s.cols, s.rows, seed); break;
        case Generator::RandomWalls: s.grid = pf::random_walls(s.cols, s.rows, 0.3, seed); break;
        case Generator::Terrain: s.grid = pf::random_terrain(s.cols, s.rows, 0.15, 9, seed); break;
    }
    keep_endpoints_open(s);
    s.comparison.clear();
    clear_search(s);
}

void reset_grid(State& s) {
    generate(s, Generator::Empty);
    s.status = "Grid reset";
}

void run(State& s, pf::Algorithm algorithm) {
    clear_search(s);
    s.algorithm = algorithm;
    s.search_ms = timed_search(s, algorithm, s.result);
    s.phase = Phase::Expanding;
    s.paused = false;
    s.status = "Searching...";
}

void compare_all(State& s) {
    s.comparison.clear();
    for (pf::Algorithm a : pf::kAlgorithms) {
        Comparison c{a, {}, 0.0};
        c.ms = timed_search(s, a, c.result);
        s.comparison.push_back(std::move(c));
    }
    run(s, s.algorithm);  // and replay the selected one
}

void advance(State& s, float dt) {
    if (s.paused || s.phase == Phase::Idle) return;
    s.clock += dt;
    if (s.phase == Phase::Expanding) {
        s.revealed = std::min(s.revealed + static_cast<double>(s.speed) * dt,
                              static_cast<double>(s.result.expanded.size()));
        while (s.revealed_at.size() < static_cast<std::size_t>(s.revealed))
            s.revealed_at.push_back(static_cast<float>(s.clock));
        if (s.revealed >= static_cast<double>(s.result.expanded.size()))
            s.phase = s.result.found ? Phase::Tracing : Phase::Done;
    } else if (s.phase == Phase::Tracing) {
        // trace the path back in about half a second, whatever its length
        const double length = static_cast<double>(s.result.path.size());
        s.path_shown = std::min(s.path_shown + std::max(length / 0.5, 30.0) * dt, length);
        if (s.path_shown >= length) s.phase = Phase::Done;
    }
    if (s.phase == Phase::Done && s.status == "Searching...")
        s.status = s.result.found ? "Path found!" : "No path found";
}

void skip_to_end(State& s) {
    if (s.phase == Phase::Idle) return;
    s.revealed = static_cast<double>(s.result.expanded.size());
    s.path_shown = static_cast<double>(s.result.path.size());
    s.revealed_at.resize(s.result.expanded.size(), static_cast<float>(s.clock - 1.0));  // settled
    s.phase = Phase::Done;
    s.status = s.result.found ? "Path found!" : "No path found";
}

}  // namespace app
