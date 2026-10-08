// The side panel, in Dear ImGui: algorithm, result, playback, grid, drawing tool, comparison.
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include "app.hpp"

namespace app {
namespace {

ImFont* g_title = nullptr;
ImFont* g_bold = nullptr;

const ImVec4 kAccent{0.39f, 0.40f, 0.95f, 1.0f};  // indigo
const ImVec4 kMuted{0.58f, 0.64f, 0.72f, 1.0f};
const ImVec4 kGood{0.20f, 0.83f, 0.60f, 1.0f};
const ImVec4 kWarn{0.98f, 0.75f, 0.14f, 1.0f};

const char* describe(pf::Algorithm a) {
    switch (a) {
        case pf::Algorithm::BFS: return "Least steps; ignores cell cost";
        case pf::Algorithm::DFS: return "Goes deep first; no guarantee";
        case pf::Algorithm::Dijkstra: return "Least cost; explores evenly";
        case pf::Algorithm::AStar: return "Least cost, guided by distance";
        case pf::Algorithm::Greedy: return "Heads for the goal; no guarantee";
    }
    return "";
}

void section(const char* title) {
    ImGui::Dummy({0, 3});
    ImGui::PushFont(g_bold);
    ImGui::TextColored(kMuted, "%s", title);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Dummy({0, 2});
}

std::string thousands(long long v) {
    std::string s = std::to_string(v);
    for (int i = static_cast<int>(s.size()) - 3; i > (v < 0 ? 1 : 0); i -= 3) s.insert(static_cast<std::size_t>(i), ",");
    return s;
}

void row(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(kMuted, "%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(value.c_str());
}

// Two buttons side by side, each half the row: the widths are fixed before either is drawn.
bool half_button(const char* label) {
    const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    return ImGui::Button(label, {w, 32});
}

void comparison_table(const State& s) {
    long long best = -1;
    std::size_t fewest = SIZE_MAX;
    for (const auto& c : s.comparison) {
        if (c.result.found && (best < 0 || c.result.cost < best)) best = c.result.cost;
        fewest = std::min(fewest, c.result.nodes_expanded());
    }
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH;
    if (!ImGui::BeginTable("cmp", 4, flags)) return;
    ImGui::TableSetupColumn("");
    ImGui::TableSetupColumn("Cost");
    ImGui::TableSetupColumn("Expanded");
    ImGui::TableSetupColumn("ms");
    ImGui::TableHeadersRow();
    for (const auto& c : s.comparison) {
        ImGui::TableNextRow();
        if (c.algorithm == s.algorithm)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(79, 70, 229, 70));
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(std::string(pf::name(c.algorithm)).c_str());
        ImGui::TableSetColumnIndex(1);
        if (!c.result.found) ImGui::TextColored(kMuted, "no path");
        else if (c.result.cost == best) ImGui::TextColored(kGood, "%s", thousands(c.result.cost).c_str());
        else ImGui::TextColored(kWarn, "%s", thousands(c.result.cost).c_str());
        ImGui::TableSetColumnIndex(2);
        const auto e = thousands(static_cast<long long>(c.result.nodes_expanded()));
        if (c.result.nodes_expanded() == fewest) ImGui::TextColored(kGood, "%s", e.c_str());
        else ImGui::TextUnformatted(e.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2f", c.ms);
    }
    ImGui::EndTable();
    ImGui::TextColored(kMuted, "Green: least cost, fewest cells. Amber: costlier.");
}

}  // namespace

bool load_fonts(const std::string& dir) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.OversampleH = 3;
    const std::string regular = dir + "/fonts/Inter-Regular.ttf";
    const std::string bold = dir + "/fonts/Inter-SemiBold.ttf";
    if (!io.Fonts->AddFontFromFileTTF(regular.c_str(), 16.5f, &cfg)) return false;
    g_bold = io.Fonts->AddFontFromFileTTF(bold.c_str(), 15.0f, &cfg);
    g_title = io.Fonts->AddFontFromFileTTF(bold.c_str(), 24.0f, &cfg);
    return g_bold && g_title;
}

void apply_theme() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowPadding = {20, 16};
    st.FramePadding = {10, 6};
    st.ItemSpacing = {8, 6};
    st.WindowRounding = 0;
    st.FrameRounding = 7;
    st.GrabRounding = 7;
    st.PopupRounding = 7;
    st.TabRounding = 7;
    st.ScrollbarRounding = 7;
    st.GrabMinSize = 14;
    st.WindowBorderSize = 0;
    st.FrameBorderSize = 0;
    st.SeparatorTextBorderSize = 1;

