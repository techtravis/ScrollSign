// AnimationStrategies.h: Animation strategy abstractions for ScrollSignTest.
#pragma once
#include "graphics.h"
#include "led-matrix.h"
#include <string>
#include <memory>
#include <random>

using namespace rgb_matrix;
using std::string;

// Abstract base class for animation strategies.
class AnimationStrategy {
public:
    virtual ~AnimationStrategy() {}
    // Renders the text on the canvas using the given font, color, and speed.
    virtual void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) = 0;
};

// Scrolls the text horizontally across the canvas.
class ScrollAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Blinks the text in place several times.
class BlinkAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Fade In/Out Animation
class FadeAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Wave Animation
class WaveAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Bounce Animation
class BounceAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Typewriter Animation
class TypewriterAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Flip Animation
class FlipAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Diagonal Slide Animation
class DiagonalSlideAnimation : public AnimationStrategy {
public:
    void render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) override;
};

// Chooses an animation strategy based on whether the text fits and randomness.
std::unique_ptr<AnimationStrategy> chooseStrategy(bool fits, std::mt19937 &gen);
