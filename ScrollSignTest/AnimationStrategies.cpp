// AnimationStrategies.cpp: Implementation of animation strategies for ScrollSignTest.
// Each AnimationStrategy subclass implements a different text animation effect.
// Only non-scroll animations are used if the text fits on the display; otherwise, ScrollAnimation is used.
#include "AnimationStrategies.h"
#include "graphics.h"
#include "led-matrix.h"
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <random>
#include <string>

// ScrollAnimation: Scrolls the text horizontally across the canvas. Used for long messages.
void ScrollAnimation::render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) {
    char buf[1024];
    strncpy(buf, text.c_str(), sizeof(buf));
    buf[sizeof(buf)-1] = 0;
    int draw_len = static_cast<int>(text.size()) * 9; // approximate width
    int prev_x = canvas->width();
    for (int r = 0; r < 2; ++r) {
        for (int x = canvas->width(); x > -draw_len - 40; --x) {
            // Erase previous text by overdrawing with black
            if (x != prev_x) {
                rgb_matrix::DrawText(canvas, font, prev_x, y + font.baseline(), Color(0,0,0), nullptr, buf);
            }
            rgb_matrix::DrawText(canvas, font, x, y + font.baseline(), color, nullptr, buf);
            usleep(speed_ms * 1000);
            prev_x = x;
        }
    }
}

// BlinkAnimation: Blinks the text in place several times.
void BlinkAnimation::render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int /*speed_ms*/) {
    char buf[1024];
    strncpy(buf, text.c_str(), sizeof(buf));
    buf[sizeof(buf)-1] = 0;
    int draw_len = static_cast<int>(text.size()) * 9;
    int start_x = (int)round((canvas->width() - draw_len) / 2.0);
    for (int i = 0; i < 6; ++i) {
        rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), color, nullptr, buf);
        usleep(1000 * 3000);
        rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), Color(0,0,0), nullptr, buf);
        usleep(500 * 2000);
    }
}

// FadeAnimation: Fades the text in and out at the center of the display.
void FadeAnimation::render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) {
    char buf[1024];
    strncpy(buf, text.c_str(), sizeof(buf));
    buf[sizeof(buf)-1] = 0;
    int draw_len = static_cast<int>(text.size()) * 9;
    int start_x = (int)round((canvas->width() - draw_len) / 2.0);
    int min_duration_ms = 10000; // 10 seconds
    int fade_steps = 22; // 11 in, 11 out
    int cycle_time_ms = fade_steps * speed_ms;
    int cycles = std::max(1, min_duration_ms / std::max(1, cycle_time_ms));
    for (int c = 0; c < cycles; ++c) {
        for (int step = 0; step <= 10; ++step) {
            Color fadeColor(
                (uint8_t)(color.r * step / 10),
                (uint8_t)(color.g * step / 10),
                (uint8_t)(color.b * step / 10)
            );
            rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), fadeColor, nullptr, buf);
            usleep(speed_ms * 3000);
        }
        for (int step = 10; step >= 0; --step) {
            Color fadeColor(
                (uint8_t)(color.r * step / 10),
                (uint8_t)(color.g * step / 10),
                (uint8_t)(color.b * step / 10)
            );
            rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), fadeColor, nullptr, buf);
            usleep(speed_ms * 3000);
        }
    }
}

// WaveAnimation: Animates the text with a sine wave effect, making each character move up and down.
void WaveAnimation::render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) {
    int base_x = (int)round((canvas->width() - text.size() * 9) / 2.0);
    int min_duration_ms = 10000; // 10 seconds
    int frames = std::max(32, min_duration_ms / std::max(1, speed_ms));
    for (int frame = 0; frame < frames; ++frame) {
        for (size_t i = 0; i < text.size(); ++i) {
            int char_x = base_x + (int)(i * 9);
            int wave_y = y + font.baseline() + (int)(3 * sin((frame + i) * 0.5));
            rgb_matrix::DrawText(canvas, font, char_x, wave_y, color, nullptr, string(1, text[i]).c_str());
        }
        usleep(speed_ms * 3000);
        // Erase by overdrawing with black
        for (size_t i = 0; i < text.size(); ++i) {
            int char_x = base_x + (int)(i * 9);
            int wave_y = y + font.baseline() + (int)(3 * sin((frame + i) * 0.5));
            rgb_matrix::DrawText(canvas, font, char_x, wave_y, Color(0,0,0), nullptr, string(1, text[i]).c_str());
        }
    }
}