    ImVec4* c = st.Colors;
    const ImVec4 bg{0.067f, 0.094f, 0.153f, 1.0f};
    const ImVec4 frame{0.122f, 0.161f, 0.239f, 1.0f};
    const ImVec4 hover{0.165f, 0.212f, 0.306f, 1.0f};
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg;
    c[ImGuiCol_PopupBg] = {0.09f, 0.12f, 0.19f, 1.0f};
    c[ImGuiCol_Text] = {0.90f, 0.93f, 0.97f, 1.0f};
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_Border] = {0.20f, 0.25f, 0.33f, 1.0f};
    c[ImGuiCol_Separator] = {0.20f, 0.25f, 0.33f, 1.0f};
    c[ImGuiCol_FrameBg] = frame;
    c[ImGuiCol_FrameBgHovered] = hover;
    c[ImGuiCol_FrameBgActive] = hover;
    c[ImGuiCol_Button] = frame;
    c[ImGuiCol_ButtonHovered] = hover;
    c[ImGuiCol_ButtonActive] = {0.21f, 0.26f, 0.37f, 1.0f};
    c[ImGuiCol_Header] = {0.25f, 0.26f, 0.62f, 0.55f};
    c[ImGuiCol_HeaderHovered] = {0.30f, 0.31f, 0.75f, 0.55f};
    c[ImGuiCol_HeaderActive] = {0.33f, 0.34f, 0.85f, 0.70f};
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = {0.50f, 0.51f, 1.0f, 1.0f};
    c[ImGuiCol_PlotHistogram] = kAccent;
    c[ImGuiCol_TableHeaderBg] = frame;
    c[ImGuiCol_TableRowBgAlt] = {1, 1, 1, 0.025f};
    c[ImGuiCol_TableBorderLight] = {0.20f, 0.25f, 0.33f, 1.0f};
    c[ImGuiCol_TableBorderStrong] = {0.20f, 0.25f, 0.33f, 1.0f};
}

void draw_panel(State& s, sf::Vector2f window) {
    ImGui::SetNextWindowPos({window.x - kPanelWidth, 0});
    ImGui::SetNextWindowSize({kPanelWidth, window.y});
    ImGui::Begin("panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushFont(g_title);
    ImGui::TextUnformatted("Pathfinding");
    ImGui::PopFont();
    ImGui::TextColored(kMuted, "Grid search, recorded and replayed");

    // Algorithm
    section("ALGORITHM");
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8, 3});
    for (std::size_t i = 0; i < pf::kAlgorithms.size(); ++i) {
        const pf::Algorithm a = pf::kAlgorithms[i];
        char label[48];
        std::snprintf(label, sizeof label, "%zu   %s", i + 1, std::string(pf::name(a)).c_str());
        if (ImGui::Selectable(label, s.algorithm == a, 0, {0, 24})) s.algorithm = a;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", describe(a));
        ImGui::SameLine(kPanelWidth - 158);
        ImGui::TextColored(pf::optimal_on(a, false) ? kGood : pf::optimal_on(a, true) ? kWarn : kMuted, "%s",
                           pf::optimal_on(a, false)  ? "optimal"
                           : pf::optimal_on(a, true) ? "uniform cost only"
                                                     : "not optimal");
    }
    ImGui::PopStyleVar();
    ImGui::Dummy({0, 4});
    ImGui::PushStyleColor(ImGuiCol_Button, {0.31f, 0.27f, 0.90f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.39f, 0.40f, 0.95f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.26f, 0.22f, 0.80f, 1.0f});
    const bool go = half_button("Run   Space");
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    const bool all = ImGui::Button("Compare all   A", {-1, 32});
    if (go) run(s);
    if (all) compare_all(s);

    // Result
    section(s.comparison.empty() ? "RESULT" : "ALL FIVE ON THIS GRID");
    if (!s.comparison.empty()) {
        comparison_table(s);
    } else if (s.phase == Phase::Idle) {
        ImGui::TextColored(kMuted, "Press Run to search, or Compare all.");
    } else {
        const auto& r = s.result;
        if (ImGui::BeginTable("result", 2, ImGuiTableFlags_SizingStretchProp)) {
            row("Outcome", r.found ? "path found" : "no path: goal walled off");
            if (r.found) row("Path cost", thousands(r.cost) + "   (" + thousands(static_cast<long long>(r.path.size())) + " cells)");
            row("Cells expanded", thousands(static_cast<long long>(r.nodes_expanded())));
            char ms[32];
            std::snprintf(ms, sizeof ms, "%.3f ms", s.search_ms);
            row("Search time", ms);
            ImGui::EndTable();
        }
    }

    // Replay
    section("REPLAY");
    const auto& r = s.result;
    const float progress = s.phase == Phase::Idle || r.expanded.empty()
                               ? 0.0f
                               : static_cast<float>(s.revealed / static_cast<double>(r.expanded.size()));
    char overlay[48];
    std::snprintf(overlay, sizeof overlay, "%.0f%% of %s cells", 100.0 * progress,
                  thousands(static_cast<long long>(r.expanded.size())).c_str());
    ImGui::ProgressBar(progress, {-1, 20}, s.phase == Phase::Idle ? "" : overlay);
    const float button = 74;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2 * (button + ImGui::GetStyle().ItemSpacing.x));
    ImGui::SliderFloat("##speed", &s.speed, 50.0f, 50000.0f, "%.0f cells/s", ImGuiSliderFlags_Logarithmic);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Replay speed");
    ImGui::SameLine();
    if (ImGui::Button(s.paused ? "Resume" : "Pause", {button, 0})) s.paused = !s.paused;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("P");
    ImGui::SameLine();
    if (ImGui::Button("Skip", {button, 0})) skip_to_end(s);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skip to the end (E)");

    // Grid
    section("GRID");
    const char* generators[] = {"Empty", "Perfect maze", "Random walls", "Weighted terrain"};
    int gen = static_cast<int>(s.settings.generator);
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    ImGui::SetNextItemWidth(half);
    bool regenerate = ImGui::Combo("##generator", &gen, generators, 4);
    s.settings.generator = static_cast<Generator>(gen);
    int cols = s.settings.cols;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderInt("##size", &cols, 15, 151, "%d columns")) {
        s.settings.cols = cols | 1;
        s.settings.rows = (s.settings.cols * 2 / 3) | 1;
        regenerate = true;
    }
    if (s.settings.generator == Generator::RandomWalls || s.settings.generator == Generator::Terrain) {
        ImGui::SetNextItemWidth(-1);
        regenerate |= ImGui::SliderFloat("##density", &s.settings.density, 0.0f, 0.5f, "%.2f of cells are walls");
    }
    ImGui::SetNextItemWidth(half);
    regenerate |= ImGui::InputInt("##seed", &s.settings.seed, 1, 10);
    ImGui::SameLine();
    if (ImGui::Button("New seed", {-1, 0})) {
        static std::mt19937 rng{std::random_device{}()};
        s.settings.seed = static_cast<int>(rng() % 100000);
        regenerate = true;
    }
    if (regenerate) generate(s);

    // Drawing
    section("DRAW");
    int tool = static_cast<int>(s.tool);
    ImGui::RadioButton("Wall", &tool, 0);
    ImGui::SameLine(0, 18);
    ImGui::RadioButton("Mud", &tool, 1);
    ImGui::SameLine(0, 18);
    ImGui::RadioButton("Erase", &tool, 2);
    s.tool = static_cast<Tool>(tool);
    if (s.tool == Tool::Mud) {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##mud", &s.mud_cost, 2, 9, "mud costs %d to cross");
    }
    ImGui::TextColored(kMuted, "Drag on the grid; right-drag erases.\nDrag a marker to move start or goal.");
    ImGui::End();
}

