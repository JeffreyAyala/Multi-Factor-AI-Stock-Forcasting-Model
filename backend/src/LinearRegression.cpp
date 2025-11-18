#include "LinearRegression.hpp"
#include <numeric>

void LinearRegression::train(const std::vector<double>& x, const std::vector<double>& y){
    size_t n = x.size();
    if(n == 0 || y.size() != n) return;

    double mx = std::accumulate(x.begin(), x.end(), 0.0) / n;
    double my = std::accumulate(y.begin(), y.end(), 0.0) / n;

    double num = 0.0, den = 0.0;
    for(size_t i = 0; i < n; ++i){
        num += (x[i] - mx) * (y[i] - my);
        den += (x[i] - mx) * (x[i] - mx);
    }
    if(den == 0.0){
        a = 0.0;
        b = my;
        return;
    }

    a = num / den;
    b = my - a * mx;
}

double LinearRegression::predict(double x) const{
    return a * x + b;
}
