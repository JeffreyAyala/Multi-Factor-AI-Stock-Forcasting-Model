#include "PolygonAPI.hpp"
#include "HttpClient.hpp"
#include "json.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>


// ---------------------------------------------------------
// Date helper: returns YYYY-MM-DD for N days ago
// ---------------------------------------------------------
namespace {
    std::string dateFromOffset(int daysAgo)
    {
        using namespace std::chrono;

        system_clock::time_point tp =
            system_clock::now() - hours(24 * daysAgo);

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


// =========================================================
//   1. GET 1-MINUTE BARS — last 2 trading days
// =========================================================
std::vector<Bar> PolygonAPI::getBars(const std::string& ticker)
{
    std::vector<Bar> bars;

    // Polygon Free Tier limitation:
    // We must avoid using today's date.
    // Use last 7 calendar days; sort=desc gives newest first.

    std::string to = dateFromOffset(1);      // yesterday
    std::string from = dateFromOffset(7);    // one week ago

    std::string url =
        "https://api.polygon.io/v2/aggs/ticker/" + ticker +
        "/range/1/minute/" + from + "/" + to +
        "?adjusted=true&sort=desc&limit=5000&apiKey=" + apiKey;

    std::string res = HttpClient::Get(url);

    if (res.size() < 50)
        return bars;

    size_t pos = 0;
    while (true)
    {
        size_t cpos = res.find("\"c\":", pos);
        if (cpos == std::string::npos)
            break;

        size_t vpos = res.find("\"v\":", cpos);
        if (vpos == std::string::npos)
            break;

        double close = mini_json::extract_number_after(res, cpos + 4);
        double volume = mini_json::extract_number_after(res, vpos + 4);

        if (close > 0)
            bars.push_back({ close, volume });

        pos = vpos + 4;
    }

    // we downloaded newest first → reverse so UI shows oldest→newest
    std::reverse(bars.begin(), bars.end());

    // Only need 300 most recent bars
    if (bars.size() > 300)
        bars.erase(bars.begin(), bars.end() - 300);

    return bars;
}



// =========================================================
//   2. GET CURRENT PRICE (Starter-plan compatible)
//      Uses Polygon SNAPSHOT endpoint for last trade / day close
// =========================================================
double PolygonAPI::getLastPrice(const std::string& ticker)
{
    std::string url =
        "https://api.polygon.io/v2/snapshot/locale/us/markets/stocks/tickers/"
        + ticker + "?apiKey=" + apiKey;

    std::string res = HttpClient::Get(url);

    if (res.size() < 30)
        return 0.0;

    // -----------------------------------------
    // Try: lastTrade -> price
    // -----------------------------------------
    size_t lt = res.find("\"lastTrade\"");
    if (lt != std::string::npos)
    {
        size_t ppos = res.find("\"p\":", lt);
        if (ppos != std::string::npos)
        {
            double price = mini_json::extract_number_after(res, ppos + 4);
            if (price > 0)
                return price;
        }
    }

    // -----------------------------------------
    // Fallback: use "day" close
    // -----------------------------------------
    size_t day = res.find("\"day\"");
    if (day != std::string::npos)
    {
        size_t cpos = res.find("\"c\":", day);
        if (cpos != std::string::npos)
        {
            double close = mini_json::extract_number_after(res, cpos + 4);
            if (close > 0)
                return close;
        }
    }

    return 0.0;
}






