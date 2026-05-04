#include <SFML/Graphics.hpp>
#include <vector>
#include <queue>
#include <stack>
#include <chrono>
#include <random>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <limits>
using namespace std;
using namespace chrono;

const int WINDOW_WIDTH  = 1200;
const int WINDOW_HEIGHT = 800;
const int GRID_SIZE     = 40;
const int CELL_SIZE     = min(WINDOW_HEIGHT, WINDOW_WIDTH) / GRID_SIZE;
const int UI_WIDTH      = 300;
const int GRID_WIDTH    = WINDOW_WIDTH - UI_WIDTH;   // grid stops here
const int BTN_HEIGHT    = 40;
const int BTN_SPACING   = 10;

enum class CellType { Empty, Wall, Start, End, Path, Visited };
enum class Algorithm  { BFS, DFS, AStar, Dijkstra, Greedy };
enum class State      { IDLE, VISUALIZING };

struct Cell {
    sf::RectangleShape rect;
    CellType type = CellType::Empty;
    int x, y;

    Cell(int x, int y) : x(x), y(y) {
        rect.setSize(sf::Vector2f(CELL_SIZE - 1, CELL_SIZE - 1));
        rect.setPosition(x * CELL_SIZE, y * CELL_SIZE);
        rect.setFillColor(sf::Color::White);
    }
};

vector<vector<Cell>> grid(GRID_SIZE, vector<Cell>(GRID_SIZE, Cell(0, 0)));
sf::Font font;
pair<int, int> startPos(-1, -1), endPos(-1, -1);
int visualizationDelay = 10;

Algorithm currentAlgorithm = Algorithm::BFS;
string    statusMessage    = "Place start, then end, then draw walls";
double    lastBenchmark    = 0.0;
bool      pathFound        = false;
State     currentState     = State::IDLE;

namespace Colors {
    const sf::Color Background (40,  40,  40);
    const sf::Color Wall       (30,  30,  30);
    const sf::Color Start      (0,   200, 0);
    const sf::Color End        (200, 0,   0);
    const sf::Color Path       (255, 255, 100);
    const sf::Color Visited    (100, 200, 255);
    const sf::Color Button     (70,  70,  70);
    const sf::Color ButtonActive(0,  140, 210);  // selected algorithm highlight
    const sf::Color Text       (255, 255, 255);
    const sf::Color Warning    (255, 180, 0);
}

// ─── forward declarations ────────────────────────────────────────────────────
void drawGrid(sf::RenderWindow& window);
void drawUI  (sf::RenderWindow& window);

// Sleep for `ms` milliseconds while still draining the SFML event queue so
// the window stays responsive (can be moved/closed) during visualization.
static void sleepAndPoll(sf::RenderWindow& window, int ms) {
    auto deadline = high_resolution_clock::now() + milliseconds(ms);
    while (high_resolution_clock::now() < deadline) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
        }
        sf::sleep(sf::milliseconds(1));
    }
}

// ─── Maze generator ──────────────────────────────────────────────────────────
class MazeGenerator {
public:
    static void generateRandomWalls(double probability) {
        static mt19937 rng(
            static_cast<unsigned>(chrono::system_clock::now().time_since_epoch().count()));
        bernoulli_distribution dist(probability);

        for (auto& row : grid) {
            for (auto& cell : row) {
                // Never overwrite start/end; also clear stale visited/path cells
                if (cell.type == CellType::Start || cell.type == CellType::End) continue;
                cell.type = dist(rng) ? CellType::Wall : CellType::Empty;
            }
        }
    }
};

// ─── Grid rendering ──────────────────────────────────────────────────────────
void drawGrid(sf::RenderWindow& window) {
    for (auto& row : grid) {
        for (auto& cell : row) {
            switch (cell.type) {
            case CellType::Wall:    cell.rect.setFillColor(Colors::Wall);    break;
            case CellType::Start:   cell.rect.setFillColor(Colors::Start);   break;
            case CellType::End:     cell.rect.setFillColor(Colors::End);     break;
            case CellType::Path:    cell.rect.setFillColor(Colors::Path);    break;
            case CellType::Visited: cell.rect.setFillColor(Colors::Visited); break;
            default:                cell.rect.setFillColor(sf::Color::White);
            }
            window.draw(cell.rect);
        }
    }

    // Vertical grid lines
    sf::RectangleShape line(sf::Vector2f(1, WINDOW_HEIGHT));
    line.setFillColor(sf::Color(50, 50, 50));
    for (int x = 0; x <= GRID_SIZE; ++x) {
        line.setPosition(x * CELL_SIZE, 0);
        window.draw(line);
    }

    // Horizontal grid lines — stop at GRID_WIDTH, not full WINDOW_WIDTH
    line.setSize(sf::Vector2f(GRID_WIDTH, 1));
    for (int y = 0; y <= GRID_SIZE; ++y) {
        line.setPosition(0, y * CELL_SIZE);
        window.draw(line);
    }
}

