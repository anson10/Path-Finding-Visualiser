// A 4-connected grid of cells with an entry cost each: 0 is a wall, 1..255 the cost of
// stepping onto the cell. Uniform cost 1 is an ordinary maze; higher costs model terrain
// that is passable but expensive, which is where BFS stops being optimal.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace pf {

struct Point {
    int x = 0;
    int y = 0;
    friend bool operator==(Point, Point) = default;
};

class Grid {
public:
    static constexpr std::uint8_t kWall = 0;
    static constexpr std::uint8_t kOpen = 1;

    Grid(int width, int height);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] int size() const noexcept { return width_ * height_; }

    [[nodiscard]] bool in_bounds(Point p) const noexcept {
        return p.x >= 0 && p.y >= 0 && p.x < width_ && p.y < height_;
    }
    [[nodiscard]] bool passable(Point p) const noexcept {
        return in_bounds(p) && cost_[index(p)] != kWall;
    }
    [[nodiscard]] std::uint8_t cost(Point p) const { return cost_.at(index(p)); }
    void set_cost(Point p, std::uint8_t cost) { cost_.at(index(p)) = cost; }
    void set_wall(Point p) { set_cost(p, kWall); }
    void fill(std::uint8_t cost);

    [[nodiscard]] int index(Point p) const noexcept { return p.y * width_ + p.x; }
    [[nodiscard]] Point point(int index) const noexcept { return {index % width_, index / width_}; }

    // Calls f(neighbour) for each passable 4-neighbour, always in the order up, right,
    // down, left, so every algorithm (and every test) sees the same tie order.
    template <class F>
    void for_each_neighbour(Point p, F&& f) const {
        static constexpr std::array<Point, 4> kSteps{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
        for (Point step : kSteps) {
            const Point n{p.x + step.x, p.y + step.y};
            if (passable(n)) f(n);
        }
    }

private:
    int width_;
    int height_;
    std::vector<std::uint8_t> cost_;
};

// Walls placed independently with probability `density`; the rest cost 1.
Grid random_walls(int width, int height, double density, std::uint64_t seed);

// Open cells get a random cost in [1, max_cost] and walls appear with probability `density`.
Grid random_terrain(int width, int height, double density, std::uint8_t max_cost,
                    std::uint64_t seed);

// A perfect maze (exactly one path between any two open cells) by iterative recursive
// backtracking. Open cells sit at odd coordinates; width and height should be odd.
Grid perfect_maze(int width, int height, std::uint64_t seed);

}  // namespace pf
