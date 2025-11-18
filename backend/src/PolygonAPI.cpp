#include "PolygonAPI.hpp"
#include "HttpClient.hpp"
#include "json.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

// Helper: format YYYY-MM-DD for "today - daysAgo"
namespace {
    std::string dateFromOffset(int daysAgo) {
        using namespace std::chrono;

        system_clock::time_point tp = system_clock::now() - hours(24 * daysAgo);
        std::time_t tt = system_clock::to_time_t(tp);

        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        tm = *std::localtime(&tt);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d");
        return oss.str();
    }
}

// Use 1-minute bars over the last 2 calendar days for fresher data
std::vector<Bar> PolygonAPI::getBars(const std::string& ticker) {
    // from = 2 days ago, to = today
    std::string to = dateFromOffset(0);  // today
    std::string from = dateFromOffset(2);  // 2 days ago

    // 1-minute bars instead of 5-minute
    std::string url = "https://api.polygon.io/v2/aggs/ticker/" + ticker +
        "/range/1/minute/" + from + "/" + to +
        "?adjusted=true&sort=asc&limit=2000&apiKey=" + apiKey;

    std::string res = HttpClient::Get(url);
    std::vector<Bar> bars;
    if (res.size() < 20) return bars;

    size_t pos = 0;
    while (true) {
        size_t cpos = res.find("\"c\":", pos);
        if (cpos == std::string::npos) break;
        size_t vpos = res.find("\"v\":", cpos);
        if (vpos == std::string::npos) break;

        double close = mini_json::extract_number_after(res, cpos + 4);
        double volume = mini_json::extract_number_after(res, vpos + 4);

        bars.push_back({ close, volume });
        pos = vpos + 4;
    }

    return bars;
}

// "Current" price = latest 1-minute close from Polygon aggregates
double PolygonAPI::getLastPrice(const std::string& ticker) {
    auto bars = getBars(ticker);
    if (!bars.empty()) {
        return bars.back().close;   // most recent 1-minute bar
    }
    return 0.0;
}