// ─── Pathfinder ──────────────────────────────────────────────────────────────
class Pathfinder {
public:
    static bool findPath(Algorithm algo, sf::RenderWindow& window, double& duration) {
        auto t0 = high_resolution_clock::now();
        pathFound = false;

        switch (algo) {
        case Algorithm::BFS:      pathFound = BFS     (window); break;
        case Algorithm::DFS:      pathFound = DFS     (window); break;
        case Algorithm::AStar:    pathFound = aStar   (window); break;
        case Algorithm::Dijkstra: pathFound = dijkstra(window); break;
        case Algorithm::Greedy:   pathFound = greedy  (window); break;
        }

        duration      = duration_cast<milliseconds>(high_resolution_clock::now() - t0).count() / 1000.0;
        lastBenchmark = duration;
        return pathFound;
    }

private:
    struct Node {
        int x, y;
        float g, h;
        Node(int x, int y, float g, float h) : x(x), y(y), g(g), h(h) {}
        bool operator>(const Node& o) const { return (g + h) > (o.g + o.h); }
    };

    // Mark a cell as visited, redraw, and sleep — shared by all algorithms
    static void updateVisual(int y, int x, sf::RenderWindow& window) {
        if (grid[y][x].type != CellType::Start && grid[y][x].type != CellType::End) {
            grid[y][x].type = CellType::Visited;
            drawGrid(window);
            window.display();
            sleepAndPoll(window, visualizationDelay);
        }
    }

    // Trace the path backwards and animate it
    static void reconstructPath(const vector<vector<pair<int,int>>>& parent,
                                 sf::RenderWindow& window) {
        pair<int,int> current = endPos;
        while (current != startPos) {
            // Guard: break if index is out of range or parent not set
            if (current.first  < 0 || current.first  >= GRID_SIZE ||
                current.second < 0 || current.second >= GRID_SIZE) break;

            pair<int,int> next = parent[current.second][current.first];
            if (next.first == -1 && next.second == -1) break;

            if (grid[current.second][current.first].type != CellType::Start &&
                grid[current.second][current.first].type != CellType::End)
                grid[current.second][current.first].type = CellType::Path;

            current = next;
            drawGrid(window);
            window.display();
            sleepAndPoll(window, visualizationDelay);
        }
    }

    static bool BFS(sf::RenderWindow& window) {
        queue<pair<int,int>> q;
        vector<vector<bool>>         visited(GRID_SIZE, vector<bool>(GRID_SIZE, false));
        vector<vector<pair<int,int>>> parent (GRID_SIZE, vector<pair<int,int>>(GRID_SIZE, {-1,-1}));

        q.push(startPos);
        visited[startPos.second][startPos.first] = true;

        while (!q.empty()) {
            auto [x, y] = q.front(); q.pop();
            if (x == endPos.first && y == endPos.second) { reconstructPath(parent, window); return true; }

            for (int dx : {-1, 0, 1}) for (int dy : {-1, 0, 1}) {
                if (abs(dx) + abs(dy) != 1) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
                if (visited[ny][nx] || grid[ny][nx].type == CellType::Wall) continue;
                visited[ny][nx] = true;
                parent [ny][nx] = {x, y};
                q.push({nx, ny});
                updateVisual(ny, nx, window);
            }
        }
        return false;
    }

    static bool DFS(sf::RenderWindow& window) {
        stack<pair<int,int>> s;
        vector<vector<bool>>         visited(GRID_SIZE, vector<bool>(GRID_SIZE, false));
        vector<vector<pair<int,int>>> parent (GRID_SIZE, vector<pair<int,int>>(GRID_SIZE, {-1,-1}));

        s.push(startPos);
        visited[startPos.second][startPos.first] = true;

        while (!s.empty()) {
            auto [x, y] = s.top(); s.pop();
            if (x == endPos.first && y == endPos.second) { reconstructPath(parent, window); return true; }

            for (int dx : {-1, 0, 1}) for (int dy : {-1, 0, 1}) {
                if (abs(dx) + abs(dy) != 1) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
                if (visited[ny][nx] || grid[ny][nx].type == CellType::Wall) continue;
                visited[ny][nx] = true;
                parent [ny][nx] = {x, y};
                s.push({nx, ny});
                updateVisual(ny, nx, window);
            }
        }
        return false;
    }

