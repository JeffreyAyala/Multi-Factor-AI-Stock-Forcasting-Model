#pragma once

struct Indicators
{
    // ==== BASIC TECHNICAL ====
    double sma20 = 0.0;
    double ema20 = 0.0;
    double rsi14 = 0.0;

    // ==== ADVANCED SIGNALS ====
    double rsiDiv = 0.0;        // -1 bearish, +1 bullish
    double magZone = 0.0;       // deviation from mean
    double trend = 0.0;         // up vs down ratio
    double volPressure = 0.0;   // volume pressure

    // ==== MULTI-FACTOR PLACEHOLDERS ====
    double newsSent = 0.0;      // from ML server later
    double pcr = 0.0;           // put/call ratio
    double vix = 0.0;           // volatility regime

    // ==== SESSION TAGS ====
    bool isLondon = false;
    bool isNewYork = false;
    bool isPowerHour = false;
}; // End of struct definition


