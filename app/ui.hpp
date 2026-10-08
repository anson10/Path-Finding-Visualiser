// A small immediate-mode widget set drawn with SFML, in the visualiser's own style: flat grey
// buttons, the blue accent for anything selected, Arvo text. Each call draws one widget at
// the cursor, moves the cursor down and reports whether the user changed something.
#pragma once

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

namespace ui {

struct Input {
    sf::Vector2f mouse{-1, -1};
    bool pressed = false;  // left button went down this frame
    bool down = false;     // left button is held
};

namespace colors {
const sf::Color Panel(50, 50, 50);
const sf::Color Button(70, 70, 70);
const sf::Color ButtonHover(88, 88, 88);
const sf::Color Accent(0, 140, 210);
const sf::Color AccentHover(20, 160, 230);
const sf::Color Track(34, 34, 34);
const sf::Color Text(255, 255, 255);
const sf::Color Dim(170, 170, 170);
const sf::Color Rule(70, 70, 70);
const sf::Color Good(120, 220, 120);
const sf::Color Warning(255, 180, 0);
}  // namespace colors

class Ui {
public:
    // `active` remembers which slider is being dragged between frames.
    Ui(sf::RenderTarget& target, const sf::Font& font, const Input& input, int& active, sf::Vector2f origin, float width);

    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return width_; }
    void set_y(float y) { y_ = y; }
    void gap(float h) { y_ += h; }

    // Text without moving the cursor (dx, dy relative to it).
    void text(const std::string& s, unsigned size, sf::Color colour, float dx = 0, float dy = 0);
    void heading(const std::string& s);  // a section title with a rule under it
    void line(const std::string& s, sf::Color colour = colors::Text, unsigned size = 17);

    bool button(const std::string& label, bool selected = false, float w = 0, float h = 36, unsigned size = 18);
    // Buttons side by side, equal widths; returns the index clicked or -1.
    int button_row(const std::vector<std::string>& labels, int selected = -1, float h = 34, unsigned size = 16);
    bool tabs(const std::vector<std::string>& labels, int& current);
    bool segmented(const std::vector<std::string>& labels, int& value);
    bool toggle(const std::string& label, bool& value);
    bool slider(const std::string& label, float& value, float lo, float hi, const std::string& shown, bool logarithmic = false);
    bool slider(const std::string& label, int& value, int lo, int hi, int step = 1);

    // A tooltip for the last widget, drawn at the end of the frame.
    void tooltip(const std::string& s);
    void finish();

private:
    bool hovered(sf::FloatRect r) const { return r.contains(input_.mouse); }
    void rect(sf::FloatRect r, sf::Color c);
    float measure(const std::string& s, unsigned size) const;
    int id(const std::string& key) const;

    sf::RenderTarget& target_;
    const sf::Font& font_;
    const Input& input_;
    int& active_;
    float x_;
    float y_;
    float width_;
    sf::FloatRect last_{};
    std::string tip_;
};

}  // namespace ui
