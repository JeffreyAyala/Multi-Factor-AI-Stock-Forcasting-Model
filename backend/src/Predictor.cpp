#include <string>
#include <cstdlib>
#include <sstream>
#include <cmath>
#include <algorithm>

#include "HttpClient.hpp"
#include "Predictor.hpp"

// ---------------------------------------------------------
// Call Python ML server (FastAPI) using the closes vector
// ---------------------------------------------------------
double get_ml_prediction_from_series(const std::string& ticker,
    int horizonMinutes,
    const std::vector<double>& closes)
{
    // Build JSON body:
    // {
    //   "ticker": "AAPL",
    //   "horizon": 60,
    //   "closes": [ ... ]
    // }
    std::ostringstream oss;
    oss << "{";
    oss << "\"ticker\":\"" << ticker << "\",";
    oss << "\"horizon\":" << horizonMinutes << ",";
    oss << "\"closes\":[";
    for (size_t i = 0; i < closes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << closes[i];
    }
    oss << "]}";

    std::string body = oss.str();

    // Call the Python ML server (POST /predict_from_series)
    std::string res = HttpClient::PostJson(
        "http://localhost:6000/predict_from_series",
        body
    );

    // Find "prediction" in the JSON response
    std::size_t pos = res.find("\"prediction\":");
    if (pos == std::string::npos)
        return -1.0;

    pos += 13;  // move past "prediction":
    while (pos < res.size() && (res[pos] == ' ' || res[pos] == ':'))
        ++pos;

    const char* start = res.c_str() + pos;
    char* endPtr = nullptr;
    double value = std::strtod(start, &endPtr);

    return value;
}

// ---------------------------------------------------------
// Existing indicator helpers
// ---------------------------------------------------------
double Predictor::computeSMA(const std::vector<double>& data, int period) {
    if (data.size() < static_cast<size_t>(period)) return 0.0;
    double sum = 0.0;
    for (size_t i = data.size() - period; i < data.size(); ++i) {
        sum += data[i];
    }
    return sum / period;
}

double Predictor::computeEMA(const std::vector<double>& data, int period) {
    if (data.size() < static_cast<size_t>(period)) return 0.0;
    double alpha = 2.0 / (period + 1.0);
    double ema = data[data.size() - period];
    for (size_t i = data.size() - period + 1; i < data.size(); ++i) {
        ema = alpha * data[i] + (1.0 - alpha) * ema;
    }
    return ema;
}

double Predictor::computeRSI(const std::vector<double>& data, int period) {
    if (data.size() <= static_cast<size_t>(period)) return 0.0;
    double gain = 0.0, loss = 0.0;
    for (size_t i = data.size() - period; i < data.size(); ++i) {
        double diff = data[i] - data[i - 1];
        if (diff > 0) gain += diff;
        else loss -= diff;
    }
    if (loss == 0.0) return 100.0;
    double rs = gain / loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// ---------------------------------------------------------
// Main prediction function used by the backend
// ---------------------------------------------------------
double Predictor::predictNextPrice(const std::string& ticker,
    Indicators& outIndics,
    std::vector<double>& closes,
    int aheadMinutes)
{
    auto bars = api->getBars(ticker);
    closes.clear();
    for (auto& b : bars) {
        closes.push_back(b.close);
    }

    if (closes.size() < 5) return 0.0;

    // clamp horizon: at least 1 minute, at most 120 minutes
    if (aheadMinutes < 1) aheadMinutes = 1;
    if (aheadMinutes > 120) aheadMinutes = 120;

    // compute indicators for the UI (unchanged)
    outIndics.sma20 = computeSMA(closes, 20);
    outIndics.ema20 = computeEMA(closes, 20);
    outIndics.rsi14 = computeRSI(closes, 14);

    // prepare data for linear regression (fallback model)
    std::vector<double> x;
    x.reserve(closes.size());
    for (size_t i = 0; i < closes.size(); ++i) {
        x.push_back(static_cast<double>(i));
    }

    LinearRegression lr;
    lr.train(x, closes);

    int n = static_cast<int>(closes.size());
    int lastIndex = n - 1;
    double futureIndex = static_cast<double>(lastIndex)
        + static_cast<double>(aheadMinutes);

    // ---------------------------------------------
    // NEW: try ML prediction from Python XGBoost
    // using the closes vector we already have
    // ---------------------------------------------
    double mlPred = get_ml_prediction_from_series(ticker, aheadMinutes, closes);

    // If ML server responded with a valid value, use it.
    // Otherwise, fall back to the old linear regression.
    if (mlPred > 0.0) {
        return mlPred;
    }
    else {
        return lr.predict(futureIndex);
    }
}

