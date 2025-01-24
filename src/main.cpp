#include <SFML/Graphics.hpp>
// #include<Windows.h>
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

// Constants
const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 800;
const int GRID_SIZE = 40;
const int CELL_SIZE = min(WINDOW_HEIGHT, WINDOW_WIDTH) / GRID_SIZE;
const int UI_WIDTH = 300;
const int BTN_HEIGHT = 40;
const int BTN_SPACING = 10;
#ifndef INFINITY
const float INFINITY = numeric_limits<float>::max();
#endif
enum class CellType { Empty, Wall, Start, End, Path, Visited };
enum class Algorithm { BFS, DFS, AStar, Dijkstra, Greedy };
enum class State { IDLE, VISUALIZING };


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
string statusMessage = "Ready";
double lastBenchmark = 0.0;
bool pathFound = false;
State currentState = State::IDLE;

namespace Colors {
    const sf::Color Background(40, 40, 40);
    const sf::Color Wall(30, 30, 30);
    const sf::Color Start(0, 200, 0);
    const sf::Color End(200, 0, 0);
    const sf::Color Path(255, 255, 100);
    const sf::Color Visited(100, 200, 255);
    const sf::Color Button(70, 70, 70);
    const sf::Color ButtonHover(100, 100, 100);
    const sf::Color Text(255, 255, 255);
}


class MazeGenerator {
public:
    static void generateRandomWalls(double probability) {
        cout << "Generating random walls..." << endl; // Debug statement

        static mt19937 rng(chrono::system_clock::now().time_since_epoch().count());
        bernoulli_distribution dist(probability);

        for (auto& row : grid) {
            for (auto& cell : row) {
                if (cell.type != CellType::Start && cell.type != CellType::End) {
                    cell.type = dist(rng) ? CellType::Wall : CellType::Empty;
                }
            }
        }

        cout << "Random walls generated!" << endl; // Debug statement
    }
};
void drawGrid(sf::RenderWindow& window) {
    cout << "Drawing grid..." << endl; // Debug statement

    for (auto& row : grid) {
        for (auto& cell : row) {
            switch (cell.type) {
            case CellType::Wall: cell.rect.setFillColor(Colors::Wall); break;
            case CellType::Start: cell.rect.setFillColor(Colors::Start); break;
            case CellType::End: cell.rect.setFillColor(Colors::End); break;
            case CellType::Path: cell.rect.setFillColor(Colors::Path); break;
            case CellType::Visited: cell.rect.setFillColor(Colors::Visited); break;
            default: cell.rect.setFillColor(sf::Color::White);
            }
            window.draw(cell.rect);
        }
    }

    // Draw grid lines
    sf::RectangleShape line(sf::Vector2f(1, WINDOW_HEIGHT));
    line.setFillColor(sf::Color(50, 50, 50));
    for (int x = 0; x <= GRID_SIZE; ++x) {
        line.setPosition(x * CELL_SIZE, 0);
        window.draw(line);
    }

    line.setSize(sf::Vector2f(WINDOW_WIDTH, 1));
    for (int y = 0; y <= GRID_SIZE; ++y) {
        line.setPosition(0, y * CELL_SIZE);
        window.draw(line);
    }
}

class Pathfinder {
public:
    static bool findPath(Algorithm algo, sf::RenderWindow& window, double& duration) {
        auto start = high_resolution_clock::now();
        pathFound = false;

        switch (algo) {
        case Algorithm::BFS: pathFound = BFS(window); break;
        case Algorithm::DFS: pathFound = DFS(window); break;
        case Algorithm::AStar: pathFound = aStar(window); break;
        case Algorithm::Dijkstra: pathFound = dijkstra(window); break;
        case Algorithm::Greedy: pathFound = greedy(window); break;
        }

        duration = duration_cast<milliseconds>(high_resolution_clock::now() - start).count() / 1000.0;
        lastBenchmark = duration;
        return pathFound;
    }

private:
    struct Node {
        int x, y;
        float g, h;

