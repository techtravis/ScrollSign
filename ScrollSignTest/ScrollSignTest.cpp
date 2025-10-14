// ScrollSignTest.cpp: Main application logic for ScrollSignTest.
// Displays RSS and static messages on an RGB LED matrix using animation strategies.
// Refactored to use MessageAggregator and AnimationStrategy abstractions.
// Public domain (original parts); depends on GPLv2 led-matrix library.

#include "graphics.h"
#include "led-matrix.h"
#include "pugixml.hpp"
#include "MessageSources.h"
#include "AnimationStrategies.h"

#include <getopt.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <thread>
#include <algorithm>

using namespace rgb_matrix;
using std::string;

int isDebug = 0;

// Prints usage information for the program.
static int usage(const char *progname)
{
    fprintf(stderr, "usage: %s [options]\n", progname);
    fprintf(stderr, "Reads text from RSS feeds and static XML lines and scrolls them.\n");
    fprintf(stderr, "Options:\n");
    rgb_matrix::PrintMatrixFlags(stderr);
    fprintf(stderr,
            "\t-f <font-file>    : Use given BDF font.\n"
            "\t-b <brightness>   : Brightness 1..100 (default 100).\n"
            "\t-C <r,g,b>        : Fixed text color. Default random.\n"
            "\t-B <r,g,b>        : Background color (currently unused).\n");
    return 1;
}

// Parses a color from a string in the format "r,g,b".
static bool parseColor(Color *c, const char *str)
{
    return sscanf(str, "%hhu,%hhu,%hhu", &c->r, &c->g, &c->b) == 3;
}

// Trims leading and trailing spaces from a string.
static string trim(const string &str)
{
    size_t first = str.find_first_not_of(' ');
    if (first == string::npos) return str;
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, last - first + 1);
}

// Returns the current time as a formatted string for display.
static string currentTime()
{
    time_t now = time(nullptr);
    struct tm tstruct; tstruct = *localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), " %I:%M %p", &tstruct);
    return buf;
}

// Reads the regex string from Settings.xml for filtering messages.
// Updated to look inside Configs child folder where resources are copied on build.
static string getRegexStr()
{
	const char *paths[] = {
		"Configs/Settings.xml",		   // running from binary dir
		"./Configs/Settings.xml",				   // explicit relative
        "Debug/Configs/Settings.xml",      // running from project root (Debug build)
        "Release/Configs/Settings.xml"     // running from project root (Release build)
    };
    pugi::xml_document doc;
    for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) {
        if (doc.load_file(paths[i])) {
            pugi::xml_node root = doc.child("settings");
            return trim(root.child("regex").child("string").child_value());
        }
    }
    return string();
}

// Thread worker for displaying feeds/messages on one row of the matrix.
static void displayFeeds(RGBMatrix *&canvas, const string &position, const string &fontFile, const Color &fixedColor, bool useFixedColor)
{
    Font font;
    font.LoadFont(fontFile.c_str());

    int y = (position == "top") ? 0 : 16;
    int speed_ms = (position == "top") ? 11 : 14;

    MessageAggregator aggregator;
    std::random_device rd; std::mt19937 gen(rd());

    time_t lastFetch; time(&lastFetch);
    bool firstPass = true;
    std::vector<string> messages;

    while (true) {
        // Periodically fetch new messages.
        time_t now; time(&now);
        double elapsed = difftime(now, lastFetch);
        if (firstPass || elapsed > 120.0) {
            lastFetch = now; firstPass = false;
            string regexStr = getRegexStr();
            messages = aggregator.fetchAll(position, regexStr);
            if (position == "bottom") {
                string timeStr = currentTime();
                for (auto &m : messages) {
                    int msg_len = (int)m.size() * 9;
                    int msg_time_len = (int)(m.size() + timeStr.size()) * 9;
                    if (msg_len < (canvas->width() - 1) && msg_time_len >= (canvas->width() - 1)) {
                        // Only add time if original message already overflows
                        continue;
                    }
                    m += timeStr;
                }
            }
            std::shuffle(messages.begin(), messages.end(), gen);
            if (isDebug) fprintf(stderr, "Fetched %zu messages for %s row.\n", messages.size(), position.c_str());
        }

        if (messages.empty()) {
            if (isDebug) fprintf(stderr, "No messages for %s row. Sleeping 5s.\n", position.c_str());
            usleep(5 * 1000 * 1000);
            continue;
        }

        // Display each message using the chosen animation strategy.
        for (const auto &msg : messages) {
            int draw_len = (int)msg.size() * 9;
            bool fits = draw_len < (canvas->width() - 1);
            Color drawColor(0,0,0);
            if (useFixedColor) {
                drawColor = fixedColor;
            } else {
                int r = gen() % 255, g = gen() % 255, b = gen() % 255;
                while (r + g + b < 50) { r = gen() % 255; g = gen() % 255; b = gen() % 255; }
                drawColor = Color(r,g,b);
            }
            auto strategy = chooseStrategy(fits, gen);
            strategy->render(canvas, font, msg, y, drawColor, speed_ms);
        }
    }
}

int main(int argc, char *argv[])
{
    // Wait 10 seconds to allow system to boot up
    sleep(20);
    // Create the RGB matrix from command-line flags.
    RGBMatrix *canvas = rgb_matrix::CreateMatrixFromFlags(&argc, &argv);
    if (!canvas) return 1;

    Color textColor(0,0,0); // will be overridden if -C passed.
    Color bgColor(0,0,0);   // currently unused (animations draw over black).
    bool colorSpecified = false;
    string fontPath;
    bool fontSpecified = false;
    int brightness = 100;

    // Parse command-line options.
    int opt;
    while ((opt = getopt(argc, argv, "f:C:B:b:")) != -1) {
        switch (opt) {
        case 'f': fontPath = optarg; fontSpecified = true; break;
        case 'b': brightness = atoi(optarg); break;
        case 'C': if (!parseColor(&textColor, optarg)) return usage(argv[0]); colorSpecified = true; break;
        case 'B': if (!parseColor(&bgColor, optarg)) return usage(argv[0]); break;
        default: return usage(argv[0]);
        }
    }

    if (!fontSpecified) return usage(argv[0]);
    if (brightness < 1 || brightness > 100) {
        fprintf(stderr, "Brightness outside 1..100\n");
        return 1;
    }

    // Validate font file early.
    Font testFont; if (!testFont.LoadFont(fontPath.c_str())) { fprintf(stderr, "Couldn't load font '%s'\n", fontPath.c_str()); return usage(argv[0]); }

    canvas->SetBrightness(brightness);
    canvas->SetPWMBits(8); // reduced color depth for performance

    // Start threads for top and bottom rows.
    std::thread topThread(displayFeeds, std::ref(canvas), "top", fontPath, textColor, colorSpecified);
    std::thread bottomThread(displayFeeds, std::ref(canvas), "bottom", fontPath, textColor, colorSpecified);
    topThread.join();
    bottomThread.join();

    canvas->Clear();
    delete canvas;
    return 0;
}
