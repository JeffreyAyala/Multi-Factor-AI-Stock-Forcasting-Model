#pragma once
#include <string>
#include <vector>

// ==========================================================
// Basic bar structure returned from Polygon:
//   close  = last price of the minute
//   volume = volume of that minute
// ----------------------------------------------------------
struct Bar {
    double close = 0.0;
    double volume = 0.0;
};

// ==========================================================
// PolygonAPI wrapper (Starter-plan compatible)
// Provides:
//   • getBars()       → last 1-minute candles (2 trading days)
//   • getLastPrice()  → snapshot last trade / day close
// ----------------------------------------------------------
class PolygonAPI {
public:
    explicit PolygonAPI(const std::string& key)
        : apiKey(key)
    {}

    // Retrieve 1-minute bars for charts & ML features
    std::vector<Bar> getBars(const std::string& ticker);

    // Get the latest available trading price (delayed on Starter plan)
    double getLastPrice(const std::string& ticker);

private:
    std::string apiKey;
};

