#include "Delta.h"

void AdjustPeriods0(std::vector<double>& periods, double start, double end, bool up)
{
	if (periods.size() == 0) periods.push_back(0.0);
	if (up) {
		if (periods.size() == 1 || start > periods[periods.size() - 1]) {
			periods.push_back(start);
			periods.push_back(end);
		}
		else {
			for (int i = 2; i < periods.size(); i += 2) {
				if (start > periods[i]) continue;
				if (end < periods[i - 1]) {
					periods.insert(periods.begin() + i - 1, start);
					periods.insert(periods.begin() + i, end);
					for (int j = i + 1; j < periods.size(); j++)
						periods[j] += end - start;
					break;
				}
				else {
					for (int j = i; j < periods.size(); j++)
						periods[j] += end - start;
					break;
				}
			}
		}
	}
	else {
		if (periods.size() == 1 || start > periods[periods.size() - 1]) {
			for (int j = 1; j < periods.size(); j++) periods[j] -= end - start;
			periods.push_back(start);
			periods.push_back(end);
		}
		else {
			for (int i = 2; i < periods.size(); i += 2) {
				if (start > periods[i]) continue;
				if (end < periods[i - 1]) {
					for (int j = 1; j < i - 1; j++) periods[j] -= end - start;
					periods.insert(periods.begin() + i - 1, start);
					periods.insert(periods.begin() + i, end);
					break;
				}
				else {
					for (int j = 1; j < i; j++) periods[j] -= end - start;
					break;
				}
			}
		}
		periods[0] -= end - start;
	}
}

void CombinePeriods(std::vector<double>& periods, int k)
{
	if (k < 2) return;
	if (k >= periods.size()) return;
	int off = 0;
	for (int i = 1; i <= k; i += 2) {
		off += periods[i] - periods[i - 1];
	}
	periods[k - 1] = periods[k] - off;
	for (int i = 0; i < k - 1; i++) periods.erase(periods.begin());
}

void AdjustPeriods(std::vector<double>& periods, double start, double end, bool up)
{
	if (periods.size() < 2) return;
	if (up) {
		for (int i = 1; i < periods.size(); i += 2) {
			if (start > periods[i]) continue;
			if (start > periods[i - 1]) {
				periods.insert(periods.begin() + i, start);
				periods.insert(periods.begin() + i + 1, end);
				for (int j = i + 2; j < periods.size(); j++)
					periods[j] += end - start;
				//CombinePeriods(periods, i - 2);
				break;
			}
			else if (end > periods[i - 1]) {
				double off = end - periods[i - 1];
				for (int j = i - 1; j < periods.size(); j++)
					periods[j] += off;
				//CombinePeriods(periods, i - 2);
				break;
			}
		}
	}
	else {
		for (int i = periods.size()-1; i > 0; i -= 2) {
			if (end < periods[i-1]) continue;
			if (end < periods[i]) {
				for (int j = 0; j <= i - 1; j++)
					periods[j] -= end - start;
				periods.insert(periods.begin() + i, start);
				periods.insert(periods.begin() + i + 1, end);
				break;
			}
			else if (start < periods[i]) {
				double off = periods[i] - start;
				for (int j = 0; j <= i; j++)
					periods[j] -= off;
				break;
			}
		}
	}
}

double deltaCalculation(double price, double bottomPrice, double topPrice, double multiplier, double offset, const std::vector<double>& periods)
{
	if (periods.size() > 0) {
		double off = 0;
		for (int i = 1; i < periods.size(); i += 2) {
			if (price > periods[i]) {
				off += periods[i] - periods[i - 1];
			}
			else if (price < periods[i - 1]) {
				break;
			}
			else {
				off += price - periods[i - 1];
			}
		}
		price = bottomPrice + off;
	}

	double delta = 0;
	if (price > topPrice) {
		delta = 0;
	}
	else if (topPrice <= bottomPrice) {
		delta = 0;
	}
	else if (price < bottomPrice) {
		delta = 1;
	}
	else {
		delta = log(topPrice / price) / log(topPrice / bottomPrice); //= IF(G2>AE2, LOG(AD2 / G2) / (LOG(AD2 / AE2)), 1)*AF2
	}

	delta *= multiplier * 100;
	delta += offset;
	return round(delta + 0.49);
}

double priceFromDelta(double delta, double bottomPrice, double topPrice, double multiplier, double offset, const std::vector<double>& periods)
{
	double price;
	if (multiplier == 0.0 || topPrice <= bottomPrice) {
		if (delta >= offset) price = bottomPrice;
		else price = topPrice;
	}
	else price = topPrice / exp((delta - offset - 0.49) / (100 * multiplier) * log(topPrice / bottomPrice));
	if (periods.size() > 0) {
		double off = price - bottomPrice;
		for (int i = 1; i < periods.size(); i += 2) {
			if (off > periods[i] - periods[i - 1]) {
				off -= periods[i] - periods[i - 1];
				continue;
			}
			else {
				return periods[i - 1] + off;
			}
		}
		if (off > 0) return periods[periods.size() - 1] + off;
	}

	return price;
}


double round(double a, double precision)
{
	return round(a / precision) * precision;
}

