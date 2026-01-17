#pragma once
#include <vector>

class LinearRegression {
public:
    void train(const std::vector<double>& x, const std::vector<double>& y);
    double predict(double x) const;

private:
    double a = 0.0;
    double b = 0.0;
};
