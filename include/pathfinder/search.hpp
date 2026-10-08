// Grid search as data: every algorithm returns the path it found and the order in which it
// expanded cells, so a front end can replay the search and a benchmark can time it without
// any drawing in the loop.
#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include "pathfinder/grid.hpp"

namespace pf {

enum class Algorithm { BFS, DFS, Dijkstra, AStar, Greedy };

inline constexpr std::array<Algorithm, 5> kAlgorithms{
    Algorithm::BFS, Algorithm::DFS, Algorithm::Dijkstra, Algorithm::AStar, Algorithm::Greedy};

[[nodiscard]] std::string_view name(Algorithm algorithm) noexcept;

// Whether the algorithm guarantees a least-cost path: Dijkstra and A* always, BFS only
// when every cell costs the same, DFS and greedy best-first never.
[[nodiscard]] bool optimal_on(Algorithm algorithm, bool uniform_cost) noexcept;

struct SearchResult {
    bool found = false;
    std::vector<Point> path;      // start .. goal inclusive; empty when not found
    std::vector<Point> expanded;  // cells in the order they were expanded (closed)
    long long cost = 0;           // sum of the entry costs along the path (start excluded)

    [[nodiscard]] std::size_t nodes_expanded() const noexcept { return expanded.size(); }
};

// `astar_weight` w >= 1 turns A* into weighted A*: it orders cells by g + w·h, trading
// optimality for speed. With the consistent heuristic used here the path costs at most w
// times the optimum (no cell is ever re-expanded). Other algorithms ignore it.
[[nodiscard]] SearchResult search(const Grid& grid, Point start, Point goal,
                                  Algorithm algorithm, double astar_weight = 1.0);

// Cost of walking `path` on `grid`, or -1 if it is not a walk of passable 4-neighbours.
[[nodiscard]] long long path_cost(const Grid& grid, const std::vector<Point>& path);

}  // namespace pf
