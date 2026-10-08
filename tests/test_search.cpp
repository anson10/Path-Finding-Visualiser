#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <queue>

#include "pathfinder/grid.hpp"
#include "pathfinder/search.hpp"

using Catch::Generators::range;
using pf::Algorithm;
using pf::Grid;
using pf::Point;

namespace {

// Corners cleared so every random grid has a passable start and goal.
Grid open_corners(Grid grid) {
    grid.set_cost({0, 0}, Grid::kOpen);
    grid.set_cost({grid.width() - 1, grid.height() - 1}, Grid::kOpen);
    return grid;
}

Point far_corner(const Grid& grid) { return {grid.width() - 1, grid.height() - 1}; }

// Flood fill from `from`: the number of passable cells it reaches.
int reachable(const Grid& grid, Point from) {
    std::vector<bool> seen(grid.size(), false);
    std::queue<Point> q;
    q.push(from);
    seen[grid.index(from)] = true;
    int count = 0;
    while (!q.empty()) {
        const Point p = q.front();
        q.pop();
        ++count;
        grid.for_each_neighbour(p, [&](Point n) {
            if (!seen[grid.index(n)]) {
                seen[grid.index(n)] = true;
                q.push(n);
            }
        });
    }
    return count;
}

}  // namespace

TEST_CASE("a straight corridor gives every algorithm the same path") {
    Grid grid(5, 1);
    for (Algorithm a : pf::kAlgorithms) {
        const auto r = pf::search(grid, {0, 0}, {4, 0}, a);
        INFO(pf::name(a));
        REQUIRE(r.found);
        CHECK(r.path.size() == 5);
        CHECK(r.cost == 4);
        CHECK(r.path.front() == Point{0, 0});
        CHECK(r.path.back() == Point{4, 0});
    }
}

TEST_CASE("start equal to goal is a zero-cost path of one cell") {
    Grid grid(3, 3);
    for (Algorithm a : pf::kAlgorithms) {
        const auto r = pf::search(grid, {1, 1}, {1, 1}, a);
        INFO(pf::name(a));
        CHECK(r.found);
        CHECK(r.cost == 0);
        CHECK(r.path == std::vector<Point>{{1, 1}});
    }
}

TEST_CASE("a walled-off goal is reported as not found, after exploring the reachable cells") {
    Grid grid(5, 5);
    for (int y = 0; y < 5; ++y) grid.set_wall({2, y});  // a full wall down the middle
    for (Algorithm a : pf::kAlgorithms) {
        const auto r = pf::search(grid, {0, 0}, {4, 4}, a);
        INFO(pf::name(a));
        CHECK_FALSE(r.found);
        CHECK(r.path.empty());
        CHECK(r.nodes_expanded() == 10);  // the two columns left of the wall
    }
}

TEST_CASE("a wall as start or goal is not found without searching") {
    Grid grid(3, 3);
    grid.set_wall({2, 2});
    const auto r = pf::search(grid, {0, 0}, {2, 2}, Algorithm::AStar);
    CHECK_FALSE(r.found);
    CHECK(r.expanded.empty());
}

TEST_CASE("expensive terrain makes BFS suboptimal while Dijkstra and A* go around") {
    // Direct route through two cost-9 cells (cost 18) vs a detour of four cost-1 steps
    // around them: BFS takes the short, expensive route; Dijkstra and A* take the detour.
    Grid grid(3, 2);
    grid.set_cost({1, 0}, 9);
    grid.set_cost({2, 0}, 9);
    const auto bfs = pf::search(grid, {0, 0}, {2, 0}, Algorithm::BFS);
    const auto dijkstra = pf::search(grid, {0, 0}, {2, 0}, Algorithm::Dijkstra);
    const auto astar = pf::search(grid, {0, 0}, {2, 0}, Algorithm::AStar);
    CHECK(bfs.cost == 18);
    CHECK(dijkstra.cost == 12);  // down 1, right 1, right 1, up 9
    CHECK(astar.cost == dijkstra.cost);
}

TEST_CASE("every returned path is a valid walk with the reported cost (random grids)") {
    const auto seed = GENERATE(range<std::uint64_t>(0, 60));
    const Grid grid = open_corners(pf::random_terrain(30, 20, 0.25, 9, seed));
    for (Algorithm a : pf::kAlgorithms) {
        const auto r = pf::search(grid, {0, 0}, far_corner(grid), a);
        INFO(pf::name(a) << " seed " << seed);
        if (!r.found) continue;
        CHECK(pf::path_cost(grid, r.path) == r.cost);
        CHECK(r.path.front() == Point{0, 0});
        CHECK(r.path.back() == far_corner(grid));
    }
}

