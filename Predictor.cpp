#include <string>
#include <cstdlib>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

#include "HttpClient.hpp"
#include "Predictor.hpp"
#include "LinearRegression.hpp"
#include "Indicators.hpp"

// ---------------------------------------------------------
// POST to Python ML Server
// ---------------------------------------------------------
double get_ml_prediction_from_series(
    const std::string& ticker,
    int horizonMinutes,
    const std::vector<double>& closes)
{
    if (closes.empty())
        return -1.0;

    std::ostringstream oss;
    oss << "{";
    oss << "\"ticker\":\"" << ticker << "\",";
    oss << "\"horizon\":" << horizonMinutes << ",";
    oss << "\"closes\":[";

    for (size_t i = 0; i < closes.size(); ++i)
    {
        if (i > 0) oss << ",";
        oss << closes[i];
    }
    oss << "]}";

    const std::string body = oss.str();
    const std::string res = HttpClient::PostJson(
        "http://localhost:6000/predict_from_series",
        body
    );

    // Find "prediction":
    std::size_t pos = res.find("\"prediction\":");
    if (pos == std::string::npos)
        return -1.0;

    pos += 13;

    // Skip chars until hitting a number or minus sign
    while (pos < res.size() &&
        !((res[pos] >= '0' && res[pos] <= '9') || res[pos] == '-'))
        pos++;

    // Find end of number
    std::size_t end_pos = res.find_first_of(",}", pos);
    if (end_pos == std::string::npos)
        end_pos = res.size();

    std::string val_str = res.substr(pos, end_pos - pos);
    double val = -1.0;

    try {
        val = std::stod(val_str);
    }
    catch (...) {
        return -1.0;
    }

    if (!std::isfinite(val))
        return -1.0;

    return val;
}

// ---------------------------------------------------------------------
// *** FIX C26451 ARITHMETIC OVERFLOW ***
// The overflow warning at Line 202 must be fixed in your local file by 
// casting one operand of the subtraction to long long (or int64_t).
// Example: long long diff = (long long)time_end - time_start;
// ---------------------------------------------------------------------

// ======================================================================
// BASIC INDICATORS
// ======================================================================
double Predictor::computeSMA(const std::vector<double>& data, int period)
{
    if (data.size() < static_cast<size_t>(period))
        return 0.0;

    const size_t start_idx = data.size() - static_cast<size_t>(period);

    double sum = 0.0;
    for (size_t i = start_idx; i < data.size(); ++i)
        sum += data[i];

    return sum / static_cast<double>(period);
}

double Predictor::computeEMA(const std::vector<double>& data, int period)
{
    if (data.size() < static_cast<size_t>(period))
        return 0.0;

    double alpha = 2.0 / (static_cast<double>(period) + 1.0);

    size_t start_idx = data.size() - static_cast<size_t>(period);
    double ema = data[start_idx];

    // FIX C26451: Casting 1 to size_t to ensure 8-byte index arithmetic
    for (size_t i = start_idx + (size_t)1; i < data.size(); ++i)
        ema = alpha * data[i] + (1.0 - alpha) * ema;

    return ema;
}

double Predictor::computeRSI(const std::vector<double>& data, int period)
{
    if (data.size() <= static_cast<size_t>(period))
        return 0.0;

    double gain = 0.0;
    double loss = 0.0;

    size_t start_loop = data.size() - static_cast<size_t>(period) + 1;

    for (size_t i = start_loop; i < data.size(); ++i)
    {
        // Cast is redundant since data is vector<double>, but harmless
        double diff = data[i] - (double)data[i - 1];
        if (diff > 0) gain += diff;
        else           loss -= diff;
    }

    if (loss == 0.0)
        return 100.0;

    double rs = gain / loss;

    return 100.0 - (100.0 / (1.0 + rs));
}


// ======================================================================
// ADVANCED INDICATORS
// ======================================================================

// Magnetic Zone
double Predictor::computeMagneticZone(const std::vector<double>& data)
{
    if (data.empty()) return 0.0;

    double recent = data.back();
    double avg = std::accumulate(data.begin(), data.end(), 0.0)
        / static_cast<double>(data.size());

    return (recent - avg) / 5.0;
}