    static bool dijkstra(sf::RenderWindow& window) {
        using PQEntry = pair<float, pair<int,int>>;
        priority_queue<PQEntry, vector<PQEntry>, greater<PQEntry>> pq;

        const float INF = numeric_limits<float>::max();
        vector<vector<float>>        dist  (GRID_SIZE, vector<float>(GRID_SIZE, INF));
        vector<vector<pair<int,int>>> parent(GRID_SIZE, vector<pair<int,int>>(GRID_SIZE, {-1,-1}));

        dist[startPos.second][startPos.first] = 0;
        pq.push({0, startPos});

        while (!pq.empty()) {
            auto [currentDist, pos] = pq.top(); pq.pop();
            auto [x, y] = pos;
            if (x == endPos.first && y == endPos.second) { reconstructPath(parent, window); return true; }
            if (currentDist > dist[y][x]) continue;

            for (int dx : {-1, 0, 1}) for (int dy : {-1, 0, 1}) {
                if (abs(dx) + abs(dy) != 1) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
                if (grid[ny][nx].type == CellType::Wall) continue;
                float newDist = currentDist + 1;
                if (newDist < dist[ny][nx]) {
                    dist  [ny][nx] = newDist;
                    parent[ny][nx] = {x, y};
                    pq.push({newDist, {nx, ny}});
                    updateVisual(ny, nx, window);
                }
            }
        }
        return false;
    }

    static bool greedy(sf::RenderWindow& window) {
        auto h = [](int x, int y) {
            return abs(x - endPos.first) + abs(y - endPos.second);
        };
        using PQEntry = pair<int, pair<int,int>>;
        priority_queue<PQEntry, vector<PQEntry>, greater<PQEntry>> pq;

        vector<vector<bool>>         visited(GRID_SIZE, vector<bool>(GRID_SIZE, false));
        vector<vector<pair<int,int>>> parent (GRID_SIZE, vector<pair<int,int>>(GRID_SIZE, {-1,-1}));

        pq.push({h(startPos.first, startPos.second), startPos});
        visited[startPos.second][startPos.first] = true;

        while (!pq.empty()) {
            auto [hVal, pos] = pq.top(); pq.pop();
            auto [x, y] = pos;
            if (x == endPos.first && y == endPos.second) { reconstructPath(parent, window); return true; }

            for (int dx : {-1, 0, 1}) for (int dy : {-1, 0, 1}) {
                if (abs(dx) + abs(dy) != 1) continue;
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
                if (visited[ny][nx] || grid[ny][nx].type == CellType::Wall) continue;
                visited[ny][nx] = true;
                parent [ny][nx] = {x, y};
                pq.push({h(nx, ny), {nx, ny}});
                updateVisual(ny, nx, window);
            }
        }
        return false;
    }

    static bool aStar(sf::RenderWindow& window) {
        priority_queue<Node, vector<Node>, greater<Node>> openSet;

        const float INF = numeric_limits<float>::max();
        vector<vector<float>>        gScore(GRID_SIZE, vector<float>(GRID_SIZE, INF));
        vector<vector<pair<int,int>>> parent(GRID_SIZE, vector<pair<int,int>>(GRID_SIZE, {-1,-1}));

        auto heuristic = [](int x1, int y1, int x2, int y2) -> float {
            return static_cast<float>(abs(x1 - x2) + abs(y1 - y2));
        };

        gScore[startPos.second][startPos.first] = 0;
        openSet.push(Node(startPos.first, startPos.second, 0,
            heuristic(startPos.first, startPos.second, endPos.first, endPos.second)));

        while (!openSet.empty()) {
            Node cur = openSet.top(); openSet.pop();
            if (cur.x == endPos.first && cur.y == endPos.second) {
                reconstructPath(parent, window); return true;
            }

            for (int dx : {-1, 0, 1}) for (int dy : {-1, 0, 1}) {
                if (dx == 0 && dy == 0) continue;
                if (abs(dx) + abs(dy) == 2) continue;
                int nx = cur.x + dx, ny = cur.y + dy;
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
                if (grid[ny][nx].type == CellType::Wall) continue;

                float tG = cur.g + 1;
                if (tG < gScore[ny][nx]) {
                    parent[ny][nx] = {cur.x, cur.y};
                    gScore[ny][nx] = tG;
                    openSet.push(Node(nx, ny, tG,
                        heuristic(nx, ny, endPos.first, endPos.second)));
                    updateVisual(ny, nx, window);  // consistent with all other algorithms
                }
            }
        }
        return false;
    }
};