void draw_overlay(const State& s, const Layout& L) {
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    const float bottom = L.origin.y + L.cell * static_cast<float>(s.grid.height()) + 8;

    // Legend, under the grid.
    ImGui::SetNextWindowPos({L.origin.x - 8, bottom});
    ImGui::Begin("legend", nullptr, flags);
    auto swatch = [](ImU32 colour, const char* label, bool round = false) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        auto* dl = ImGui::GetWindowDrawList();
        if (round) dl->AddCircleFilled({p.x + 6, p.y + 10}, 6, colour);
        else dl->AddRectFilled({p.x, p.y + 4}, {p.x + 12, p.y + 16}, colour, 3);
        ImGui::Dummy({14, 18});
        ImGui::SameLine();
        ImGui::TextColored(kMuted, "%s", label);
        ImGui::SameLine(0, 18);
    };
    swatch(IM_COL32(79, 70, 229, 255), "expanded first");
    swatch(IM_COL32(34, 211, 238, 255), "expanded last");
    swatch(IM_COL32(251, 191, 36, 255), "path");
    swatch(IM_COL32(146, 84, 30, 255), "costly terrain");
    swatch(IM_COL32(16, 185, 129, 255), "start", true);
    swatch(IM_COL32(244, 63, 94, 255), "goal", true);
    ImGui::End();

    // Hovered cell, above the grid's top-left corner.
    if (s.hover) {
        ImGui::SetNextWindowPos({L.origin.x - 8, L.origin.y - 34});
        ImGui::Begin("hover", nullptr, flags);
        const auto c = s.grid.cost(*s.hover);
        if (c == pf::Grid::kWall) ImGui::TextColored(kMuted, "(%d, %d)   wall", s.hover->x, s.hover->y);
        else ImGui::TextColored(kMuted, "(%d, %d)   costs %d to enter", s.hover->x, s.hover->y, c);
        ImGui::End();
    }
}

}  // namespace app
