#include "pathfinder/search.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <tuple>

namespace pf {
namespace {

constexpr long long kInf = std::numeric_limits<long long>::max();

// Walks the parent links back from the goal; `parent[i] == -1` marks the start.
std::vector<Point> reconstruct(const Grid& grid, const std::vector<int>& parent, Point goal) {
    std::vector<Point> path;
    for (int i = grid.index(goal); i != -1; i = parent[i]) path.push_back(grid.point(i));
    std::reverse(path.begin(), path.end());
    return path;
}

long long manhattan(Point a, Point b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

// Smallest entry cost on the grid: scales the Manhattan heuristic so it never overestimates.
long long min_cost(const Grid& grid) {
    int lowest = std::numeric_limits<int>::max();
    for (int i = 0; i < grid.size(); ++i) {
        const int c = grid.cost(grid.point(i));
        if (c != Grid::kWall) lowest = std::min(lowest, c);
    }
    return lowest == std::numeric_limits<int>::max() ? 1 : lowest;
}

SearchResult finish(const Grid& grid, SearchResult result, const std::vector<int>& parent,
                    Point goal) {
    result.found = true;
    result.path = reconstruct(grid, parent, goal);
    result.cost = path_cost(grid, result.path);
    return result;
}

// Breadth-first: a FIFO frontier, so cells are expanded in order of hop count.
SearchResult bfs(const Grid& grid, Point start, Point goal) {
    SearchResult result;
    std::vector<int> parent(grid.size(), -1);
    std::vector<bool> seen(grid.size(), false);
    std::queue<Point> frontier;
    frontier.push(start);
    seen[grid.index(start)] = true;
    while (!frontier.empty()) {
        const Point cur = frontier.front();
        frontier.pop();
        result.expanded.push_back(cur);
        if (cur == goal) return finish(grid, std::move(result), parent, goal);
        grid.for_each_neighbour(cur, [&](Point n) {
            if (seen[grid.index(n)]) return;
            seen[grid.index(n)] = true;
            parent[grid.index(n)] = grid.index(cur);
            frontier.push(n);
        });
    }
    return result;
}

// Depth-first with an explicit stack. A cell is closed when popped, not when pushed, and
// its parent is the cell that pushed the copy that got popped: that is what makes the
// expansion order a real depth-first order.
SearchResult dfs(const Grid& grid, Point start, Point goal) {
    SearchResult result;
    std::vector<int> parent(grid.size(), -1);
    std::vector<bool> closed(grid.size(), false);
    std::vector<std::pair<Point, int>> stack{{start, -1}};
    while (!stack.empty()) {
        const auto [cur, from] = stack.back();
        stack.pop_back();
        if (closed[grid.index(cur)]) continue;
        closed[grid.index(cur)] = true;
        parent[grid.index(cur)] = from;
        result.expanded.push_back(cur);
        if (cur == goal) return finish(grid, std::move(result), parent, goal);
        // Push in reverse so the first neighbour in up/right/down/left order is explored first.
        std::array<Point, 4> next{};
        int count = 0;
        grid.for_each_neighbour(cur, [&](Point n) {
            if (!closed[grid.index(n)]) next[count++] = n;
        });
        while (count > 0) stack.emplace_back(next[--count], grid.index(cur));
    }
    return result;
}

// Best-first search on a priority. Dijkstra orders by path cost g, A* by g + h with ties
// broken towards the larger g (deeper, closer to the goal), greedy by h alone. Stale
// queue entries are skipped on pop (lazy deletion) instead of decreasing keys.
enum class Order { Cost, CostPlusHeuristic, Heuristic };

// Priorities are integers: costs times kScale, so a fractional A* weight stays exact enough
// without floating-point ties.
constexpr long long kScale = 1000;

SearchResult best_first(const Grid& grid, Point start, Point goal, Order order, double weight = 1.0) {
    SearchResult result;
    const long long h_scale = order == Order::Cost ? 0 : min_cost(grid);
    const long long w = std::llround(std::max(weight, 1.0) * kScale);
    auto h = [&](Point p) { return h_scale * manhattan(p, goal); };

    std::vector<int> parent(grid.size(), -1);
    std::vector<long long> g(grid.size(), kInf);
    std::vector<bool> closed(grid.size(), false);
    // (priority, tie-break, cell); tie-break = -g so deeper nodes win ties
    using Entry = std::tuple<long long, long long, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;

    auto priority = [&](long long g_cost, Point p) {
        switch (order) {
            case Order::Cost: return g_cost * kScale;
            case Order::CostPlusHeuristic: return g_cost * kScale + w * h(p);
            case Order::Heuristic: return h(p);
        }
        return g_cost;
    };

    g[grid.index(start)] = 0;
    open.emplace(priority(0, start), 0, grid.index(start));
    while (!open.empty()) {
        const int i = std::get<2>(open.top());
        open.pop();
        if (closed[i]) continue;
        closed[i] = true;
        const Point cur = grid.point(i);
        result.expanded.push_back(cur);
        if (cur == goal) return finish(grid, std::move(result), parent, goal);
        grid.for_each_neighbour(cur, [&](Point n) {
            const int j = grid.index(n);
            if (closed[j]) return;
            const long long candidate = g[i] + grid.cost(n);
            // Greedy keeps the first parent it finds; the others relax to the cheapest.
            if (order == Order::Heuristic ? g[j] != kInf : candidate >= g[j]) return;
            g[j] = candidate;
            parent[j] = i;
            open.emplace(priority(candidate, n), -candidate, j);
        });
    }
    return result;
}

}  // namespace

std::string_view name(Algorithm algorithm) noexcept {
    switch (algorithm) {
        case Algorithm::BFS: return "BFS";
        case Algorithm::DFS: return "DFS";
        case Algorithm::Dijkstra: return "Dijkstra";
        case Algorithm::AStar: return "A*";
        case Algorithm::Greedy: return "Greedy";
    }
    return "?";
}

bool optimal_on(Algorithm algorithm, bool uniform_cost) noexcept {
    switch (algorithm) {
        case Algorithm::Dijkstra:
        case Algorithm::AStar: return true;
        case Algorithm::BFS: return uniform_cost;
        case Algorithm::DFS:
        case Algorithm::Greedy: return false;
    }
    return false;
}

SearchResult search(const Grid& grid, Point start, Point goal, Algorithm algorithm, double astar_weight) {
    if (!grid.passable(start) || !grid.passable(goal)) return {};
    switch (algorithm) {
        case Algorithm::BFS: return bfs(grid, start, goal);
        case Algorithm::DFS: return dfs(grid, start, goal);
        case Algorithm::Dijkstra: return best_first(grid, start, goal, Order::Cost);
        case Algorithm::AStar: return best_first(grid, start, goal, Order::CostPlusHeuristic, astar_weight);
        case Algorithm::Greedy: return best_first(grid, start, goal, Order::Heuristic);
    }
    return {};
}

long long path_cost(const Grid& grid, const std::vector<Point>& path) {
    if (path.empty() || !grid.passable(path.front())) return -1;
    long long total = 0;
    for (std::size_t k = 1; k < path.size(); ++k) {
        const Point a = path[k - 1];
        const Point b = path[k];
        if (manhattan(a, b) != 1 || !grid.passable(b)) return -1;
        total += grid.cost(b);
    }
    return total;
}

}  // namespace pf
