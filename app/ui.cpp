#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ui {

Ui::Ui(sf::RenderTarget& target, const sf::Font& font, const Input& input, int& active, sf::Vector2f origin,
       float width)
    : target_(target), font_(font), input_(input), active_(active), x_(origin.x), y_(origin.y), width_(width) {
    if (!input_.down) active_ = -1;  // a drag ends when the button is released
}

void Ui::rect(sf::FloatRect r, sf::Color c) {
    sf::RectangleShape shape({r.width, r.height});
    shape.setPosition(r.left, r.top);
    shape.setFillColor(c);
    target_.draw(shape);
}

float Ui::measure(const std::string& s, unsigned size) const {
    return sf::Text(s, font_, size).getLocalBounds().width;
}

int Ui::id(const std::string& key) const { return static_cast<int>(std::hash<std::string>{}(key) & 0x7fffffff); }

void Ui::text(const std::string& s, unsigned size, sf::Color colour, float dx, float dy) {
    sf::Text t(s, font_, size);
    t.setPosition(std::round(x_ + dx), std::round(y_ + dy));
    t.setFillColor(colour);
    target_.draw(t);
}

void Ui::heading(const std::string& s) {
    text(s, 15, colors::Dim);
    rect({x_, y_ + 22, width_, 1}, colors::Rule);
    y_ += 32;
}

void Ui::line(const std::string& s, sf::Color colour, unsigned size) {
    text(s, size, colour);
    y_ += static_cast<float>(size) + 7;
}

bool Ui::button(const std::string& label, bool selected, float w, float h, unsigned size) {
    const sf::FloatRect r(x_, y_, w > 0 ? w : width_, h);
    const bool over = hovered(r);
    rect(r, selected ? (over ? colors::AccentHover : colors::Accent) : over ? colors::ButtonHover : colors::Button);
    text(label, size, colors::Text, 10, (h - static_cast<float>(size)) / 2 - 3);
    last_ = r;
    y_ += h + 8;
    return over && input_.pressed;
}

int Ui::button_row(const std::vector<std::string>& labels, int selected, float h, unsigned size) {
    const float n = static_cast<float>(labels.size());
    const float w = (width_ - 8 * (n - 1)) / n;
    const float x0 = x_, y0 = y_;
    int clicked = -1;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        x_ = x0 + static_cast<float>(i) * (w + 8);
        y_ = y0;
        const sf::FloatRect r(x_, y_, w, h);
        const bool over = hovered(r);
        const bool sel = static_cast<int>(i) == selected;
        rect(r, sel ? colors::Accent : over ? colors::ButtonHover : colors::Button);
        const float tw = measure(labels[i], size);
        text(labels[i], size, colors::Text, (w - tw) / 2, (h - static_cast<float>(size)) / 2 - 3);
        if (over && input_.pressed) clicked = static_cast<int>(i);
    }
    x_ = x0;
    y_ = y0 + h + 8;
    return clicked;
}

bool Ui::tabs(const std::vector<std::string>& labels, int& current) {
    const float n = static_cast<float>(labels.size());
    const float w = width_ / n;
    bool changed = false;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const sf::FloatRect r(x_ + static_cast<float>(i) * w, y_, w, 34);
        const bool sel = static_cast<int>(i) == current;
        const bool over = hovered(r);
        if (over && input_.pressed && !sel) {
            current = static_cast<int>(i);
            changed = true;
        }
        const float tw = measure(labels[i], 18);
        text(labels[i], 18, sel ? colors::Text : over ? sf::Color(220, 220, 220) : colors::Dim,
             static_cast<float>(i) * w + (w - tw) / 2, 4);
        rect({r.left, y_ + 31, w, sel ? 3.0f : 1.0f}, sel ? colors::Accent : colors::Rule);
    }
    y_ += 46;
    return changed;
}

bool Ui::segmented(const std::vector<std::string>& labels, int& value) {
    const int clicked = button_row(labels, value, 30, 15);
    if (clicked < 0 || clicked == value) return false;
    value = clicked;
    return true;
}

bool Ui::toggle(const std::string& label, bool& value) {
    const sf::FloatRect r(x_, y_, width_, 26);
    const bool over = hovered(r);
    const sf::FloatRect sw(x_ + width_ - 44, y_ + 3, 44, 20);
    rect(sw, value ? colors::Accent : colors::Track);
    const float knob_x = value ? sw.left + sw.width - 18 : sw.left + 2;
    rect({knob_x, sw.top + 2, 16, 16}, over ? sf::Color(240, 240, 240) : sf::Color(220, 220, 220));
    text(label, 17, colors::Text, 0, 1);
    last_ = r;
    y_ += 34;
    if (over && input_.pressed) {
        value = !value;
        return true;
    }
    return false;
}

bool Ui::slider(const std::string& label, float& value, float lo, float hi, const std::string& shown, bool logarithmic) {
    const int me = id(label);
    text(label, 16, colors::Dim);
    const float vw = measure(shown, 16);
    text(shown, 16, colors::Text, width_ - vw);
    const sf::FloatRect track(x_, y_ + 28, width_, 6);
    const sf::FloatRect hit(x_ - 4, y_ + 18, width_ + 8, 26);
    auto to_t = [&](float v) {
        return logarithmic ? (std::log(v) - std::log(lo)) / (std::log(hi) - std::log(lo)) : (v - lo) / (hi - lo);
    };
    auto from_t = [&](float t) {
        return logarithmic ? std::exp(std::log(lo) + t * (std::log(hi) - std::log(lo))) : lo + t * (hi - lo);
    };
    bool changed = false;
    if (hovered(hit) && input_.pressed) active_ = me;
    if (active_ == me && input_.down) {
        const float t = std::clamp((input_.mouse.x - track.left) / track.width, 0.0f, 1.0f);
        const float v = from_t(t);
        changed = v != value;
        value = v;
    }
    const float t = std::clamp(to_t(value), 0.0f, 1.0f);
    rect(track, colors::Track);
    rect({track.left, track.top, track.width * t, track.height}, colors::Accent);
    const bool hot = active_ == me || hovered(hit);
    const float k = hot ? 16.0f : 14.0f;
    rect({track.left + track.width * t - k / 2, track.top + 3 - k / 2, k, k}, sf::Color(235, 235, 235));
    last_ = hit;
    y_ += 52;
    return changed;
}

bool Ui::slider(const std::string& label, int& value, int lo, int hi, int step) {
    float v = static_cast<float>(value);
    slider(label, v, static_cast<float>(lo), static_cast<float>(hi), std::to_string(value));
    int snapped = lo + static_cast<int>(std::lround((v - static_cast<float>(lo)) / static_cast<float>(step))) * step;
    snapped = std::clamp(snapped, lo, hi);
    if (snapped == value) return false;
    value = snapped;
    return true;
}

void Ui::tooltip(const std::string& s) {
    if (last_.contains(input_.mouse)) tip_ = s;
}

void Ui::finish() {
    if (tip_.empty()) return;
    const float w = measure(tip_, 15) + 20;
    const float x = std::min(input_.mouse.x + 14, static_cast<float>(target_.getSize().x) - w - 6);
    const sf::FloatRect r(x, input_.mouse.y + 18, w, 28);
    rect(r, sf::Color(25, 25, 25, 240));
    sf::Text t(tip_, font_, 15);
    t.setPosition(std::round(r.left + 10), std::round(r.top + 4));
    t.setFillColor(colors::Text);
    target_.draw(t);
}

}  // namespace ui
