#pragma once
#include "PolygonAPI.hpp"
#include "LinearRegression.hpp"
#include <vector>

struct Indicators {
    double sma20 = 0.0;
    double ema20 = 0.0;
    double rsi14 = 0.0;
};

class Predictor {
public:
    explicit Predictor(PolygonAPI* api) : api(api) {}

    // aheadMinutes = how many 1-minute bars into the future to predict (>=1)
    double predictNextPrice(const std::string& ticker,
        Indicators& outIndics,
        std::vector<double>& closes,
        int aheadMinutes = 1);

private:
    PolygonAPI* api;

    double computeSMA(const std::vector<double>& data, int period);
    double computeEMA(const std::vector<double>& data, int period);
    double computeRSI(const std::vector<double>& data, int period);
};