        Node(int x, int y, float g, float h) : x(x), y(y), g(g), h(h) {}
        bool operator>(const Node& other) const { return (g + h) > (other.g + other.h); }
    };

    static bool BFS(sf::RenderWindow& window) {
        queue<pair<int, int>> q;
        vector<vector<bool>> visited(GRID_SIZE, vector<bool>(GRID_SIZE, false));
        vector<vector<pair<int, int>>> parent(GRID_SIZE, vector<pair<int, int>>(GRID_SIZE, { -1, -1 }));

        q.push(startPos);
        visited[startPos.second][startPos.first] = true;

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();

            if (x == endPos.first && y == endPos.second) {
                reconstructPath(parent, window);
                return true;
            }

            for (int dx : {-1, 0, 1}) {
                for (int dy : {-1, 0, 1}) {
                    if (abs(dx) + abs(dy) != 1) continue;

                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE &&
                        !visited[ny][nx] && grid[ny][nx].type != CellType::Wall) {
                        visited[ny][nx] = true;
                        parent[ny][nx] = { x, y };
                        q.push({ nx, ny });
                        updateVisual(ny, nx, window);
                    }
                }
            }
        }
        return false;
    }

    static bool DFS(sf::RenderWindow& window) {
        stack<pair<int, int>> s;
        vector<vector<bool>> visited(GRID_SIZE, vector<bool>(GRID_SIZE, false));
        vector<vector<pair<int, int>>> parent(GRID_SIZE, vector<pair<int, int>>(GRID_SIZE, { -1, -1 }));

        s.push(startPos);
        visited[startPos.second][startPos.first] = true;

        while (!s.empty()) {
            auto [x, y] = s.top();
            s.pop();

            if (x == endPos.first && y == endPos.second) {
                reconstructPath(parent, window);
                return true;
            }

            for (int dx : {-1, 0, 1}) {
                for (int dy : {-1, 0, 1}) {
                    if (abs(dx) + abs(dy) != 1) continue;

                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE &&
                        !visited[ny][nx] && grid[ny][nx].type != CellType::Wall) {
                        visited[ny][nx] = true;
                        parent[ny][nx] = { x, y };
                        s.push({ nx, ny });
                        updateVisual(ny, nx, window);
                    }
                }
            }
        }
        return false;
    }

    static bool dijkstra(sf::RenderWindow& window) {
        priority_queue<pair<float, pair<int, int>>,
            vector<pair<float, pair<int, int>>>,
            greater<>> pq;

        vector<vector<float>> dist(GRID_SIZE, vector<float>(GRID_SIZE, INFINITY));
        vector<vector<pair<int, int>>> parent(GRID_SIZE, vector<pair<int, int>>(GRID_SIZE, { -1, -1 }));

        dist[startPos.second][startPos.first] = 0;
        pq.push({ 0, startPos });

        while (!pq.empty()) {
            auto [currentDist, pos] = pq.top();
            auto [x, y] = pos;
            pq.pop();

            if (x == endPos.first && y == endPos.second) {
                reconstructPath(parent, window);
                return true;
            }

            if (currentDist > dist[y][x]) continue;

            for (int dx : {-1, 0, 1}) {
                for (int dy : {-1, 0, 1}) {
                    if (abs(dx) + abs(dy) != 1) continue;

                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE &&
                        grid[ny][nx].type != CellType::Wall) {

                        float newDist = currentDist + 1;
                        if (newDist < dist[ny][nx]) {
                            dist[ny][nx] = newDist;
                            parent[ny][nx] = { x, y };
                            pq.push({ newDist, {nx, ny} });
                            updateVisual(ny, nx, window);
                        }
                    }
                }
            }
        }
        return false;
    }

    static bool greedy(sf::RenderWindow& window) {
        auto heuristic = [](int x1, int y1) {
            return abs(x1 - endPos.first) + abs(y1 - endPos.second);
            };

        priority_queue<pair<int, pair<int, int>>,
            vector<pair<int, pair<int, int>>>,
            greater<>> pq;

        vector<vector<bool>> visited(GRID_SIZE, vector<bool>(GRID_SIZE, false));
        vector<vector<pair<int, int>>> parent(GRID_SIZE, vector<pair<int, int>>(GRID_SIZE, { -1, -1 }));

        pq.push({ heuristic(startPos.first, startPos.second), startPos });
        visited[startPos.second][startPos.first] = true;

        while (!pq.empty()) {
            auto [hVal, pos] = pq.top();
            auto [x, y] = pos;
            pq.pop();

            if (x == endPos.first && y == endPos.second) {
                reconstructPath(parent, window);
                return true;
            }

            for (int dx : {-1, 0, 1}) {
                for (int dy : {-1, 0, 1}) {
                    if (abs(dx) + abs(dy) != 1) continue;

                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE &&
                        !visited[ny][nx] && grid[ny][nx].type != CellType::Wall) {
                        visited[ny][nx] = true;
                        parent[ny][nx] = { x, y };
                        pq.push({ heuristic(nx, ny), {nx, ny} });
                        updateVisual(ny, nx, window);
                    }
                }
            }
        }
        return false;
    }

    static bool aStar(sf::RenderWindow& window) {
        priority_queue<Node, vector<Node>, greater<Node>> openSet;
        vector<vector<float>> gScore(GRID_SIZE, vector<float>(GRID_SIZE, INFINITY));
        vector<vector<pair<int, int>>> parent(GRID_SIZE, vector<pair<int, int>>(GRID_SIZE, { -1, -1 }));

        auto heuristic = [](int x1, int y1, int x2, int y2) {
            return abs(x1 - x2) + abs(y1 - y2);
            };

        gScore[startPos.second][startPos.first] = 0;
        openSet.push(Node(startPos.first, startPos.second, 0,
            heuristic(startPos.first, startPos.second, endPos.first, endPos.second)));

        while (!openSet.empty()) {
            Node current = openSet.top();
            openSet.pop();

            if (current.x == endPos.first && current.y == endPos.second) {
                reconstructPath(parent, window);
                return true;
            }

            for (int dx : {-1, 0, 1}) {
                for (int dy : {-1, 0, 1}) {
                    if (dx == 0 && dy == 0) continue;
                    if (abs(dx) + abs(dy) == 2) continue;

                    int nx = current.x + dx, ny = current.y + dy;
                    if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) continue;
                    if (grid[ny][nx].type == CellType::Wall) continue;

                    float tentativeG = current.g + 1;
                    if (tentativeG < gScore[ny][nx]) {
                        parent[ny][nx] = { current.x, current.y };
                        gScore[ny][nx] = tentativeG;
                        float h = heuristic(nx, ny, endPos.first, endPos.second);
                        openSet.push(Node(nx, ny, tentativeG, h));

                        if (grid[ny][nx].type != CellType::Start && grid[ny][nx].type != CellType::End) {
                            grid[ny][nx].type = CellType::Visited;
                            drawGrid(window);
                            window.display();
                            sf::sleep(sf::milliseconds(visualizationDelay));
                        }
                    }
                }
            }
        }
        return false;
    }

    static void updateVisual(int y, int x, sf::RenderWindow& window) {
        if (grid[y][x].type != CellType::Start && grid[y][x].type != CellType::End) {
            grid[y][x].type = CellType::Visited;
            drawGrid(window);
            window.display();
            sf::sleep(sf::milliseconds(visualizationDelay));
        }
    }

    static void reconstructPath(const vector<vector<pair<int, int>>>& parent, sf::RenderWindow& window) {
        pair<int, int> current = endPos;
        while (current != startPos) {
            grid[current.second][current.first].type = CellType::Path;
            current = parent[current.second][current.first];
            drawGrid(window);
            window.display();
            sf::sleep(sf::milliseconds(visualizationDelay));
        }
    }
};