// ─── Mouse input ─────────────────────────────────────────────────────────────
void handleMouseClick(sf::RenderWindow& window, sf::Event::MouseButtonEvent event) {
    if (currentState != State::IDLE) return;

    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    if (mousePos.x >= GRID_WIDTH) return;

    int gridX = mousePos.x / CELL_SIZE;
    int gridY = mousePos.y / CELL_SIZE;
    if (gridX < 0 || gridX >= GRID_SIZE || gridY < 0 || gridY >= GRID_SIZE) return;

    Cell& cell = grid[gridY][gridX];

    if (event.button == sf::Mouse::Left) {
        switch (cell.type) {
        case CellType::Empty:
        case CellType::Visited:
        case CellType::Path:
            if (startPos.first == -1) {
                startPos = {gridX, gridY};
                cell.type = CellType::Start;
            } else if (endPos.first == -1) {
                endPos = {gridX, gridY};
                cell.type = CellType::End;
            } else {
                cell.type = CellType::Wall;
            }
            break;
        case CellType::Wall:
            cell.type = CellType::Empty;   // left-click toggles wall off
            break;
        case CellType::Start:
            startPos  = {-1, -1};
            cell.type = CellType::Empty;
            break;
        case CellType::End:
            endPos    = {-1, -1};
            cell.type = CellType::Empty;
            break;
        }
    } else if (event.button == sf::Mouse::Right) {
        if (cell.type == CellType::Wall || cell.type == CellType::Visited ||
            cell.type == CellType::Path) {
            cell.type = CellType::Empty;
        } else if (cell.type == CellType::Start) {
            startPos = {-1, -1}; cell.type = CellType::Empty;
        } else if (cell.type == CellType::End) {
            endPos   = {-1, -1}; cell.type = CellType::Empty;
        }
    }
}

// ─── UI rendering ────────────────────────────────────────────────────────────
void drawUI(sf::RenderWindow& window) {
    sf::RectangleShape panel(sf::Vector2f(UI_WIDTH, WINDOW_HEIGHT));
    panel.setPosition(GRID_WIDTH, 0);
    panel.setFillColor(sf::Color(50, 50, 50));
    window.draw(panel);

    const float btnX = GRID_WIDTH + 20;
    const float btnY = 20;
    const vector<string> algoLabels = {"BFS", "DFS", "A*", "Dijkstra", "Greedy"};

    // Section: algorithms
    sf::Text algoTitle("Pathfinding Algorithms", font, 22);
    algoTitle.setPosition(btnX, btnY);
    algoTitle.setFillColor(Colors::Text);
    window.draw(algoTitle);

    for (size_t i = 0; i < algoLabels.size(); ++i) {
        sf::RectangleShape btn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
        btn.setPosition(btnX, btnY + 35 + i * (BTN_HEIGHT + BTN_SPACING));
        bool isSelected = (static_cast<Algorithm>(i) == currentAlgorithm);
        btn.setFillColor(isSelected ? Colors::ButtonActive : Colors::Button);
        window.draw(btn);

        sf::Text text(algoLabels[i], font, 20);
        text.setPosition(btnX + 10, btnY + 40 + i * (BTN_HEIGHT + BTN_SPACING));
        text.setFillColor(Colors::Text);
        window.draw(text);
    }

    // Section: maze
    float mazeBtnY = btnY + 35 + algoLabels.size() * (BTN_HEIGHT + BTN_SPACING) + 15;
    sf::RectangleShape mazeBtn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
    mazeBtn.setPosition(btnX, mazeBtnY);
    mazeBtn.setFillColor(Colors::Button);
    window.draw(mazeBtn);

    sf::Text mazeText("Generate Random Maze", font, 17);
    mazeText.setPosition(btnX + 5, mazeBtnY + 8);
    mazeText.setFillColor(Colors::Text);
    window.draw(mazeText);

    // Section: results
    float resultsY = mazeBtnY + BTN_HEIGHT + 20;
    sf::Text resultsTitle("Results", font, 22);
    resultsTitle.setPosition(btnX, resultsY);
    resultsTitle.setFillColor(Colors::Text);
    window.draw(resultsTitle);

    stringstream ss;
    ss << "Time:   " << fixed << setprecision(3) << lastBenchmark << " s\n"
       << "Status: " << statusMessage << "\n"
       << "Result: " << (pathFound ? "Path found" : "No path");
    sf::Text results(ss.str(), font, 18);
    results.setPosition(btnX, resultsY + 35);
    results.setFillColor(Colors::Text);
    window.draw(results);

    // Section: reset (pinned to bottom)
    sf::RectangleShape resetBtn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
    resetBtn.setPosition(btnX, WINDOW_HEIGHT - BTN_HEIGHT - 20);
    resetBtn.setFillColor(Colors::Button);
    window.draw(resetBtn);

    sf::Text resetText("Reset Grid", font, 20);
    resetText.setPosition(btnX + 10, WINDOW_HEIGHT - BTN_HEIGHT - 15);
    resetText.setFillColor(Colors::Text);
    window.draw(resetText);
}