TEST_CASE("all algorithms agree on whether the goal is reachable (random grids)") {
    const auto seed = GENERATE(range<std::uint64_t>(0, 100));
    const Grid grid = open_corners(pf::random_walls(25, 25, 0.35, seed));
    const bool expected = pf::search(grid, {0, 0}, far_corner(grid), Algorithm::BFS).found;
    for (Algorithm a : pf::kAlgorithms) {
        INFO(pf::name(a) << " seed " << seed);
        CHECK(pf::search(grid, {0, 0}, far_corner(grid), a).found == expected);
    }
}

TEST_CASE("A* matches Dijkstra's cost and never expands more cells (weighted grids)") {
    const auto seed = GENERATE(range<std::uint64_t>(0, 100));
    const Grid grid = open_corners(pf::random_terrain(40, 40, 0.2, 9, seed));
    const auto dijkstra = pf::search(grid, {0, 0}, far_corner(grid), Algorithm::Dijkstra);
    const auto astar = pf::search(grid, {0, 0}, far_corner(grid), Algorithm::AStar);
    INFO("seed " << seed);
    REQUIRE(astar.found == dijkstra.found);
    CHECK(astar.cost == dijkstra.cost);
    CHECK(astar.nodes_expanded() <= dijkstra.nodes_expanded());
}

TEST_CASE("on uniform cost BFS is optimal, DFS and greedy never beat it") {
    const auto seed = GENERATE(range<std::uint64_t>(0, 100));
    const Grid grid = open_corners(pf::random_walls(30, 30, 0.3, seed));
    const auto bfs = pf::search(grid, {0, 0}, far_corner(grid), Algorithm::BFS);
    if (!bfs.found) return;
    INFO("seed " << seed);
    CHECK(pf::search(grid, {0, 0}, far_corner(grid), Algorithm::Dijkstra).cost == bfs.cost);
    CHECK(pf::search(grid, {0, 0}, far_corner(grid), Algorithm::DFS).cost >= bfs.cost);
    CHECK(pf::search(grid, {0, 0}, far_corner(grid), Algorithm::Greedy).cost >= bfs.cost);
}

TEST_CASE("each cell is expanded at most once") {
    const auto seed = GENERATE(range<std::uint64_t>(0, 30));
    const Grid grid = open_corners(pf::random_terrain(30, 30, 0.2, 9, seed));
    for (Algorithm a : pf::kAlgorithms) {
        const auto r = pf::search(grid, {0, 0}, far_corner(grid), a);
        std::vector<int> times(grid.size(), 0);
        for (Point p : r.expanded) ++times[grid.index(p)];
        INFO(pf::name(a) << " seed " << seed);
        CHECK(*std::max_element(times.begin(), times.end()) <= 1);
    }
}

TEST_CASE("a perfect maze connects every open cell, with exactly one path between two") {
    const auto seed = GENERATE(range<std::uint64_t>(0, 20));
    const Grid maze = pf::perfect_maze(41, 31, seed);
    int open = 0;
    int edges = 0;
    for (int i = 0; i < maze.size(); ++i) {
        const Point p = maze.point(i);
        if (!maze.passable(p)) continue;
        ++open;
        maze.for_each_neighbour(p, [&](Point) { ++edges; });
    }
    INFO("seed " << seed);
    CHECK(reachable(maze, {1, 1}) == open);  // connected
    CHECK(edges / 2 == open - 1);             // a tree: no loops, so one path between cells
    CHECK(open == 20 * 15 * 2 - 1);           // every lattice cell plus the walls between them
}

TEST_CASE("generators are deterministic for a seed") {
    CHECK(pf::path_cost(pf::random_walls(10, 10, 0.3, 7), {{0, 0}}) ==
          pf::path_cost(pf::random_walls(10, 10, 0.3, 7), {{0, 0}}));
    const Grid a = pf::perfect_maze(21, 21, 3);
    const Grid b = pf::perfect_maze(21, 21, 3);
    bool same = true;
    for (int i = 0; i < a.size(); ++i) same &= a.cost(a.point(i)) == b.cost(b.point(i));
    CHECK(same);
}

TEST_CASE("path_cost rejects broken walks") {
    Grid grid(3, 3);
    grid.set_wall({1, 1});
    CHECK(pf::path_cost(grid, {}) == -1);
    CHECK(pf::path_cost(grid, {{0, 0}, {2, 0}}) == -1);          // a jump
    CHECK(pf::path_cost(grid, {{1, 0}, {1, 1}}) == -1);          // through a wall
    CHECK(pf::path_cost(grid, {{0, 0}, {1, 0}, {2, 0}}) == 2);
}
