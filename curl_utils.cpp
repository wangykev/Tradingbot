#include "curl_utils.hpp"
#include "time_utils.hpp"

#include <curl/curl.h>
#include <sstream>
#include <vector>

size_t writeCallback(char *content, size_t size, size_t nmemb, void *userdata) {
    // Append the content to user data
    ((std::string*)userdata)->append(content, size * nmemb);

    // Return the real content size
    return size * nmemb;
}

std::string downloadYahooCsv(
    std::string symbol,
    std::time_t period1,
    std::time_t period2,
    std::string interval
) {
    std::stringstream ss1; 
    ss1 << period1; 
    std::stringstream ss2; 
    ss2 << period2;
    std::string url = "https://query1.finance.yahoo.com/v7/finance/download/"
            + symbol
            + "?period1=" + ss1.str()
            + "&period2=" + ss2.str()
            + "&interval=" + interval
            + "&events=history&includeAdjustedClose=true";

    CURL* curl = curl_easy_init();
    std::string responseBuffer;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Write result into the buffer
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);

        // Cleanup
        curl_easy_cleanup(curl);
    }

    return responseBuffer;
}

void processCSV(std::string csv)
{
    std::istringstream iss(csv);

    std::string line;
    std::getline(iss, line);
    while (std::getline(iss, line))
    {
        int year, mon, day;
        double high, low, open, close, adj_close, volume;
        if (sscanf(line.c_str(), "%04d-%02d-%02d,%lf,%lf,%lf,%lf,%lf,%lf", &year, &mon, &day, &open, &high, &low, &close, &adj_close, &volume) != 9) {
            printf("Error Process Yahoo CSV file line %s\n", line.c_str());
        }
        else {
            printf("%04d-%02d-%02d,%lf,%lf,%lf,%lf,%lf,%lf\n", year, mon, day, open, high, low, close, adj_close, volume);
        }
    }
}

bool getSTockHistoryInfo(const char* symbol,int& lastYear, int &lastMon, int &lastDay, double &todayClose, double &previousClose, double &volatility30, double &volatility100, double &volatility250)
{
    time_t now_t = time(NULL);
    struct tm* tm_struct = localtime(&now_t);

    struct tm time1 = { 0 };
    time1.tm_year = tm_struct->tm_year - 1;
    time1.tm_mon = tm_struct->tm_mon;
    time1.tm_mday = tm_struct->tm_mday;

    struct tm time2 = { 0 };
    time2.tm_year = tm_struct->tm_year;
    time2.tm_mon = tm_struct->tm_mon;
    time2.tm_mday = tm_struct->tm_mday;

    std::string csv = downloadYahooCsv(symbol, _mkgmtime(&time1), now_t, "1d");
    std::istringstream iss(csv);

    std::string line;
    std::getline(iss, line);
    
    struct dayData {
        int year, mon, day;
        double high, low, open, close, adj_close, volume;
        double day_change;
    };

    std::vector<dayData> history;
 //   history.clear();

    while (std::getline(iss, line))
    {
        dayData d;
        if (sscanf(line.c_str(), "%04d-%02d-%02d,%lf,%lf,%lf,%lf,%lf,%lf", &d.year, &d.mon, &d.day, &d.open, &d.high, &d.low, &d.close, &d.adj_close, &d.volume) != 9) {
            printf("Error Process Yahoo CSV file line %s\n", line.c_str());
            return false;
        }
        else {
            history.push_back(d);
        }
    }

    dayData d1, d2;
    if (history.size() == 0) {
        lastYear = lastMon = lastDay = 0;
        todayClose = previousClose = volatility30 = volatility100 = volatility250 = 0.0;
        return false;
    }
    else if (history.size() == 1) {
        d1 = history[0];
        lastYear = d1.year; lastMon = d1.mon; lastDay = d1.day;
        if (d1.year == tm_struct->tm_year + 1900 && d1.mon == tm_struct->tm_mon + 1 && d1.day == tm_struct->tm_mday) {
            todayClose = d1.adj_close;
            previousClose = volatility30 = volatility100 = volatility250 = 0.0;
            return true;
        }
        else {
            previousClose = d1.adj_close;
            todayClose = volatility30 = volatility100 = volatility250 = 0.0;
            return true;
        }
    }

    d1 = history[history.size() - 1];
    lastYear = d1.year; lastMon = d1.mon; lastDay = d1.day;
    d2 = history[history.size() - 2];
    if (d1.year == tm_struct->tm_year + 1900 && d1.mon == tm_struct->tm_mon + 1 && d1.day == tm_struct->tm_mday) {
        todayClose = d1.adj_close;
        previousClose = d2.adj_close;
    }
    else {
        previousClose = d1.adj_close;
        todayClose = 0.0;
    }

    history[0].day_change = 0;
    for (int i = 1; i < history.size(); i++) {
        history[i].day_change = history[i].adj_close/history[i-1].adj_close;
    }

    double volatilityall;
    {
        double avg = 0;
        for (int i = history.size() - 1; i >= 1; i--) {
            avg += history[i].day_change;
        }
        avg /= (history.size() - 1);
        double volatility = 0;
        for (int i = history.size() - 1; i >= 1; i--) {
            volatility += (history[i].day_change - avg) * (history[i].day_change - avg);
        }
        volatilityall = sqrt(volatility / (history.size() - 2));
    }

    int count;
    count = 30;
    if (history.size() >= count + 1) {
        double avg = 0;
        for (int i = history.size() - 1; i >= history.size() - count; i--) {
            avg += history[i].day_change;
        }
        avg /= count;
        double volatility = 0;
        for (int i = history.size() - 1; i >= history.size() - count; i--) {
            volatility += (history[i].day_change - avg) * (history[i].day_change - avg);
        }
        volatility30 = sqrt(volatility / (count-1));
    }
    else volatility30 = volatilityall;

    count = 100;
    if (history.size() >= count+1) {
        double avg = 0;
        for (int i = history.size() - 1; i >= history.size() - count; i--) {
            avg += history[i].day_change;
        }
        avg /= count;
        double volatility = 0;
        for (int i = history.size() - 1; i >= history.size() - count; i--) {
            volatility += (history[i].day_change - avg) * (history[i].day_change - avg);
        }
        volatility100 = sqrt(volatility / (count-1));
    }
    else volatility100 = volatilityall;

    count = 250;
    if (history.size() >= count+1) {
        double avg = 0;
        for (int i = history.size() - 1; i >= history.size() - count; i--) {
            avg += history[i].day_change;
        }
        avg /= count;
        double volatility = 0;
        for (int i = history.size() - 1; i >= history.size() - count; i--) {
            volatility += (history[i].day_change - avg) * (history[i].day_change - avg);
        }
        volatility250 = sqrt(volatility / (count-1));
    }
    else volatility250 = volatilityall;
    return true;
}