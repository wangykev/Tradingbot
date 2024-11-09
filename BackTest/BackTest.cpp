// BackTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "../curl_utils.hpp"
#include <sstream>
#include <vector>
#include "../Delta.h"
#define sign(x) ((x>0) - (x< 0))

int main1()
{
    time_t now_t = time(NULL);
    struct tm* tm_struct = localtime(&now_t);

    struct tm time1 = { 0 };
    time1.tm_year = tm_struct->tm_year - 30;
    time1.tm_mon = tm_struct->tm_mon;
    time1.tm_mday = tm_struct->tm_mday;

    struct tm time2 = { 0 };
    time2.tm_year = tm_struct->tm_year;
    time2.tm_mon = tm_struct->tm_mon;
    time2.tm_mday = tm_struct->tm_mday;

    std::string symbol = "RCL";

    std::string csv = downloadYahooCsv(symbol, _mkgmtime(&time1), now_t, "1d");
    std::istringstream iss(csv);

    std::string line;
    std::getline(iss, line);
    std::string output; char outline[2000];

    struct dayData {
        int year, mon, day;
        double high, low, open, close, adj_close, volume;
        double day_change, tmp_change;
        double std_variation;
        double top_bound, low_bound;
        double low_bound_discount;
        double fat_change, basic_change;
        double top, bottom, multiplier, offset;
        double position;
        double pos_change, basic_pos_change, fat_pos_change;
        double basic_pos_change_price, fat_pos_change_price;
        double trade_profit;
        double cash, balance;
        double fat_position, fat_cash, fat_balance;
        double orig_position, orig_cash, orig_balance, orig_trade_profit;
    };

    std::vector<dayData> history;
    //   history.clear();

    while (std::getline(iss, line))
    {
        dayData d;
        if (sscanf(line.c_str(), "%04d-%02d-%02d,%lf,%lf,%lf,%lf,%lf,%lf", &d.year, &d.mon, &d.day, &d.open, &d.high, &d.low, &d.close, &d.adj_close, &d.volume) != 9) {
            printf("Error Process Yahoo CSV file line %s\n", line.c_str());
        }
        else {
            history.push_back(d);
        }
    }

    if (history.size() < 3) return 1;
//Date	Open	High	Low	Close	Adj Close	Volume	Change Percent	Temp Change Percent	Temp std derivation	Bound	Bound	Fat changes	Middle changes	Change	Change	Closing Adjust Rate	Bottom	Top	Between?	Change	Change	Change Price	Position	Offset	Cash	trading profit	Balance	position change	change price	position	Cash	Balance

    sprintf(outline, "Date,Open,High,Low,Close,Adj Close,Volume,Change Percent,Temp Change Percent,Temp std derivation,Low Bound, High Bound,Fat change,Basic changes, Bottom,Top, Fat Position Change, Basic Position Change, Basic Change Price, Position, Offset, Cash,trading profit,Balance, fat_position, fat_cash, fat_balance, orig_position, orig_trade_profit, orig_cash, orig_balance\n");
    output += outline;
    for (int i = 0; i < history.size(); i++) {
        dayData& h = history[i];
        h.multiplier = 1000;
        if (i == 0) {
            h.day_change = h.tmp_change = 0;
            h.std_variation = 0.02;
            h.fat_change = h.basic_change = 0;
            h.top = h.adj_close * 1.3;
            h.low_bound_discount = 0.05;
            h.offset = 0;
        }
        else {
            dayData& p = history[i - 1];
            h.day_change = h.adj_close / p.adj_close - 1.0;
            h.tmp_change = fabs(h.day_change) < 3 * p.std_variation ? h.day_change : sign(h.day_change) * 3 * p.std_variation;                //=IF(ABS(H5)< 3*J4, H5, SIGN(H5)*3*J4)
            int count;
            count = 30;
            double volatility;
            if (i < count + 1) h.std_variation = p.std_variation;
            else {
                double avg = 0;
                for (int j = i - 1; j >= i - count; j--) {
                    avg += history[j].tmp_change;
                }
                avg /= count;
                volatility = 0;
                for (int j = i - 1; j >= i - count; j--) {
                    volatility += (history[j].tmp_change - avg) * (history[j].tmp_change - avg);
                }
                volatility = sqrt(volatility / (count - 1));
                h.std_variation = volatility > 0.02 ? (volatility - 0.02) / 2 + 0.02 : volatility; //=IF(STDEV.S(I5:I34) > 0.02, (STDEV.S(I5:I34) - 0.02)/2+0.02, STDEV.S(I5:I34))
            }
            h.low_bound_discount = p.low_bound_discount;
        }
        h.top_bound = h.std_variation;
        h.low_bound = -h.std_variation * (1 + h.low_bound_discount);

        if (i > 0) {
            dayData& p = history[i - 1];
            //=IF(H5>K5, (1+H5)/(1+K5)-1, IF(H5 < L5, (1+H5)/(1+L5)-1, 0))
            if (h.day_change > h.top_bound) {
                h.fat_change = (1 + h.day_change) / (1 + h.top_bound) - 1;
            }
            else if (h.day_change < h.low_bound) {
                h.fat_change = (1 + h.day_change) / (1 + h.low_bound) - 1;
            }
            else h.fat_change = 0;

            h.basic_change = (1 + h.day_change) / (1 + h.fat_change) - 1;
            h.top = p.top;
            h.bottom = h.top / (1 + h.std_variation * sqrt(252)) / (1 + h.std_variation * sqrt(252));
            h.offset = p.offset + h.multiplier * log(1 + h.fat_change);
        }


        if (i == 0) {
            h.position = h.multiplier * log(h.top / h.adj_close) + h.offset;
            h.pos_change = 0;
            h.basic_pos_change = 0;
            h.fat_pos_change = 0; h.fat_pos_change_price = h.adj_close;
            h.cash = -h.position * h.adj_close;
            h.trade_profit = 0;
            h.balance = 0;
            h.basic_pos_change_price = h.adj_close;
            h.fat_cash = h.fat_balance = 0;
            h.fat_position = 0;
            h.orig_position = h.multiplier * log(h.top / h.adj_close);
            h.orig_cash = -h.orig_position * h.adj_close;
            h.orig_balance = h.orig_trade_profit = 0;
        }
        else {
            dayData& p = history[i - 1];
            bool doCap = false;

            h.position = h.multiplier * log(h.top / h.adj_close) + h.offset;

            if (doCap) {
                if (h.adj_close > h.top) {
                    h.position = h.offset;
                }
                else if (h.adj_close < h.bottom) {
                    h.position = h.multiplier * log(h.top / h.bottom) + h.offset;
                }
            }

            h.pos_change = h.position - p.position;
            h.fat_pos_change = h.offset - p.offset; h.fat_pos_change_price = h.adj_close;
            h.basic_pos_change = h.pos_change;
            h.basic_pos_change_price = p.adj_close * sqrt(1 + h.basic_change);

            if (doCap && h.adj_close > h.top) {
                h.trade_profit = 0;
            }
            else if (doCap && h.adj_close < h.bottom) {
                h.trade_profit = 0;
            }
            else {
                h.trade_profit = 0.5 * h.multiplier * h.basic_pos_change_price * h.std_variation * h.std_variation;
                if (fabs(h.fat_pos_change) > 0.0001) h.trade_profit *= fabs(h.basic_change) / (fabs(h.basic_change) + fabs(h.fat_change));
            }

            h.cash = p.cash -h.pos_change * h.basic_pos_change_price + h.trade_profit;
            h.balance = h.position * h.adj_close + h.cash;
            h.fat_cash = p.fat_cash - h.fat_pos_change * h.fat_pos_change_price;
            h.fat_position = p.fat_position + h.fat_pos_change;
            h.fat_balance = h.fat_cash+h.fat_position*h.adj_close;

            h.orig_position = h.multiplier * log(h.top / h.adj_close);
            h.orig_trade_profit = 0.5 * h.multiplier * sqrt(p.adj_close * h.adj_close) * h.std_variation * h.std_variation;
            h.orig_cash = p.orig_cash - (h.orig_position - p.orig_position) * sqrt(p.adj_close * h.adj_close) + h.orig_trade_profit;
            h.orig_balance = h.orig_cash + h.orig_position * h.adj_close;
        }
        //h.low_bound_discount = (h.adj_close / sqrt(h.top * h.bottom) - 1) * 0.10;
        //h.low_bound_discount = 0.05;
        h.low_bound_discount = -h.position / (h.multiplier * log(1+h.std_variation*sqrt(252)) * 2) * 0.25;

        sprintf(outline, "%02d/%02d/%04d,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g, %g,%g,%g, %g,%g, %g, %g, %g, %g, %g, %g,%g,%g, %g, %g, %g, %g, %g, %g, %g\n",
            h.mon, h.day, h.year, h.open, h.high, h.low, h.close, h.adj_close, h.volume, h.day_change, h.tmp_change, h.std_variation, h.low_bound, h.top_bound, h.fat_change, h.basic_change, h.bottom, h.top, 
            h.fat_pos_change, h.basic_pos_change, h.basic_pos_change_price, h.position, h.offset, h.cash, h.trade_profit, h.balance, h.fat_position, h.fat_cash, h.fat_balance, h.orig_position, h.orig_trade_profit, h.orig_cash, h.orig_balance);
        output += outline;
    }

    std::string filename = symbol;
    filename += "_backtest.csv";
    FILE* f = fopen(filename.c_str(), "w");
    fprintf(f, "%s", output.c_str());
    fclose(f);

    return 0;
}

