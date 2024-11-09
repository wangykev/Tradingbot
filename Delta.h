#pragma once
#include <iostream>
#include <sstream>
#include <vector>

void AdjustPeriods(std::vector<double>& periods, double start, double end, bool up);
double deltaCalculation(double price, double bottomPrice, double topPrice, double multiplier, double offset, const std::vector<double>& periods);
double priceFromDelta(double delta, double bottomPrice, double topPrice, double multiplier, double offset, const std::vector<double>& periods);
double round(double a, double precision);