void handleMouseClick(sf::RenderWindow& window, sf::Event::MouseButtonEvent event) {
    if (currentState != State::IDLE) return;

    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    if (mousePos.x >= WINDOW_WIDTH - UI_WIDTH) return;

    int gridX = mousePos.x / CELL_SIZE;
    int gridY = mousePos.y / CELL_SIZE;

    if (gridX < 0 || gridX >= GRID_SIZE || gridY < 0 || gridY >= GRID_SIZE) return;

    Cell& cell = grid[gridY][gridX];

    if (event.button == sf::Mouse::Left) {
        if (cell.type == CellType::Empty) {
            if (startPos.first == -1) {
                startPos = { gridX, gridY };
                cell.type = CellType::Start;
            }
            else if (endPos.first == -1) {
                endPos = { gridX, gridY };
                cell.type = CellType::End;
            }
            else {
                cell.type = CellType::Wall;
            }
        }
        else if (cell.type == CellType::Start) {
            startPos = { -1, -1 };
            cell.type = CellType::Empty;
        }
        else if (cell.type == CellType::End) {
            endPos = { -1, -1 };
            cell.type = CellType::Empty;
        }
    }
    else if (event.button == sf::Mouse::Right) {
        if (cell.type == CellType::Wall) {
            cell.type = CellType::Empty;
        }
        else if (cell.type == CellType::Start) {
            startPos = { -1, -1 };
            cell.type = CellType::Empty;
        }
        else if (cell.type == CellType::End) {
            endPos = { -1, -1 };
            cell.type = CellType::Empty;
        }
    }

    drawGrid(window);
    window.display();
}

