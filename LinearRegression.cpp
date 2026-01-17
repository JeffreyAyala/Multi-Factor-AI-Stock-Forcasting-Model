#include "LinearRegression.hpp"
#include <vector>
#include <numeric>   // for accumulate
#include <cmath>     // for isnan

// --------------------------------------------------------
// Train simple linear regression:
//     y = a*x + b
//
// Uses classic least-squares formulas.
// --------------------------------------------------------
void LinearRegression::train(const std::vector<double>& x,
    const std::vector<double>& y)
{
    size_t n = x.size();
    if (n < 2 || y.size() != n) {
        // Not enough data → default to flat line
        a = 0.0;
        b = (n > 0 ? y.back() : 0.0);
        return;
    }

    // Compute means
    double sumX = std::accumulate(x.begin(), x.end(), 0.0);
    double sumY = std::accumulate(y.begin(), y.end(), 0.0);
    double meanX = sumX / n;
    double meanY = sumY / n;

    // Compute covariance and variance
    double covXY = 0.0;
    double varX = 0.0;

    for (size_t i = 0; i < n; i++) {
        double dx = x[i] - meanX;
        covXY += dx * (y[i] - meanY);
        varX += dx * dx;
    }

    if (varX == 0.0) {
        // No variation in X → flat slope
        a = 0.0;
        b = meanY;
        return;
    }

    // Slope + intercept
    a = covXY / varX;
    b = meanY - a * meanX;
}

// --------------------------------------------------------
// Predict y for given x using trained model
// --------------------------------------------------------
double LinearRegression::predict(double xVal) const
{
    return a * xVal + b;
}

