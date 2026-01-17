#pragma once
#include <vector>
#include <string>
#include "PolygonAPI.hpp"
#include "Indicators.hpp"


// ================================================
// EXTENDED INDICATORS (Matches Predictor.cpp)
// ================================================


class Predictor {
public:
    explicit Predictor(PolygonAPI* api) : api(api) {}

    // Main prediction entry point
    double predictNextPrice(
        const std::string& ticker,
        Indicators& outIndics,
        std::vector<double>& closes,
        int aheadMinutes = 1
    );

private:
    PolygonAPI* api;

    // BASIC INDICATORS
    double computeSMA(const std::vector<double>& data, int period);
    double computeEMA(const std::vector<double>& data, int period);
    double computeRSI(const std::vector<double>& data, int period);

    // ADVANCED INDICATORS (these were missing before)
    double computeMagneticZone(const std::vector<double>& data);
    double computeRSIDivergence(const std::vector<double>& data);
    double computeTrendStrength(const std::vector<double>& data);
    double computeVolumePressure(const std::vector<double>& volume);
};



