// MessageSources.cpp: Implements message aggregation and fetching for ScrollSignTest.
// Handles loading RSS feeds and static lines from XML, applying regex filtering, and fetching via curl.

#include "MessageSources.h"
#include "pugixml.hpp"
#include <regex>
#include <curl/curl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <iostream>

extern int isDebug;

// Callback for libcurl to write received data into a string buffer.
static size_t WriteCurlCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

// Trims leading and trailing spaces from a string.
static std::string trim(const std::string &str)
{
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

// Resolve a resource file path inside the Configs folder with fallbacks for different run locations.
static std::string resolveConfigPath(const std::string &fileName)
{
    const char *prefixes[] = {
        "Configs/",          // running from binary dir
        "./Configs/",        // explicit relative
        "Debug/Configs/",    // running from project root (Debug)
        "Release/Configs/",  // running from project root (Release)
        "/home/pi/"          // legacy absolute path fallback
    };
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        std::string candidate = std::string(prefixes[i]) + fileName;
        if (access(candidate.c_str(), R_OK) == 0) return candidate;
    }
    return fileName; // last resort
}

// Loads items from an XML file, extracting child values under a given root.
static std::vector<std::string> loadXmlItems(const std::string &path, const char *rootName, const char *childName)
{
    std::vector<std::string> results;
    pugi::xml_document doc;
    pugi::xml_parse_result r = doc.load_file(path.c_str());
    if (!r) return results;
    pugi::xml_node root = doc.child(rootName);
    for (pugi::xml_node n = root.child("item"); n; n = n.next_sibling("item")) {
        std::string val = n.child(childName).child_value();
        results.push_back(trim(val));
    }
    return results;
}

// Gets feed URLs for the specified position (top/bottom) from XML.
static std::vector<std::string> getFeedUrls(const std::string &position)
{
    if (position == "top") return loadXmlItems(resolveConfigPath("TopFeeds.xml"), "feeds", "url");
    if (position == "bottom") return loadXmlItems(resolveConfigPath("BottomFeeds.xml"), "feeds", "url");
    return {};    
}

// Gets static message lines for the specified position (top/bottom) from XML.
static std::vector<std::string> getStaticLines(const std::string &position)
{
    if (position == "top") return loadXmlItems(resolveConfigPath("TopLines.xml"), "lines", "text");
    if (position == "bottom") return loadXmlItems(resolveConfigPath("BottomLines.xml"), "lines", "text");
    return {};    
}

// Fetches the content of a URL using libcurl. Returns error string if any.
static std::string fetchUrl(const std::string &url, std::string &err)
{
    err.clear();
    CURL *curl = curl_easy_init();
    std::string buf;
    if (!curl) { err = "Curl init failed"; return buf; }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCurlCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ScrollSignTest/1.0");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        err = curl_easy_strerror(res);
    }
    curl_easy_cleanup(curl);
    return buf;
}

// Aggregates messages from RSS feeds and static lines, applying regex filtering.
std::vector<std::string> MessageAggregator::fetchAll(const std::string &position, const std::string &regexStr)
{
    std::vector<std::string> out;
    std::regex rx(regexStr);

    // Fetch and process RSS feeds for the given position.
    auto urls = getFeedUrls(position);
    for (auto &u : urls) {
        std::string err; std::string xml = fetchUrl(u, err);
        if (!err.empty()) { out.push_back("[Feed Error] " + u + " - " + err); continue; }
        if (xml.empty()) { out.push_back("[Feed Error] " + u + " - Empty response"); continue; }
        pugi::xml_document doc; pugi::xml_parse_result r = doc.load_buffer(xml.c_str(), xml.size());
        if (!r) { out.push_back("[Parse Error] " + u + " - " + r.description()); continue; }
        pugi::xml_node channel = doc.child("rss").child("channel");
        for (pugi::xml_node item = channel.child("item"); item; item = item.next_sibling("item")) {
            std::string title = item.child("title").child_value();
            title = std::regex_replace(title, rx, "");
            out.push_back(trim(title));
        }
    }

    // Add static lines for the given position.
    auto statics = getStaticLines(position);
    out.insert(out.end(), statics.begin(), statics.end());

    return out;
}
