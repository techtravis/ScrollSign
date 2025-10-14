#pragma once
#include <string>
#include <vector>

// Aggregates all message sources (RSS, static text, events) for a given position.
class MessageAggregator {
public:
    // Fetch all messages for position ("top" or "bottom") applying regex filtering for RSS titles.
    // Returns combined list including error lines.
    std::vector<std::string> fetchAll(const std::string &position, const std::string &regexStr);
};
