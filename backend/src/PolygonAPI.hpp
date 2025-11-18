#pragma once
#include <string>
#include <vector>

struct Bar {
    double close;
    double volume;
};

class PolygonAPI {
public:
    explicit PolygonAPI(const std::string& key) : apiKey(key) {}
    std::vector<Bar> getBars(const std::string& ticker);
    double getLastPrice(const std::string& ticker);

private:
    std::string apiKey;
};