void drawUI(sf::RenderWindow& window) {
    // UI Background
    sf::RectangleShape panel(sf::Vector2f(UI_WIDTH, WINDOW_HEIGHT));
    panel.setPosition(WINDOW_WIDTH - UI_WIDTH, 0);
    panel.setFillColor(sf::Color(50, 50, 50));
    window.draw(panel);

    float btnX = WINDOW_WIDTH - UI_WIDTH + 20;
    float btnY = 20;

    // Section 1: Pathfinding Algorithms
    sf::Text algoTitle("Pathfinding Algorithms", font, 24);
    algoTitle.setPosition(btnX, btnY);
    algoTitle.setFillColor(Colors::Text);
    window.draw(algoTitle);

    vector<sf::RectangleShape> algoBtns;
    vector<sf::Text> algoTexts;
    vector<string> algoLabels = { "BFS", "DFS", "A*", "Dijkstra", "Greedy" };

    for (size_t i = 0; i < algoLabels.size(); ++i) {
        sf::RectangleShape btn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
        btn.setPosition(btnX, btnY + 40 + i * (BTN_HEIGHT + BTN_SPACING));
        btn.setFillColor(Colors::Button);
        algoBtns.push_back(btn);

        sf::Text text(algoLabels[i], font, 20);
        text.setPosition(btnX + 10, btnY + 45 + i * (BTN_HEIGHT + BTN_SPACING));
        text.setFillColor(Colors::Text);
        algoTexts.push_back(text);
    }

    // Section 2: Maze Generation Button
    float mazeBtnY = btnY + 40 + (algoLabels.size() * (BTN_HEIGHT + BTN_SPACING)) + 20;
    sf::RectangleShape mazeBtn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
    mazeBtn.setPosition(btnX, mazeBtnY);
    mazeBtn.setFillColor(Colors::Button);
    window.draw(mazeBtn);

    sf::Text mazeText("Generate Random Maze", font, 18);
    mazeText.setPosition(btnX + 10, mazeBtnY + 5);
    mazeText.setFillColor(Colors::Text);
    window.draw(mazeText);

    // Section 3: Results
    sf::Text resultsTitle("Results", font, 24);
    resultsTitle.setPosition(btnX, mazeBtnY + BTN_HEIGHT + BTN_SPACING + 20);
    resultsTitle.setFillColor(Colors::Text);
    window.draw(resultsTitle);

    // Benchmark Text
    stringstream benchText;
    benchText << "Time: " << fixed << setprecision(3) << lastBenchmark << "s\n"
        << "Status: " << statusMessage << "\n"
        << "Result: " << (pathFound ? "Path found" : "No path");
    sf::Text benchmarkText(benchText.str(), font, 20);
    benchmarkText.setPosition(btnX, resultsTitle.getPosition().y + 40);
    benchmarkText.setFillColor(Colors::Text);
    window.draw(benchmarkText);

    // Section 4: Reset Grid Button (at bottom)
    sf::RectangleShape resetBtn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
    resetBtn.setPosition(btnX, WINDOW_HEIGHT - BTN_HEIGHT - 20);
    resetBtn.setFillColor(Colors::Button);
    window.draw(resetBtn);

    sf::Text resetText("Reset Grid", font, 20);
    resetText.setPosition(btnX + 10, WINDOW_HEIGHT - BTN_HEIGHT - 15);
    resetText.setFillColor(Colors::Text);
    window.draw(resetText);

    // Draw all buttons and text
    for (const auto& btn : algoBtns) window.draw(btn);
    for (const auto& text : algoTexts) window.draw(text);
}
int main() {
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Pathfinding Visualizer");
    window.setFramerateLimit(60); // Limit frame rate to 60 FPS
    // HWND hwnd = GetConsoleWindow();
    // ShowWindow(hwnd, SW_HIDE);

    if (!font.loadFromFile("assets/fonts/arvo.ttf")) {
        cerr << "Failed to load font!" << endl;
        return EXIT_FAILURE;
    }

    // Initialize grid
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            grid[y][x] = Cell(x, y);
        }
    }

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            if (event.type == sf::Event::MouseButtonPressed) {
                sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                // Handle Reset Grid Button
                sf::RectangleShape resetBtn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
                resetBtn.setPosition(WINDOW_WIDTH - UI_WIDTH + 20, WINDOW_HEIGHT - BTN_HEIGHT - 20);

                if (resetBtn.getGlobalBounds().contains(mousePos) && currentState == State::IDLE) {
                    // Reset ALL cells to Empty
                    for (auto& row : grid) {
                        for (auto& cell : row) {
                            cell.type = CellType::Empty;
                        }
                    }
                    startPos = { -1, -1 };
                    endPos = { -1, -1 };
                    pathFound = false;
                    lastBenchmark = 0.0;
                    statusMessage = "Grid Reset";
                }

                // Handle Pathfinding Algorithm Buttons
                vector<sf::RectangleShape> algoBtns;
                vector<string> algoLabels = { "BFS", "DFS", "A*", "Dijkstra", "Greedy" };
                float btnX = WINDOW_WIDTH - UI_WIDTH + 20;
                float mazeBtnY = 20 + 40 + (algoLabels.size() * (BTN_HEIGHT + BTN_SPACING)) + 20;
                float btnY = 20;

                for (size_t i = 0; i < algoLabels.size(); ++i) {
                    sf::RectangleShape btn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
                    btn.setPosition(btnX, btnY + 40 + i * (BTN_HEIGHT + BTN_SPACING));
                    algoBtns.push_back(btn);

                    if (btn.getGlobalBounds().contains(mousePos) && currentState == State::IDLE) {
                        currentState = State::VISUALIZING;
                        currentAlgorithm = static_cast<Algorithm>(i);
                        double duration;
                        pathFound = Pathfinder::findPath(currentAlgorithm, window, duration);
                        statusMessage = pathFound ? "Path found!" : "No path found";
                        currentState = State::IDLE;
                    }
                }

                // Handle Maze Generation Button
                sf::RectangleShape mazeBtn(sf::Vector2f(UI_WIDTH - 40, BTN_HEIGHT));
                mazeBtn.setPosition(btnX, mazeBtnY);

                if (mazeBtn.getGlobalBounds().contains(mousePos) && currentState == State::IDLE) {
                    cout << "Maze Generation button clicked!" << endl; // Debug statement
                    MazeGenerator::generateRandomWalls(0.3); // Generate random walls
                    statusMessage = "Maze Generated";
                }

                // Handle grid editing
                if (mousePos.x < WINDOW_WIDTH - UI_WIDTH) {
                    handleMouseClick(window, event.mouseButton);
                }
            }
        }

        // Rendering
        window.clear(Colors::Background);
        drawGrid(window);
        drawUI(window);
        window.display();
    }

    return 0;
}