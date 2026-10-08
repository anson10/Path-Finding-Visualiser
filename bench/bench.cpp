// Headless benchmark: each algorithm on the same seeded grids, timed with nothing but the
// search in the loop. Reports median wall time, cells expanded, and path cost against the
// optimum (Dijkstra), as a Markdown table per scenario.
//
//   pathfinder-bench [--size N] [--seeds K]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "pathfinder/grid.hpp"
#include "pathfinder/search.hpp"

namespace {

struct Scenario {
    std::string title;
    std::function<pf::Grid(int size, std::uint64_t seed)> make;
};

template <class T>
T median(std::vector<T> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

void run(const Scenario& scenario, int size, int seeds) {
    const pf::Point start{1, 1};
    const pf::Point goal{size - 2, size - 2};

    std::vector<pf::Grid> grids;
    std::vector<long long> optimum;
    for (int s = 0; s < seeds; ++s) {
        pf::Grid grid = scenario.make(size, static_cast<std::uint64_t>(s));
        grid.set_cost(start, pf::Grid::kOpen);
        grid.set_cost(goal, pf::Grid::kOpen);
        const auto best = pf::search(grid, start, goal, pf::Algorithm::Dijkstra);
        if (!best.found) continue;  // only grids where a path exists
        optimum.push_back(best.cost);
        grids.push_back(std::move(grid));
    }

    std::printf("\n### %s (%dx%d, %zu grids with a path)\n\n", scenario.title.c_str(), size, size,
                grids.size());
    std::printf("| Algorithm | Median time (ms) | Median cells expanded | Path cost vs optimum |\n");
    std::printf("|---|---|---|---|\n");
    for (pf::Algorithm algorithm : pf::kAlgorithms) {
        std::vector<double> ms;
        std::vector<std::size_t> expanded;
        std::vector<double> ratio;
        for (std::size_t g = 0; g < grids.size(); ++g) {
            const auto t0 = std::chrono::steady_clock::now();
            const auto result = pf::search(grids[g], start, goal, algorithm);
            const auto t1 = std::chrono::steady_clock::now();
            ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            expanded.push_back(result.nodes_expanded());
            ratio.push_back(static_cast<double>(result.cost) / static_cast<double>(optimum[g]));
        }
        std::printf("| %s | %.2f | %zu | %+.1f%% |\n", std::string(pf::name(algorithm)).c_str(),
                    median(ms), median(expanded), 100.0 * (median(ratio) - 1.0));
    }
}

int arg(int argc, char** argv, std::string_view flag, int fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == flag) return std::atoi(argv[i + 1]);
    }
    return fallback;
}

}  // namespace

int main(int argc, char** argv) {
    const int size = arg(argc, argv, "--size", 501) | 1;  // odd, so mazes fill the grid
    const int seeds = arg(argc, argv, "--seeds", 25);

    const std::vector<Scenario> scenarios{
        {"Random walls, 30% density, uniform cost",
         [](int n, std::uint64_t s) { return pf::random_walls(n, n, 0.30, s); }},
        {"Weighted terrain, costs 1-9, 20% walls",
         [](int n, std::uint64_t s) { return pf::random_terrain(n, n, 0.20, 9, s); }},
        {"Perfect maze", [](int n, std::uint64_t s) { return pf::perfect_maze(n, n, s); }},
    };
    std::printf("# pathfinder benchmark: start (1,1), goal (%d,%d), %d seeds per scenario\n",
                size - 2, size - 2, seeds);
    for (const Scenario& scenario : scenarios) run(scenario, size, seeds);
    return 0;
}