double calcPosition(double price,  double top, double bottom, double multiplier, double offset)
{
    if (price < bottom) return multiplier + offset;
    else if (price > top) return offset;
    else return multiplier / log(top / bottom) * log(top / price) + offset;

}


int main()
{

    time_t now_t = time(NULL);
    struct tm* tm_struct = localtime(&now_t);

    struct tm time1 = { 0 };
    time1.tm_year = tm_struct->tm_year - 30;
    time1.tm_mon = tm_struct->tm_mon;
    time1.tm_mday = tm_struct->tm_mday;

    struct tm time2 = { 0 };
    time2.tm_year = tm_struct->tm_year;
    time2.tm_mon = tm_struct->tm_mon;
    time2.tm_mday = tm_struct->tm_mday;

    std::string symbol = "BABA";

    std::string csv = downloadYahooCsv(symbol, _mkgmtime(&time1), now_t, "1d");
    std::istringstream iss(csv);

    std::string line;
    std::getline(iss, line);
    std::string output; char outline[2000];

    struct dayData {
        int year, mon, day;
        double high, low, open, close, adj_close, volume;
        double day_change, tmp_change;
        double std_variation;
        double top_bound, low_bound;
        double low_bound_discount;
        double fat_change, basic_change;
        double top, bottom, multiplier, offset;
        double position;
        double pos_change, basic_pos_change, fat_pos_change;
        double basic_pos_change_price, fat_pos_change_price;
        double trade_profit;
        double cash, balance;
        double fat_position, fat_cash, fat_balance;
        double orig_position, orig_cash, orig_balance, orig_trade_profit;
        std::vector<double> periods;
    };

    std::vector<dayData> history;
    //   history.clear();

    while (std::getline(iss, line))
    {
        dayData d;
        if (sscanf(line.c_str(), "%04d-%02d-%02d,%lf,%lf,%lf,%lf,%lf,%lf", &d.year, &d.mon, &d.day, &d.open, &d.high, &d.low, &d.close, &d.adj_close, &d.volume) != 9) {
            printf("Error Process Yahoo CSV file line %s\n", line.c_str());
            return 1;
        }
        else {
            history.push_back(d);
        }
    }

    if (history.size() < 3) return 1;
    //Date	Open	High	Low	Close	Adj Close	Volume	Change Percent	Temp Change Percent	Temp std derivation	Bound	Bound	Fat changes	Middle changes	Change	Change	Closing Adjust Rate	Bottom	Top	Between?	Change	Change	Change Price	Position	Offset	Cash	trading profit	Balance	position change	change price	position	Cash	Balance

    sprintf(outline, "Date,Open,High,Low,Close,Adj Close,Volume,Change Percent,Temp Change Percent,Temp std derivation,Low Bound, High Bound,Fat change,Basic changes, Bottom,Top, Fat Position Change, Basic Position Change, Basic Change Price, Position, Offset, Cash,trading profit,Balance, fat_position, fat_cash, fat_balance, orig_position, orig_trade_profit, orig_cash, orig_balance\n");
    output += outline;
    for (int i = 0; i < history.size(); i++) {
        dayData& h = history[i];
        h.multiplier = 1000;
        if (i == 0) {
            h.day_change = h.tmp_change = 0;
            h.std_variation = 0.02;
            h.fat_change = h.basic_change = 0;
            h.top = h.adj_close * 1.5;
            h.bottom = h.adj_close / 1.5;
            h.low_bound_discount = 0.;
            h.offset = 0;
            h.periods.clear();
            h.periods.push_back(h.bottom);
            h.periods.push_back(h.top);
        }
        else {
            dayData& p = history[i - 1];
            h.day_change = h.adj_close / p.adj_close - 1.0;
            h.tmp_change = fabs(h.day_change) < 3 * p.std_variation ? h.day_change : sign(h.day_change) * 3 * p.std_variation;                //=IF(ABS(H5)< 3*J4, H5, SIGN(H5)*3*J4)
            int count;
            count = i < 3? 3: i-1;
            if (count > 30) count = 30;
            double volatility;
            if (i < count + 1) h.std_variation = p.std_variation;
            else {
                double avg = 0;
                for (int j = i - 1; j >= i - count; j--) {
                    avg += history[j].tmp_change;
                }
                avg /= count;
                volatility = 0;
                for (int j = i - 1; j >= i - count; j--) {
                    volatility += (history[j].tmp_change - avg) * (history[j].tmp_change - avg);
                }
                volatility = sqrt(volatility / (count - 1));
                //h.std_variation = volatility > 0.02 ? (volatility - 0.02) / 2 + 0.02 : volatility; //=IF(STDEV.S(I5:I34) > 0.02, (STDEV.S(I5:I34) - 0.02)/2+0.02, STDEV.S(I5:I34))
                h.std_variation = volatility;
            }
            h.low_bound_discount = p.low_bound_discount;
        }
        h.top_bound = h.std_variation;
        h.low_bound = -h.std_variation * (1 + h.low_bound_discount);

        if (i > 0) {
            dayData& p = history[i - 1];
            //=IF(H5>K5, (1+H5)/(1+K5)-1, IF(H5 < L5, (1+H5)/(1+L5)-1, 0))
            if (h.day_change > h.top_bound) {
                h.fat_change = (1 + h.day_change) / (1 + h.top_bound) - 1;
            }
            else if (h.day_change < h.low_bound) {
                h.fat_change = (1 + h.day_change) / (1 + h.low_bound) - 1;
            }
            else h.fat_change = 0;

            h.basic_change = (1 + h.day_change) / (1 + h.fat_change) - 1;
            h.top = p.top;
            //h.bottom = h.top / (1 + h.std_variation * sqrt(252)) / (1 + h.std_variation * sqrt(252));
            h.bottom = p.bottom;
            h.offset = p.offset;
            h.periods = p.periods;
            if (h.fat_change > 0 && p.adj_close > h.periods[h.periods.size() - 2]) AdjustPeriods(h.periods, p.adj_close * (1+h.fat_change), h.adj_close, true);
            else if (h.fat_change < 0 && p.adj_close < h.periods[1]) AdjustPeriods(h.periods, h.adj_close, p.adj_close * (1 + h.fat_change), false);
        }


        if (i == 0) {
            //h.position = h.multiplier / sqrt(h.adj_close) + h.offset;
            h.position = deltaCalculation(h.adj_close, h.bottom, h.top, h.multiplier/100, h.offset, h.periods);
            h.pos_change = 0;
            h.basic_pos_change = 0;
            h.fat_pos_change = 0; h.fat_pos_change_price = h.adj_close;
            h.cash = -h.position * h.adj_close;
            h.trade_profit = 0;
            h.balance = 0;
            h.basic_pos_change_price = h.adj_close;
            h.fat_cash = h.fat_balance = 0;
            h.fat_position = 0;
            h.orig_position = h.multiplier / sqrt(h.adj_close);
            h.orig_cash = -h.orig_position * h.adj_close;
            h.orig_balance = h.orig_trade_profit = 0;
        }
        else {
            dayData& p = history[i - 1];
            bool doCap = false;

            h.position = deltaCalculation(h.adj_close, h.bottom, h.top, h.multiplier/100, h.offset, h.periods);

            h.pos_change = h.position - p.position;
            h.fat_pos_change = h.offset - p.offset; h.fat_pos_change_price = h.adj_close;
            h.basic_pos_change = h.pos_change;
            h.basic_pos_change_price = p.adj_close * sqrt(1 + h.basic_change);

            h.trade_profit = 0.5 * h.multiplier/log(h.top/h.bottom) * h.adj_close * h.std_variation * h.std_variation;

            h.cash = p.cash - h.pos_change * h.basic_pos_change_price + h.trade_profit;
            h.balance = h.position * h.adj_close + h.cash;
            h.fat_cash = h.periods[0];
            h.fat_balance = h.periods[h.periods.size() - 1];

            h.orig_position = h.multiplier * log(h.top / h.adj_close);
            h.orig_trade_profit = 0.5 * h.multiplier * sqrt(p.adj_close * h.adj_close) * h.std_variation * h.std_variation;
            h.orig_cash = p.orig_cash - (h.orig_position - p.orig_position) * sqrt(p.adj_close * h.adj_close) + h.orig_trade_profit;
            h.orig_balance = h.orig_cash + h.orig_position * h.adj_close;
            h.fat_position = 0;

            /*
            double top = p.close * (1 + 1*h.top_bound);
            double low = p.close * (1 + 1*h.low_bound);
            double profit = 0;
            h.balance = p.balance;
            h.fat_balance = p.fat_balance;
            if (fabs((p.adj_close / h.adj_close) - p.close / h.close) < 0.05) {
                h.trade_profit = h.orig_trade_profit = 0;
                if (h.high > top && (h.open < top || h.low < top)) {
                    h.trade_profit = h.multiplier * (h.close - top) / top;
                    h.balance += h.multiplier * (h.close - top) / top;
                }
                if (h.low < low && (h.open > low || h.high > low)) {
                    h.fat_balance += h.multiplier * (low - h.close) / low;
                    h.orig_trade_profit = h.multiplier * (low - h.close) / low;
                }
            }
            */
            //h.fat_cash = h.fat_balance + h.balance;

        }
        //h.low_bound_discount = (h.adj_close / sqrt(h.top * h.bottom) - 1) * 0.10;
        h.low_bound_discount = 0.0;
        //h.low_bound_discount = -h.position / (h.multiplier * log(1 + h.std_variation * sqrt(252)) * 2) * 0.25;

        sprintf(outline, "%02d/%02d/%04d,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g, %g,%g,%g, %g,%g, %g, %g, %g, %g, %g, %g,%g,%g, %g, %g, %g, %g, %g, %g, %g\n",
            h.mon, h.day, h.year, h.open, h.high, h.low, h.close, h.adj_close, h.volume, h.day_change, h.tmp_change, h.std_variation, h.low_bound, h.top_bound, h.fat_change, h.basic_change, h.bottom, h.top,
            h.fat_pos_change, h.basic_pos_change, h.basic_pos_change_price, h.position, h.offset, h.cash, h.trade_profit, h.balance, h.fat_position, h.fat_cash, h.fat_balance, h.orig_position, h.orig_trade_profit, h.orig_cash, h.orig_balance);
        output += outline;
    }

    std::string filename = symbol;
    filename += "_backtest.csv";
    FILE* f = fopen(filename.c_str(), "w");
    fprintf(f, "%s", output.c_str());
    fclose(f);

    return 0;
}


