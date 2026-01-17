#pragma once
#include <string>

// -------------------------------------------------------------
// HttpClient
//   Lightweight WinINet wrapper for GET and POST JSON requests.
//   Used by:
//      • PolygonAPI      (market data)
//      • FastAPI ML API  (XGBoost predictions)
//      • AlphaVantage    (fundamentals)
// -------------------------------------------------------------
class HttpClient {
public:
    // ---------------------------------------------------------
    // GET request
    // Example:
    //   std::string r = HttpClient::Get("https://api.polygon.io/v2/...");
    //
    // Returns "{}" on error.
    // ---------------------------------------------------------
    static std::string Get(const std::string& url);

    // ---------------------------------------------------------
    // POST JSON request
    // Example:
    //   std::string r = HttpClient::PostJson(
    //       "http://localhost:6000/predict_from_series",
    //       R"({"ticker":"AAPL","horizon":15})"
    //   );
    //
    // Returns "{}" on error.
    // ---------------------------------------------------------
    static std::string PostJson(
        const std::string& url,
        const std::string& jsonBody
    );
};