// RSI Divergence
double Predictor::computeRSIDivergence(const std::vector<double>& data)
{
    if (data.size() < 30)
        return 0.0;

    double rsiA = computeRSI(data, 14);

    size_t prior_end = data.size() - 10;
    std::vector<double> prior(data.begin(), data.begin() + prior_end);

    double rsiB = computeRSI(prior, 14);

    double priceA = data.back();
    double priceB = data[data.size() - 11];

    bool bearish = (priceA > priceB && rsiA < rsiB);
    bool bullish = (priceA < priceB&& rsiA > rsiB);

    if (bearish) return -1.0;
    if (bullish) return 1.0;

    return 0.0;
}

// Trend Strength
double Predictor::computeTrendStrength(const std::vector<double>& data)
{
    if (data.size() < 10) return 0.0;

    int up = 0;
    int down = 0;

    for (size_t i = 1; i < data.size(); ++i)
    {
        if (data[i] > data[i - 1]) up++;
        else if (data[i] < data[i - 1]) down++;
    }

    int total = up + down;
    if (total == 0)
        return 0.0;

    return (static_cast<double>(up) - static_cast<double>(down)) / static_cast<double>(total);
}

// Volume Pressure
double Predictor::computeVolumePressure(const std::vector<double>& volume)
{
    if (volume.size() < 10)
        return 0.0;

    double recent = volume.back();
    double avg = 0.0;

    size_t start = volume.size() - 10;

    for (size_t i = start; i < volume.size(); ++i)
        avg += volume[i];

    avg /= 10.0;

    return recent - avg;
}


// ======================================================================
// MAIN PREDICTION ENGINE
// ======================================================================
double Predictor::predictNextPrice(
    const std::string& ticker,
    Indicators& outIndics,
    std::vector<double>& closes,
    int aheadMinutes)
{
    // Load bars
    auto bars = api->getBars(ticker);

    closes.clear();
    std::vector<double> volumes;

    for (auto& b : bars)
    {
        closes.push_back(b.close);
        volumes.push_back(b.volume);
    }

    if (closes.size() < 5)
        return 0.0;

    aheadMinutes = std::max(1, std::min(aheadMinutes, 120));

    // BASIC
    outIndics.sma20 = computeSMA(closes, 20);
    outIndics.ema20 = computeEMA(closes, 20);
    outIndics.rsi14 = computeRSI(closes, 14);

    // ADVANCED
    outIndics.magZone = computeMagneticZone(closes);
    outIndics.rsiDiv = computeRSIDivergence(closes);
    outIndics.trend = computeTrendStrength(closes);
    outIndics.volPressure = computeVolumePressure(volumes);

    // Stub fields
    // FIX L257: Assigning a numeric value (0.0) instead of an incorrect string literal
    outIndics.newsSent = 0.0;
    outIndics.pcr = 0.0;
    outIndics.vix = 0.0;
    outIndics.isLondon = false;
    outIndics.isNewYork = false;
    outIndics.isPowerHour = false;

    // LINEAR REGRESSION
    std::vector<double> x(closes.size());
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<double>(i);

    LinearRegression lr;
    lr.train(x, closes);

    size_t last_index = closes.size() - 1;

    double lr_point = static_cast<double>(last_index) +
        static_cast<double>(aheadMinutes);

    double lrPred = lr.predict(lr_point);

    // ML Server
    double mlPred = get_ml_prediction_from_series(ticker, aheadMinutes, closes);
    bool ml_ok = (mlPred > 0 && std::isfinite(mlPred));

    double finalPred;

    if (ml_ok)
    {
        finalPred = (0.80 * mlPred) + (0.20 * lrPred);

        if (outIndics.rsiDiv == -1) finalPred *= 0.995;
        if (outIndics.rsiDiv == 1)  finalPred *= 1.005;

        if (std::abs(outIndics.magZone) > 0.6)
            finalPred = (finalPred + closes.back()) / 2.0;
    }
    else {
        finalPred = lrPred;
    }

    return finalPred;
}