// ─── Entry point ─────────────────────────────────────────────────────────────
int main() {
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
                            "Pathfinding Visualizer");
    window.setFramerateLimit(60);

    if (!font.loadFromFile("assets/fonts/arvo.ttf")) {
        cerr << "Failed to load font: assets/fonts/arvo.ttf\n";
        return EXIT_FAILURE;
    }

    for (int y = 0; y < GRID_SIZE; ++y)
        for (int x = 0; x < GRID_SIZE; ++x)
            grid[y][x] = Cell(x, y);

    // Pre-compute button positions once (same formula as drawUI)
    const float btnX = GRID_WIDTH + 20;
    const float btnY = 20;
    const vector<string> algoLabels = {"BFS", "DFS", "A*", "Dijkstra", "Greedy"};
    const float mazeBtnY = btnY + 35 + algoLabels.size() * (BTN_HEIGHT + BTN_SPACING) + 15;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();

            if (event.type == sf::Event::MouseButtonPressed) {
                sf::Vector2f mp = window.mapPixelToCoords(sf::Mouse::getPosition(window));

                // ── Reset button ──────────────────────────────────────────────
                sf::FloatRect resetBounds(btnX, WINDOW_HEIGHT - BTN_HEIGHT - 20,
                                          UI_WIDTH - 40, BTN_HEIGHT);
                if (resetBounds.contains(mp) && currentState == State::IDLE) {
                    for (auto& row : grid)
                        for (auto& cell : row)
                            cell.type = CellType::Empty;
                    startPos      = {-1, -1};
                    endPos        = {-1, -1};
                    pathFound     = false;
                    lastBenchmark = 0.0;
                    statusMessage = "Grid Reset";
                    continue;
                }

                // ── Algorithm buttons ─────────────────────────────────────────
                bool clickedAlgo = false;
                for (size_t i = 0; i < algoLabels.size(); ++i) {
                    sf::FloatRect bounds(btnX,
                                        btnY + 35 + i * (BTN_HEIGHT + BTN_SPACING),
                                        UI_WIDTH - 40, BTN_HEIGHT);
                    if (!bounds.contains(mp)) continue;
                    clickedAlgo = true;
                    if (currentState != State::IDLE) break;

                    // Crash fix: require start & end before running any algorithm
                    if (startPos.first == -1 || endPos.first == -1) {
                        statusMessage = "Place start & end first!";
                        break;
                    }

                    currentState      = State::VISUALIZING;
                    currentAlgorithm  = static_cast<Algorithm>(i);
                    double dur        = 0;
                    pathFound         = Pathfinder::findPath(currentAlgorithm, window, dur);
                    statusMessage     = pathFound ? "Path found!" : "No path found";
                    currentState      = State::IDLE;
                    break;
                }
                if (clickedAlgo) continue;

                // ── Maze button ───────────────────────────────────────────────
                sf::FloatRect mazeBounds(btnX, mazeBtnY, UI_WIDTH - 40, BTN_HEIGHT);
                if (mazeBounds.contains(mp) && currentState == State::IDLE) {
                    // Clear stale visited/path cells before generating
                    for (auto& row : grid)
                        for (auto& cell : row)
                            if (cell.type == CellType::Visited || cell.type == CellType::Path)
                                cell.type = CellType::Empty;
                    MazeGenerator::generateRandomWalls(0.3);
                    statusMessage = "Random maze generated";
                    continue;
                }

                // ── Grid editing ──────────────────────────────────────────────
                if (mp.x < GRID_WIDTH)
                    handleMouseClick(window, event.mouseButton);
            }
        }

        window.clear(Colors::Background);
        drawGrid(window);
        drawUI(window);
        window.display();
    }

    return 0;
}