// BounceAnimation: Moves the text horizontally, bouncing off the display edges.
void BounceAnimation::render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) {
    char buf[1024];
    strncpy(buf, text.c_str(), sizeof(buf));
    buf[sizeof(buf)-1] = 0;
    int draw_len = static_cast<int>(text.size()) * 9;
    int min_x = 0, max_x = canvas->width() - draw_len;
    int x = min_x, dx = 2;
    int bounce_frames = 2 * (max_x - min_x);
    int min_duration_ms = 10000; // 10 seconds
    int frames = std::max(bounce_frames, min_duration_ms / std::max(1, speed_ms));
    for (int frame = 0; frame < frames; ++frame) {
        rgb_matrix::DrawText(canvas, font, x, y + font.baseline(), color, nullptr, buf);
        usleep(speed_ms * 3000);
        rgb_matrix::DrawText(canvas, font, x, y + font.baseline(), Color(0,0,0), nullptr, buf);
        x += dx;
        if (x <= min_x || x >= max_x) dx = -dx;
    }
}

// TypewriterAnimation: Reveals the text one character at a time, simulating typing.
void TypewriterAnimation::render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) {
    int start_x = (int)round((canvas->width() - text.size() * 9) / 2.0);
    string shown;
    for (size_t i = 0; i < text.size(); ++i) {
        shown += text[i];
        rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), color, nullptr, shown.c_str());
        usleep(speed_ms * 3000);
        rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), Color(0,0,0), nullptr, shown.c_str());
    }
    // Show full text at end
    rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), color, nullptr, text.c_str());
    usleep(1000 * 5000);
    rgb_matrix::DrawText(canvas, font, start_x, y + font.baseline(), Color(0,0,0), nullptr, text.c_str());
}

// DiagonalSlideAnimation: Slides the text into place from a random bottom or top corner depending on row.
void DiagonalSlideAnimation::render(Canvas *canvas, Font &font, const string &text, int y, const Color &color, int speed_ms) {
    int draw_len = static_cast<int>(text.size()) * 9;
    int final_x = (int)round((canvas->width() - draw_len) / 2.0);
    int final_y = y + font.baseline();
    // Determine if this is the top or bottom row
    bool fromTop = (y == 0);
    std::random_device rd; std::mt19937 gen(rd());
    int direction;
    if (fromTop) {
        // Top row: randomly choose top-left or top-right
        direction = std::uniform_int_distribution<int>(0,1)(gen);
    } else {
        // Bottom row: randomly choose bottom-left or bottom-right
        direction = std::uniform_int_distribution<int>(2,3)(gen);
    }
    int start_x, start_y;
    switch (direction) {
        case 0: // top-left
            start_x = -draw_len; start_y = -font.height(); break;
        case 1: // top-right
            start_x = canvas->width(); start_y = -font.height(); break;
        case 2: // bottom-left
            start_x = -draw_len; start_y = canvas->height() + font.height(); break;
        case 3: // bottom-right
            start_x = canvas->width(); start_y = canvas->height() + font.height(); break;
        default:
            start_x = -draw_len; start_y = -font.height(); break;
    }
    int steps = 20;
    for (int step = 0; step <= steps; ++step) {
        float t = step / (float)steps;
        int curr_x = (int)(start_x + t * (final_x - start_x));
        int curr_y = (int)(start_y + t * (final_y - start_y));
        rgb_matrix::DrawText(canvas, font, curr_x, curr_y, color, nullptr, text.c_str());
        usleep(speed_ms * 1000);
        rgb_matrix::DrawText(canvas, font, curr_x, curr_y, Color(0,0,0), nullptr, text.c_str());
    }
    // Draw final position
    rgb_matrix::DrawText(canvas, font, final_x, final_y, color, nullptr, text.c_str());
    usleep(1000 * 1000);
    rgb_matrix::DrawText(canvas, font, final_x, final_y, Color(0,0,0), nullptr, text.c_str());
}

// chooseStrategy: Selects an animation strategy based on whether the text fits.
// If the text does not fit, always uses ScrollAnimation. Otherwise, randomly selects from all available non-scroll strategies.
std::unique_ptr<AnimationStrategy> chooseStrategy(bool fits, std::mt19937 &gen) {
    if (!fits) {
        // If text doesn't fit, always use ScrollAnimation
        return std::unique_ptr<AnimationStrategy>(new ScrollAnimation());
    }
    // Non-scroll animations implemented in this file.
    // 0: Blink, 1: Fade, 2: Wave, 3: Bounce, 4: Typewriter, 5: DiagonalSlide
    std::uniform_int_distribution<int> dist(0,5);
    int r = dist(gen);
    switch (r) {
        case 0: return std::unique_ptr<AnimationStrategy>(new BlinkAnimation());
        case 1: return std::unique_ptr<AnimationStrategy>(new FadeAnimation());
        case 2: return std::unique_ptr<AnimationStrategy>(new WaveAnimation());
        case 3: return std::unique_ptr<AnimationStrategy>(new BounceAnimation());
        case 4: return std::unique_ptr<AnimationStrategy>(new TypewriterAnimation());
        case 5: return std::unique_ptr<AnimationStrategy>(new DiagonalSlideAnimation());
        default: return std::unique_ptr<AnimationStrategy>(new BlinkAnimation());
    }
}
