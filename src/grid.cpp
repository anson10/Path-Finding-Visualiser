#include "pathfinder/grid.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace pf {

Grid::Grid(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("grid dimensions must be positive");
    cost_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), kOpen);
}

void Grid::fill(std::uint8_t cost) { std::fill(cost_.begin(), cost_.end(), cost); }

Grid random_walls(int width, int height, double density, std::uint64_t seed) {
    Grid grid(width, height);
    std::mt19937_64 rng(seed);
    std::bernoulli_distribution wall(density);
    for (int i = 0; i < grid.size(); ++i) {
        if (wall(rng)) grid.set_wall(grid.point(i));
    }
    return grid;
}

Grid random_terrain(int width, int height, double density, std::uint8_t max_cost,
                    std::uint64_t seed) {
    Grid grid(width, height);
    std::mt19937_64 rng(seed);
    std::bernoulli_distribution wall(density);
    std::uniform_int_distribution<int> cost(1, std::max<int>(1, max_cost));
    for (int i = 0; i < grid.size(); ++i) {
        const Point p = grid.point(i);
        grid.set_cost(p, wall(rng) ? Grid::kWall : static_cast<std::uint8_t>(cost(rng)));
    }
    return grid;
}

Grid perfect_maze(int width, int height, std::uint64_t seed) {
    Grid grid(width, height);
    grid.fill(Grid::kWall);
    if (width < 3 || height < 3) return grid;

    std::mt19937_64 rng(seed);
    std::vector<bool> carved(static_cast<std::size_t>(grid.size()), false);
    std::vector<Point> stack{{1, 1}};
    grid.set_cost({1, 1}, Grid::kOpen);
    carved[grid.index({1, 1})] = true;

    // Depth-first carving on the odd-coordinate lattice: knock down the wall between the
    // current cell and a random uncarved cell two steps away; backtrack at dead ends.
    static constexpr std::array<Point, 4> kJumps{{{0, -2}, {2, 0}, {0, 2}, {-2, 0}}};
    while (!stack.empty()) {
        const Point cur = stack.back();
        std::array<Point, 4> options{};
        int n = 0;
        for (Point jump : kJumps) {
            const Point next{cur.x + jump.x, cur.y + jump.y};
            if (next.x > 0 && next.y > 0 && next.x < width - 1 && next.y < height - 1 &&
                !carved[grid.index(next)]) {
                options[n++] = next;
            }
        }
        if (n == 0) {
            stack.pop_back();
            continue;
        }
        const Point next = options[std::uniform_int_distribution<int>(0, n - 1)(rng)];
        grid.set_cost({(cur.x + next.x) / 2, (cur.y + next.y) / 2}, Grid::kOpen);
        grid.set_cost(next, Grid::kOpen);
        carved[grid.index(next)] = true;
        stack.push_back(next);
    }
    return grid;
}

}  // namespace pf
