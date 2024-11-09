/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#include "StdAfx.h"
#include "FolderDefinition.h"

#include "TestCppClient.h"

#include "EClientSocket.h"
#include "EPosixClientSocketPlatform.h"

#include "Contract.h"
#include "Order.h"
#include "OrderState.h"
#include "Execution.h"
#include "CommissionReport.h"
#include "ContractSamples.h"
#include "OrderSamples.h"
#include "ScannerSubscription.h"
#include "ScannerSubscriptionSamples.h"
#include "executioncondition.h"
#include "PriceCondition.h"
#include "MarginCondition.h"
#include "PercentChangeCondition.h"
#include "TimeCondition.h"
#include "VolumeCondition.h"
#include "AvailableAlgoParams.h"
#include "FAMethodSamples.h"
#include "CommonDefs.h"
#include "AccountSummaryTags.h"
#include "Utils.h"
#include "Delta.h"

#include "StockAndAccount.h"
#include "SharedMemory.h"

#include <stdio.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <ctime>
#include <fstream>
#include <cstdint>
#include "curl_utils.hpp"
#include "time_utils.hpp"

extern std::vector<int> soundPlay;

const int PING_DEADLINE = 2; // seconds
const int SLEEP_BETWEEN_PINGS = 30; // seconds

int rprintf(const char* format, ...)
{
	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 12);

	//print time tag
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	printf("%02d:%02d:%02d REDMSG ", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);
	int result;
	va_list args;

	va_start(args, format);
	result = vprintf(format, args);
	va_end(args);

	SetConsoleTextAttribute(hConsole, 7);  //white
	return result;
}


inline std::string trim(std::string& str)
{
	str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
	str.erase(str.find_last_not_of('\r') + 1);         //surfixing spaces
	str.erase(str.find_last_not_of(' ') + 1);         //surfixing spaces
	return str;
}

bool scanValue(std::string& line, const char* targetString, double& value1, double& value2) {
	if (line.find(targetString) != std::string::npos) {
		int equPos = line.find("=");
		if (equPos == std::string::npos) return true;
		if (line.find(",") != std::string::npos) {
			std::string tmp = line.substr(equPos + 1, line.size() - equPos - 1);
			if (sscanf(tmp.c_str(), "%lf,%lf", &value1, &value2) != 2) {
				for (int i = 0; i < 10; i++)  rprintf("!!!Error Reading File: %s\n", line.c_str());
				std::this_thread::sleep_for(std::chrono::seconds(2));
			}
		}
		else {
			std::string tmp = line.substr(equPos + 1, line.size() - equPos - 1);
			if (sscanf(tmp.c_str(), "%lf", &value1) != 1) {
				for (int i = 0; i < 10; i++)  rprintf("Error Reading File: %s\n", line.c_str());
				std::this_thread::sleep_for(std::chrono::seconds(2));
			}
			value2 = value1;
		}

		printf("%s = %g, %g\n", targetString, value1, value2);
		return true;
	}
	return false;
}

bool scanValue(std::string& line, const char* targetString, double& value) {
	if (line.find(targetString) != std::string::npos) {
		int equPos = line.find("=");
		if (equPos == std::string::npos) return true;
		std::string tmp = line.substr(equPos + 1, line.size() - equPos - 1);
		if (sscanf(tmp.c_str(), "%lf", &value) != 1) {
			for (int i = 0; i < 10; i++)  rprintf("Error Reading File: %s\n", line.c_str());
			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
		printf("%s = %g\n", targetString, value);
		return true;
	}
	return false;
}

bool scanValue(std::string& line, const char* targetString, bool& value) {
	double tmp;
	if (scanValue(line, targetString, tmp)) {
		value = (tmp != 0);
		return true;
	}
	else return false;
}

bool scanValue(std::string& line, const char* targetString, std::string& value) {
	if (line.find(targetString) != std::string::npos) {
		int equPos = line.find("=");
		if (equPos == std::string::npos) return true;
		value = line.substr(equPos + 1, line.size() - equPos - 1);
		trim(value);
		printf("%s = %s\n", targetString, value.c_str());
		return true;
	}
	return false;
}

bool scanValue(std::string& line, const char* targetString, std::vector<double>& periods) {
	if (line.find(targetString) != std::string::npos) {
		periods.clear();
		printf("%s = ", targetString);
		int equPos = line.find("=");
		if (equPos == std::string::npos) {
			periods.push_back(0.0);
			return true;
		}
		std::string tmp = line.substr(equPos + 1, line.size() - equPos - 1);
		double value;
		while ((equPos = tmp.find(",")) != std::string::npos) {
			if (sscanf(tmp.c_str(), "%lf,", &value) != 1) {
				value = 0;
			}
			tmp = tmp.substr(equPos + 1, tmp.size() - equPos - 1);
			periods.push_back(value);
			printf("%g, ", value);
		}
		if (sscanf(tmp.c_str(), "%lf", &value) != 1) {
			value = 0;
		}
		periods.push_back(value);
		printf("%g\n", value);
		return true;
	}
	return false;
}

void TestCppClient::wangUpdateConrolFile()
{
	FILETIME t; DWORD fSize;
	HANDLE p = CreateFile(IB_CONTROLFILENAME, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (p == INVALID_HANDLE_VALUE) return;
	GetFileTime(p, NULL, NULL, &t); fSize = GetFileSize(p, NULL);
	CloseHandle(p);

	SYSTEMTIME sysTime;
	FileTimeToSystemTime(&t, &sysTime);
	struct tm time1 = { 0 };
	time1.tm_year = sysTime.wYear - 1900;
	time1.tm_mon = sysTime.wMonth - 1;
	time1.tm_mday = sysTime.wDay;
	time1.tm_hour = sysTime.wHour;
	time1.tm_min = sysTime.wMinute;
	time1.tm_sec = sysTime.wSecond;
	time_t fileTime = _mkgmtime(&time1);
	struct tm* ftm_struct = localtime(&fileTime);
	sysTime.wYear = ftm_struct->tm_year + 1900;
	sysTime.wMonth = ftm_struct->tm_mon + 1;
	sysTime.wDay = ftm_struct->tm_mday;
	sysTime.wHour = ftm_struct->tm_hour;
	sysTime.wDayOfWeek = ftm_struct->tm_wday;

	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	int year = tm_struct->tm_year + 1900;
	int mon = tm_struct->tm_mon + 1;
	int day = tm_struct->tm_mday;
	int wday = tm_struct->tm_wday;

	int cutmin = 5;
	//now in the week-end or monday before 4:05pm, and file time is after market of Friday.
	if ((wday == 0 || wday == 6 || (wday == 5 && (tm_struct->tm_hour > 16 || (tm_struct->tm_hour == 16 && tm_struct->tm_min >= cutmin)))|| (wday == 1 && (tm_struct->tm_hour < 16 || (tm_struct->tm_hour == 16 && tm_struct->tm_min < cutmin))))) {
		if ((sysTime.wDayOfWeek == 5 && (sysTime.wHour > 16 || (sysTime.wHour == 16 && sysTime.wMinute >= cutmin)) || sysTime.wDayOfWeek == 6 || sysTime.wDayOfWeek == 0 || sysTime.wDayOfWeek == 1)
			&& abs(fileTime - now_t) < 3600 * 24 * 3)
			return;
	}
	else if (sysTime.wDayOfWeek == wday) { //same day: file written after 4:05pm or current time is before 4:05pm
		if ((sysTime.wHour > 16 || (sysTime.wHour == 16 && sysTime.wMinute >= cutmin)) && abs(fileTime - now_t) < 3600 * 24)
			return;
		else if (tm_struct->tm_hour < 16 || (tm_struct->tm_hour == 16 && tm_struct->tm_min < cutmin))
			return;
	}
	else if (sysTime.wDayOfWeek == wday - 1) { // file written in previous day after 4:05, current time is before 4:05pm
		if ((sysTime.wHour > 16 || (sysTime.wHour == 16 && sysTime.wMinute >= cutmin)) && abs(fileTime - now_t) < 3600 * 24 * 2 && (tm_struct->tm_hour < 16 || (tm_struct->tm_hour == 16 && tm_struct->tm_min < cutmin)))
			return;
	}

	//all else need update ControlFile.
	for (int i = 0; i < m_stocknum; i++) {
		Stock* pos = &m_stock[i];
		if (pos->stdVariation != 0.0) {
			int lastYear, lastMon, lastDay;
			double todayClose, previousClose, volatility30, volatility100, volatility250;
			if (!getSTockHistoryInfo(pos->symbol.c_str(), lastYear, lastMon, lastDay, todayClose, previousClose, volatility30, volatility100, volatility250)) return;
			if (todayClose != 0.0) {
				double topBound = pos->stdVariation;
				double lowBound = -pos->stdVariation * (1 + pos->lowBoundDiscount);
				double fatChange = 0;
				if (todayClose / previousClose - 1.0 < lowBound) {
					fatChange = todayClose / previousClose / (1 + lowBound) - 1.0;
					//adjust periods array.
					double end = previousClose * (1 + lowBound);
					double start = todayClose;
					AdjustPeriods(pos->periods, start, end, false);
				}
				else if (todayClose / previousClose - 1.0 > topBound) {
					fatChange = todayClose / previousClose / (1 + topBound) - 1.0;
					//adjust periods array.
					double start = previousClose * (1 + topBound);
					double end = todayClose;
					AdjustPeriods(pos->periods, start, end, true);
				}
				//pos->topPrice *= (1 + fatChange);
				//pos->bottomPrice *= (1 + fatChange);
				pos->previousClose = todayClose;
				//pos->stdVariation = (volatility100 + volatility30)/2;
			}
			else if (fabs(previousClose - pos->previousClose) > 0.0001) {
				struct tm time2 = { 0 };
				time2.tm_year = lastYear - 1900;
				time2.tm_mon = lastMon - 1;
				time2.tm_mday = lastDay;
				time2.tm_hour = 16;
				time2.tm_min = 5;
				time2.tm_sec = 0;
				time_t stockTime = _mkgmtime(&time2);
				if (stockTime > fileTime) {
					double topBound = pos->stdVariation;
					double lowBound = -pos->stdVariation * (1 + pos->lowBoundDiscount);
					double fatChange = 0;
					if (previousClose / pos->previousClose - 1.0 < lowBound) {
						fatChange = previousClose / pos->previousClose / (1 + lowBound) - 1.0;
						double end = pos->previousClose * (1 + lowBound);
						double start = previousClose;
						AdjustPeriods(pos->periods, start, end, false);
					}
					else if (previousClose / pos->previousClose - 1.0 > topBound) {
						fatChange = previousClose / pos->previousClose / (1 + topBound) - 1.0;
						double start = pos->previousClose * (1 + topBound);
						double end = previousClose;
						AdjustPeriods(pos->periods, start, end, true);
					}
					//pos->topPrice *= (1 + fatChange);
					//pos->bottomPrice *= (1 + fatChange);
					pos->previousClose = previousClose;
					//pos->stdVariation = (volatility100 + volatility30) / 2;
				}
			}
		}
	}

	wangExportControlFile();
}

void TestCppClient::wangProcessControlFile(bool requestMarketData)
{
	static bool firsttime = true;
	static time_t now = time(NULL);
	time_t now_t = time(NULL);
	if (!firsttime && now_t - now < 2) return;
	firsttime = false;
	now = now_t;

	FILETIME t; DWORD fSize;
	HANDLE p = CreateFile(IB_CONTROLFILENAME, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (p == INVALID_HANDLE_VALUE) return;
	GetFileTime(p, NULL, NULL, &t); fSize = GetFileSize(p, NULL);
	if ((m_fileTime.dwHighDateTime || m_fileTime.dwLowDateTime) && CompareFileTime(&t, &m_fileTime) == 0) {
		CloseHandle(p);
		return;
	}
	m_fileTime = t;

	char* pContent = new char[fSize + 1]; pContent[fSize] = '\0';
	if (!ReadFile(p, pContent, fSize, &fSize, NULL)) {
		rprintf("Can not read control file\n");
		CloseHandle(p); return;
	}
	CloseHandle(p);

	//temporarily pause all the stock buy and sell.
	m_canBuy = m_canSell = false;
	m_alpacaAllTrading = true;
	int i;
	for (i = 0; i < m_stocknum; i++) {
		Stock & s = m_stock[i];
		s.canSell = s.canBuy = false;
		s.bottomPrice = s.topPrice = s.multiplier = s.offset = 0;
		s.targetBottomPrice = s.targetTopPrice = 0;
		s.oldBottomPrice = s.oldTopPrice = s.oldMultiplier = s.oldOffset = 0;
		s.targetMultiplier = -1; s.targetOffset = 1000000001;
		s.startTime = 9 * 60 + 30; //9:30
		s.endTime = 13 * 60 + 30; //13:30
		s.buyBelow = 10000.0;
		s.sellAbove = 0;
		s.buyAbove = 0;
		s.sellBelow = 10000.0;
		s.longTermHolding = 0;
		s.a1 = s.a2 = 0; s.comparison = false;
		s.reverse = false;
		s.canShort = false;   //canShort means that when long term account holding some position the short term account can short it. 
	}

	int stockIndex = -1;
	std::istringstream iss(pContent);  	delete[] pContent;

	std::string line;
	while (std::getline(iss, line))
	{
		double value1, value2;

		if (line.find("//") != std::string::npos) line = line.substr(0, line.find("//"));

		if (stockIndex == -1 && line.find(":") == std::string::npos) {
			continue;
		}
		else if (line.find("time") != std::string::npos) {
			int equPos = line.find("=");
			if (equPos == std::string::npos) continue;
			if (line.find(",") != std::string::npos) {
				std::string tmp = line.substr(equPos + 1, line.size() - equPos - 1);
				int hour1, hour2, min1, min2;
				if (sscanf(tmp.c_str(), "%d:%d,%d:%d", &hour1, &min1, &hour2, &min2) != 4) {
					rprintf("Error Reading File: %s\n", line.c_str());
					std::this_thread::sleep_for(std::chrono::seconds(2));
				}
				if (hour1 <= 4) hour1 += 12;
				if (hour2 <= 4 || hour2 < hour1) hour2 += 12; if (hour2 > 20) hour2 -= 12;
				m_stock[stockIndex].startTime = hour1 * 60 + min1;
				m_stock[stockIndex].endTime = hour2 * 60 + min2;
				printf("time=%d:%d, %d:%d\n", hour1, min1, hour2, min2);
			}
			else {
				continue;
			}
		}
		else if (line.find(":") != std::string::npos) {
			std::string sym = line.substr(0, line.find(":"));
			if (sym == "ALL") {
				stockIndex = 99;
				continue;
			}
			add_shared_symbol(sym);

			std::string exchange = "";
			if (sym.find("@") != std::string::npos) {
				exchange = sym.substr(sym.find("@") + 1, sym.size());
				sym = sym.substr(0, sym.find("@"));
			}
			for (i = 0; i < m_stocknum; i++) {
				if (m_stock[i].symbol == sym) {
					stockIndex = i;
					break;
				}
			}
			if (i == m_stocknum) {
				stockIndex = i;
				m_stocknum++;
				m_stock[stockIndex].symbol = sym;
				m_stock[stockIndex].tikerID = addOneStock(sym);
				if (exchange != "") {
					m_stock[stockIndex].primaryExchange = exchange;
					allStock[getStockIndex(sym)].primaryExchange = exchange;
				}

			}
			if (exchange == "") printf("%s:\n", m_stock[stockIndex].symbol.c_str());
			else printf("%s@%s:\n", m_stock[stockIndex].symbol.c_str(), m_stock[stockIndex].primaryExchange.c_str());
		}
		else if (scanValue(line, "bottomPrice", m_stock[stockIndex].bottomPrice, m_stock[stockIndex].targetBottomPrice)) continue;
		else if (scanValue(line, "topPrice", m_stock[stockIndex].topPrice, m_stock[stockIndex].targetTopPrice)) continue;
		else if (scanValue(line, "multiplier", m_stock[stockIndex].multiplier, m_stock[stockIndex].targetMultiplier)) continue;
		else if (scanValue(line, "offset", m_stock[stockIndex].offset, m_stock[stockIndex].targetOffset)) continue;
		else if (scanValue(line, "buySell", value1, value2)) {
			if (stockIndex == 99) {
				m_canBuy = (value1 != 0);
				m_canSell = (value2 != 0);
				printf("ALL: m_canBuy = %s, m_canSell = %s\n", m_canBuy?"true":"false", m_canSell?"true":"false");
			}
			else {
				m_stock[stockIndex].canBuy = (value1 != 0);
				m_stock[stockIndex].canSell = (value2 != 0);
				printf("canBuy = %s, canSell = %s\n", m_stock[stockIndex].canBuy ? "true" : "false", m_stock[stockIndex].canSell ? "true" : "false");
			}
			continue;
		}
		else if (scanValue(line, "fatTailBuyShortEnbled", value1, value2)) {
			m_stock[stockIndex].fatTailBuyEnbled = (value1 != 0);
			m_stock[stockIndex].fatTailShortEnbled = (value2 != 0);
			printf("fatTailBuyEnbled = %s, fatTailShortEnbled = %s\n", m_stock[stockIndex].fatTailBuyEnbled ? "true" : "false", m_stock[stockIndex].fatTailShortEnbled ? "true" : "false");
			continue;
		}
		else if (scanValue(line, "fatTailBuyShortDone", value1, value2)) {
			m_stock[stockIndex].fatTailBuyDone = (value1 != 0);
			m_stock[stockIndex].fatTailShortDone = (value2 != 0);
			printf("fatTailBuyDone = %s, fatTailShortDone = %s\n", m_stock[stockIndex].fatTailBuyDone ? "true" : "false", m_stock[stockIndex].fatTailShortDone ? "true" : "false");
			continue;
		}
		else if (scanValue(line, "periods", m_stock[stockIndex].periods)) continue;
		else if (scanValue(line, "alpacaAllTrading", m_alpacaAllTrading)) continue;
		else if (scanValue(line, "longTermHolding", m_stock[stockIndex].longTermHolding)) continue;
		else if (scanValue(line, "previousClose", m_stock[stockIndex].previousClose)) continue;
		else if (scanValue(line, "stdVariation", m_stock[stockIndex].stdVariation)) continue;
		else if (scanValue(line, "lowBoundDiscount", m_stock[stockIndex].lowBoundDiscount)) continue;
		else if (scanValue(line, "hedgeRatio", m_stock[stockIndex].hedgeRatio)) continue;
		else if (scanValue(line, "takenProfit", m_stock[stockIndex].takenProfit)) continue;
		else if (scanValue(line, "buyBelow", m_stock[stockIndex].buyBelow)) continue;
		else if (scanValue(line, "sellAbove", m_stock[stockIndex].sellAbove)) continue;
		else if (scanValue(line, "buyAbove", m_stock[stockIndex].buyAbove)) continue;
		else if (scanValue(line, "sellBelow", m_stock[stockIndex].sellBelow)) continue;
		else if (scanValue(line, "canShort", m_stock[stockIndex].canShort)) continue;
		else if (scanValue(line, "reverse", m_stock[stockIndex].reverse)) continue;
		else if (scanValue(line, "shared", m_stock[stockIndex].shared)) continue;
		else if (scanValue(line, "alpacaTrading", m_stock[stockIndex].alpacaTrading)) continue;
		else if (scanValue(line, "maxAlpacaMarketValue", m_stock[stockIndex].maxAlpacaMarketValue)) continue;
		else if (scanValue(line, "impliedVolatility", m_stock[stockIndex].impliedVolatility)) continue;
	}

	printf("***********************\n");
	for (i = 0; i < m_stocknum; i++) {
		m_stock[i].oldBottomPrice = m_stock[i].bottomPrice;
		m_stock[i].oldTopPrice = m_stock[i].topPrice;
		m_stock[i].oldMultiplier = m_stock[i].multiplier;
		m_stock[i].oldOffset = m_stock[i].offset;
		if (m_stock[i].targetBottomPrice == 0) m_stock[i].targetBottomPrice = m_stock[i].bottomPrice;
		if (m_stock[i].targetTopPrice == 0) m_stock[i].targetTopPrice = m_stock[i].topPrice;
		if (m_stock[i].targetMultiplier < 0) m_stock[i].targetMultiplier = m_stock[i].multiplier;
		if (m_stock[i].targetOffset > 1000000) m_stock[i].targetOffset = m_stock[i].offset;

		//output the one with different
		if (fabs(m_stock[i].targetBottomPrice - m_stock[i].bottomPrice) > 0.001) {
			printf("%s Bottom price different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].bottomPrice, m_stock[i].targetBottomPrice);
		}
		if (fabs(m_stock[i].targetTopPrice - m_stock[i].topPrice) > 0.001) {
			printf("%s Top price different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].topPrice, m_stock[i].targetTopPrice);
		}
		if (fabs(m_stock[i].targetMultiplier - m_stock[i].multiplier) > 0.01) {
			printf("%s Multiplier different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].multiplier, m_stock[i].targetMultiplier);
		}
		if (fabs(m_stock[i].targetOffset - m_stock[i].offset) > 0.1) {
			printf("%s Offset different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].offset, m_stock[i].targetOffset);
		}

		/*
		if (fabs(m_stock[i].targetBottomPrice - m_stock[i].bottomPrice) / m_stock[i].bottomPrice >= 0.5) {
			printf("Wrong target BottomPrice!!!!!!!!!!!!!\n");
			m_stock[i].targetBottomPrice = m_stock[i].bottomPrice;
		}
		if (fabs(m_stock[i].targetTopPrice - m_stock[i].topPrice) / m_stock[i].topPrice >= 0.5) {
			printf("Wrong target TopPrice!!!!!!!!!!!!!\n");
			m_stock[i].targetTopPrice = m_stock[i].topPrice;
		}
		if (fabs(m_stock[i].targetMultiplier - m_stock[i].multiplier) / m_stock[i].multiplier >= 0.15) {
			printf("Wrong target multiplier!!!!!!!!!!!!!\n");
			m_stock[i].targetMultiplier = m_stock[i].multiplier;
		}
		*/
	}
	printf("***********************\n");
	std::this_thread::sleep_for(std::chrono::milliseconds(1000)); //wait for other process reading its file a little.
}

//not updated for new control variables
void TestCppClient::wangExportControlFile(bool copyAlpaca)
{

	//all else need update ControlFile.
	char line[200];
	std::string s = "ALL:\n";
	sprintf(line, "buySell = %d, %d\nalpacaAllTrading = %d\n", m_canBuy ? 1 : 0, m_canSell ? 1 : 0, m_alpacaAllTrading ? 1 : 0);
	s += line;
	for (int i = 0; i < m_stocknum; i++) {
		Stock* pos = &m_stock[i];

		if (pos->primaryExchange == "")
			sprintf(line, "\n%s:\n", pos->symbol.c_str());
		else
			sprintf(line, "\n%s@%s:\n", pos->symbol.c_str(), pos->primaryExchange.c_str());
		s += line;
		sprintf(line, "longTermHolding = %g\n", pos->longTermHolding);
		s += line;
		sprintf(line, "topPrice = %g\n", pos->topPrice);
		s += line;
		sprintf(line, "bottomPrice = %g\n", pos->bottomPrice);
		s += line;
		if (pos->stdVariation != 0.0) {
			sprintf(line, "previousClose = %g\n", pos->previousClose);
			s += line;
			sprintf(line, "stdVariation = %g\n", pos->stdVariation);
			s += line;
			sprintf(line, "lowBoundDiscount = %g\n", pos->lowBoundDiscount);
			s += line;
		}
		sprintf(line, "multiplier = %g\n", pos->multiplier);
		s += line;
		sprintf(line, "offset = %g\n", pos->offset);
		s += line;
		//periods
		if (pos->periods.size() > 0) {
			sprintf(line, "periods = ");
			s += line;
			for (int j = 0; j < pos->periods.size() - 1; j++) {
				sprintf(line, "%g, ", pos->periods[j]);
				s += line;
			}
			sprintf(line, "%g\n", pos->periods[pos->periods.size() - 1]);
			s += line;
		}


		sprintf(line, "hedgeRatio = %g\n", pos->hedgeRatio);
		s += line;
		sprintf(line, "takenProfit = %g\n", pos->takenProfit);
		s += line;
		sprintf(line, "maxAlpacaMarketValue = %g\n", pos->maxAlpacaMarketValue);
		s += line;
		sprintf(line, "buySell = %d, %d\n", pos->canBuy ? 1 : 0, pos->canSell ? 1 : 0);
		s += line;
		sprintf(line, "fatTailBuyShortEnbled = %d, %d\n", pos->fatTailBuyEnbled ? 1 : 0, pos->fatTailShortEnbled ? 1 : 0);
		s += line;
		sprintf(line, "fatTailBuyShortDone = %d, %d\n", pos->fatTailBuyDone ? 1 : 0, pos->fatTailShortDone ? 1 : 0);
		s += line;
		sprintf(line, "canShort = %d\n", pos->canShort ? 1 : 0);
		s += line;
		sprintf(line, "shared = %d\n", pos->shared ? 1 : 0);
		s += line;
		sprintf(line, "time = %d:%02d, %d:%02d\n", (int)pos->startTime / 60, (int)pos->startTime % 60, (int)pos->endTime / 60, (int)pos->endTime % 60);
		s += line;
	}

	FILE* f;
	f = fopen(IB_CONTROLFILENAME, "w");
	if (!f) return;
	fprintf(f, "%s", s.c_str());
	fclose(f);

	if (copyAlpaca) {
		f = fopen(AL_CONTROLFILENAME1, "w");
		if (!f) return;
		fprintf(f, "%s", s.c_str());
		fclose(f);
	}
}

void TestCppClient::wangShareOrderInfo(MyOrder* o)
{
	if (!o) return;
	std::string id = std::to_string(o->orderId);
	updateShareOrder(id, o->symbol, o->action, o->status, o->totalQty, o->filledQty, o->lmtPrice, o->profit, o->commission, "IB");
	if (o->status == "Filled" && fabs(o->profit) < 0.01) rprintf("shared filled orders with profit 0\n");
}

///////////////////////////////////////////////////////////
// member funcs
//! [socket_init]
//! To compare a stock for different profit taken stagies, set comparison = true, and set a1 = the first account shares, a2 = the second account shares.
TestCppClient::TestCppClient() :
      m_osSignal(2000)//2-seconds timeout
    , m_pClient(new EClientSocket(this, &m_osSignal))
	, m_state(ST_CONNECT)
	, m_sleepDeadline(0)
	, m_orderId(0)
    , m_pReader(0)
    , m_extraAuth(false)
{
	m_fileTime.dwHighDateTime = m_fileTime.dwLowDateTime = 0;
	m_stocknum = 0;

	m_canSell = true;
	m_canBuy = true;

	int i = 0;
	wangProcessControlFile(false);

	m_positionRequestStatus = m_openOrderRequestStatus = 0;

}


//! [socket_init]
TestCppClient::~TestCppClient()
{
    if (m_pReader)
        delete m_pReader;

    delete m_pClient;

	for (unsigned int i = 0; i < m_orderList.size(); i++) {
		delete m_orderList[i];
	}

}

bool TestCppClient::connect(const char *host, int port, int clientId)
{
	// trying to connect
	printf( "Connecting to %s:%d clientId:%d\n", !( host && *host) ? "127.0.0.1" : host, port, clientId);
	
	//! [connect]
	bool bRes = m_pClient->eConnect( host, port, clientId, m_extraAuth);
	//! [connect]
	
	if (bRes) {
		printf( "Connected to %s:%d clientId:%d\n", m_pClient->host().c_str(), m_pClient->port(), clientId);
		//! [ereader]
        m_pReader = new EReader(m_pClient, &m_osSignal);
		m_pReader->start();
		//! [ereader]
	}
	else
		printf( "Cannot connect to %s:%d clientId:%d\n", m_pClient->host().c_str(), m_pClient->port(), clientId);

	return bRes;
}

void TestCppClient::disconnect() const
{
	m_pClient->eDisconnect();

	rprintf ( "Disconnected\n");
}

bool TestCppClient::isConnected() const
{
	return m_pClient->isConnected();
}

void TestCppClient::setConnectOptions(const std::string& connectOptions)
{
	m_pClient->setConnectOptions(connectOptions);
}

void TestCppClient::processMessages()
{
	time_t now = time(NULL);

	/*****************************************************************/
    /* Below are few quick-to-test examples on the IB API functions grouped by functionality. Uncomment the relevant methods. */
    /*****************************************************************/
	switch (m_state) {
		case ST_PNLSINGLE:
			pnlSingleOperation();
			break;
		case ST_PNLSINGLE_ACK:
			break;
		case ST_PNL:
			pnlOperation();
			break;
		case ST_PNL_ACK:
			break;
		case ST_TICKDATAOPERATION:
			tickDataOperation();
			break;
		case ST_TICKDATAOPERATION_ACK:
			break;
		case ST_TICKOPTIONCOMPUTATIONOPERATION:
			tickOptionComputationOperation();
			break;
		case ST_TICKOPTIONCOMPUTATIONOPERATION_ACK:
			break;
		case ST_DELAYEDTICKDATAOPERATION:
			delayedTickDataOperation();
			break;
		case ST_DELAYEDTICKDATAOPERATION_ACK:
			break;
		case ST_MARKETDEPTHOPERATION:
			marketDepthOperations();
			break;
		case ST_MARKETDEPTHOPERATION_ACK:
			break;
		case ST_REALTIMEBARS:
			realTimeBars();
			break;
		case ST_REALTIMEBARS_ACK:
			break;
		case ST_MARKETDATATYPE:
			marketDataType();
			break;
		case ST_MARKETDATATYPE_ACK:
			break;
		case ST_HISTORICALDATAREQUESTS:
			historicalDataRequests();
			break;
		case ST_HISTORICALDATAREQUESTS_ACK:
			break;
		case ST_OPTIONSOPERATIONS:
			optionsOperations();
			break;
		case ST_OPTIONSOPERATIONS_ACK:
			break;
		case ST_CONTRACTOPERATION:
			contractOperations();
			break;
		case ST_CONTRACTOPERATION_ACK:
			break;
		case ST_MARKETSCANNERS:
			marketScanners();
			break;
		case ST_MARKETSCANNERS_ACK:
			break;
		case ST_FUNDAMENTALS:
			fundamentals();
			break;
		case ST_FUNDAMENTALS_ACK:
			break;
		case ST_BULLETINS:
			bulletins();
			break;
		case ST_BULLETINS_ACK:
			break;
		case ST_ACCOUNTOPERATIONS:
			accountOperations();
			break;
		case ST_ACCOUNTOPERATIONS_ACK:
			break;
		case ST_ORDEROPERATIONS:
			orderOperations();
			break;
		case ST_ORDEROPERATIONS_ACK:
			break;
		case ST_OCASAMPLES:
			ocaSamples();
			break;
		case ST_OCASAMPLES_ACK:
			break;
		case ST_CONDITIONSAMPLES:
			conditionSamples();
			break;
		case ST_CONDITIONSAMPLES_ACK:
			break;
		case ST_BRACKETSAMPLES:
			bracketSample();
			break;
		case ST_BRACKETSAMPLES_ACK:
			break;
		case ST_HEDGESAMPLES:
			hedgeSample();
			break;
		case ST_HEDGESAMPLES_ACK:
			break;
		case ST_TESTALGOSAMPLES:
			testAlgoSamples();
			break;
		case ST_TESTALGOSAMPLES_ACK:
			break;
		case ST_FAORDERSAMPLES:
			financialAdvisorOrderSamples();
			break;
		case ST_FAORDERSAMPLES_ACK:
			break;
		case ST_FAOPERATIONS:
			financialAdvisorOperations();
			break;
		case ST_FAOPERATIONS_ACK:
			break;
		case ST_DISPLAYGROUPS:
			testDisplayGroups();
			break;
		case ST_DISPLAYGROUPS_ACK:
			break;
		case ST_MISCELANEOUS:
			miscelaneous();
			break;
		case ST_MISCELANEOUS_ACK:
			break;
		case ST_FAMILYCODES:
			reqFamilyCodes();
			break;
		case ST_FAMILYCODES_ACK:
			break;
		case ST_SYMBOLSAMPLES:
			reqMatchingSymbols();
			break;
		case ST_SYMBOLSAMPLES_ACK:
			break;
		case ST_REQMKTDEPTHEXCHANGES:
			reqMktDepthExchanges();
			break;
		case ST_REQMKTDEPTHEXCHANGES_ACK:
			break;
		case ST_REQNEWSTICKS:
			reqNewsTicks();
			break;
		case ST_REQNEWSTICKS_ACK:
			break;
		case ST_REQSMARTCOMPONENTS:
			reqSmartComponents();
			break;
		case ST_REQSMARTCOMPONENTS_ACK:
			break;
		case ST_NEWSPROVIDERS:
			reqNewsProviders();
			break;
		case ST_NEWSPROVIDERS_ACK:
			break;
		case ST_REQNEWSARTICLE:
			reqNewsArticle();
			break;
		case ST_REQNEWSARTICLE_ACK:
			break;
		case ST_REQHISTORICALNEWS:
			reqHistoricalNews();
			break;
		case ST_REQHISTORICALNEWS_ACK:
			break;
		case ST_REQHEADTIMESTAMP:
			reqHeadTimestamp();
			break;
		case ST_REQHISTOGRAMDATA:
			reqHistogramData();
			break;
		case ST_REROUTECFD:
			rerouteCFDOperations();
			break;
		case ST_MARKETRULE:
			marketRuleOperations();
			break;
		case ST_CONTFUT:
			continuousFuturesOperations();
			break;
        case ST_REQHISTORICALTICKS:
            reqHistoricalTicks();
            break;
        case ST_REQHISTORICALTICKS_ACK:
            break;
		case ST_REQTICKBYTICKDATA:
			reqTickByTickData();
			break;
		case ST_REQTICKBYTICKDATA_ACK:
			break;
		case ST_WHATIFSAMPLES:
			whatIfSamples();
			break;
		case ST_WHATIFSAMPLES_ACK:
			break;
		case ST_PING:
			reqCurrentTime();
			break;
		case ST_PING_ACK:
			if( m_sleepDeadline < now) {
				disconnect();
				return;
			}
			break;
		case ST_IDLE:
			if( m_sleepDeadline < now) {
				m_state = ST_PING;
				return;
			}
			break;
	}

	m_osSignal.waitForSignal();
	errno = 0;
	m_pReader->processMsgs();
}


void TestCppClient::wangPrintOrders() {
#ifndef NO_OUTPUT
	printf("############Start Order List###########\n");
	for (unsigned int i = 0; i < m_orderList.size(); i++) {
		MyOrder * o = m_orderList[i];
		printf("%s %d %s %s <%s> %g(%g) @ %g\n", o->account.c_str(), o->orderId, o->symbol.c_str(), o->action.c_str(), o->status.c_str(), o->totalQty, o->remaining, o->avgFillPrice);
	}
	printf("############End Order List###########\n");
#endif

	printf("-----Interactive Brokers -----\n");
	double totalProfit = 0;
	double totalCommission = 0;
	for (int cp = 0; cp < 2; cp++)
		for (int index = 0; index < m_stocknum; index++) {
			Stock & s = m_stock[index];
			if (s.askPrice < 1.0 || s.bidPrice < 1.0) continue;
			if (cp == 1 && !s.comparison) continue;
			s.opPosition[0].getOptionDelta(s.symbol, (s.askPrice + s.bidPrice) / 2, s.impliedVolatility);
			s.opPosition[1].getOptionDelta(s.symbol, (s.askPrice + s.bidPrice) / 2, s.impliedVolatility);

			double nShare = 0, nCash = 0;
			double earnedProfit = 0.0;
			for (int i = m_orderList.size() - 1; i >= 0; i--) {
				MyOrder * o = m_orderList[i];
				if (o->status == "Filled" && o->symbol == s.symbol) {

					if (s.comparison) {
						if (o->account == ACCOUNT1 && cp != 0) continue;
						if (o->account == ACCOUNT2 && cp != 1) continue;
						double offset = s.offset / 2;
						if (cp == 0) {
							offset += (s.a1 - s.a2) / 2;
						}
						else {
							offset += (s.a2 - s.a1) / 2;
						}
						if (o->action == "SELL") {
							nShare -= o->filledQty;
							nCash += o->filledQty * o->avgFillPrice;
							double delta = deltaCalculation(o->avgFillPrice, s.bottomPrice, s.topPrice, s.multiplier / 2, offset, s.periods);
							delta += o->filledQty;
							double price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier / 2, offset, s.periods);
							earnedProfit += o->filledQty*(o->avgFillPrice - price)*0.5;
							earnedProfit -= o->commission;
						}
						else {
							nShare += o->filledQty;
							nCash -= o->filledQty * o->avgFillPrice;
							double delta = deltaCalculation(o->avgFillPrice, s.bottomPrice, s.topPrice, s.multiplier / 2, offset, s.periods);
							delta -= o->filledQty;
							double price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier / 2, offset, s.periods);
							earnedProfit -= o->filledQty*(o->avgFillPrice - price)*0.5;
							earnedProfit -= o->commission;
						}
					}
					else {

						if (o->action == "SELL") {
							nShare -= o->filledQty;
							nCash += o->filledQty * o->avgFillPrice;
							earnedProfit += o->profit;
							earnedProfit -= o->commission;
						}
						else {
							nShare += o->filledQty;
							nCash -= o->filledQty * o->avgFillPrice;
							earnedProfit += o->profit;
							earnedProfit -= o->commission;
						}
					}
					totalCommission += o->commission;
				}
			}

			double tdpos = 0;
			if (s.shared) read_Td_position(s.symbol, tdpos);
			//double earnedProfit = nCash + nShare * (s.askPrice + s.bidPrice) / 2;
			if (s.comparison) {
				totalProfit += earnedProfit;
				printf("Stock %s: Shares %g, cash %g; Earned Profit is %g (TD Position %g)\n", s.symbol.c_str(), nShare, nCash, round(earnedProfit, 0.01), tdpos);
			}
			else {
				totalProfit += s.totalProfit;
				printf("Stock %s: Shares %g, cash %g; Total Profit %g - %g = %g (TD Position %g)\n", s.symbol.c_str(), nShare, nCash, round(s.totalProfit, 0.01), round(s.totalCommission, 0.01), round(s.totalProfit - s.totalCommission, 0.01), tdpos);
			}
		}
	printf("Total Profit is %g, and total Commission is %g\n", round(totalProfit, 0.01), round(totalCommission, 0.01));
	printf("Account Avaible Funds is %g, Excess Liquidity is %g\n", round(m_account[0].availableFunds), round(m_account[0].excessLiquidity));
}

void TestCppClient::modifyLmtOrder(int orderId, double newSize, double newPrice)
{
	for (unsigned int i = 0; i < m_orderList.size(); i++) {
		MyOrder * o = m_orderList[i];
		if (o->orderId != orderId) continue;
		if (o->status != "Submitted" && o->status != "PreSubmitted" && o->status != "PendingSubmit") continue;
		if (o->status == "Submitted" && o->partiallyFilled && fabs(o->lmtPrice - newPrice)/o->lmtPrice < 0.003 && fabs(newSize - o->remaining) > max(min(o->remaining/3, 10), 2)) continue;
		if (o->orderType != "LMT") continue;
		if (fabs(o->tryPrice - newPrice) < 0.001 && fabs(o->trySize - newSize) < 0.001 && abs(time(NULL) - o->tryTime) <= 2) continue;

//		if (o->symbol == "RCL") {
	//		int a = 1;
		//}

		if (o->partiallyFilled) {
			m_pClient->cancelOrder(o->orderId);    //cancel it and create a new order. Since we can not save commission fees any more.
			soundPlay.push_back(3);
			return;
		}

		Contract contract;
		contract.symbol = o->symbol;
		contract.secType = "STK";
		contract.currency = o->currency;
		//In the API side, NASDAQ is always defined as ISLAND
		contract.exchange = o->exchange;
		//contract.primaryExchange = "ARCA";
		m_pClient->placeOrder(orderId, contract, OrderSamples::LimitOrder(o->action, newSize + o->filledQty, newPrice, o->account));
#ifndef NO_OUTPUT
		printf("Modified Order: %d %s %s %g %g\n", orderId, o->symbol.c_str(), o->action.c_str(), newSize + o->filledQty, newPrice);
#endif
		o->tryPrice = newPrice;
		o->trySize = newSize;
		o->tryTime = time(NULL);
	}
}

void TestCppClient::wangRequestMarketData()
{
	//m_pClient->reqIds(-1);
	read_shared_symbol_to_allStock();
	//! [reqmktdata_ticknews]
	int i;
	for (i = 0; i < allStockNum; i++) {
		if (allStock[i].dataRequested) continue;
		Contract contract;
		contract.symbol = allStock[i].symbol;
		contract.secType = "STK";
		contract.currency = allStock[i].currency;
		if (contract.currency == "USD") {
			contract.exchange = "SMART";
			//if (contract.symbol == "ABNB") contract.primaryExchange = "ARCA";
			if (allStock[i].primaryExchange != "") contract.primaryExchange = allStock[i].primaryExchange;
			m_pClient->reqMktData(allStock[i].tikerID, contract, "", false, false, TagValueListSPtr());
			printf("Request Market Data of %s, tikerID %d\n", contract.symbol.c_str(), allStock[i].tikerID);
		}
		else {
			contract.exchange = "SEHKSZSE";
			m_pClient->reqMktData(allStock[i].tikerID, contract, "", true, false, TagValueListSPtr());
		}
		allStock[i].dataRequested = true;
	}
}

void TestCppClient::wangRequest()
{
	wangUpdateOrdersAndPositions();

	m_pClient->reqAccountSummary(9001, "All", AccountSummaryTags::getAllTags());
}

void TestCppClient::wangUpdateOrdersAndPositions()
{
	//request OpenOrders, Positions.
	if (m_stocknum > 0) {
		m_openOrderRequestStatus = 1;
		m_positionRequestStatus = 1;
		m_pClient->reqOpenOrders();
		m_pClient->reqPositions();
		m_osSignal.waitForSignal();
		errno = 0;
		m_pReader->processMsgs();
	}

	int i;
	for (i = 0; i < 15*10; i++) { //wait maximum 15 seconds
		if (i == 20) rprintf("UpdateOrdersAndPositions Waiting up to 15 seconds, waited 2 seconds already ...\n");
		if (m_openOrderRequestStatus == 1 || m_positionRequestStatus == 1) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			m_osSignal.waitForSignal();
			errno = 0;
			m_pReader->processMsgs();
		}
		else
			break;
	}
	if (i >= 15*10) rprintf("!!! FAILED UpdateOrdersAndPositions !!!\n");
}

/*

double deltaCalculation(double price, double bottomPrice, double topPrice, double multiplier, double offset)
{
	double delta;
	if (price < topPrice && price > bottomPrice) delta = log(topPrice / price) / log(topPrice / bottomPrice);
	if (price < bottomPrice) delta = -1;

	delta *= multiplier * 100;
	return round(delta + offset);
}
*/

void TestCppClient::ProfitTakenAdjust() {
	int i;
	for (int index = 0; index < m_stocknum; index++) {
		Stock & s = m_stock[index];
		if (s.askPrice < 1.0 || s.bidPrice < 1.0) continue;

		std::vector <ExecTime> timeSeries;
		for (i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder * o = m_orderList[i];
			if (o->status == "Filled" && o->symbol == s.symbol) {
				if (s.comparison && o->account != ACCOUNT1) continue;
				struct tm *tm_struct = localtime(&o->fillTime);
				int hour = tm_struct->tm_hour;
				int minute = tm_struct->tm_min;
				if (hour >= 16 || hour < 9 || (hour == 9 && minute < 30)) continue;
				ExecTime e;
				e.action = o->action;
				e.fillTime = o->fillTime;
				e.qty = o->filledQty;
				timeSeries.push_back(e);
			}
		}

		printf("Current %s profit taken is %g\n", s.symbol.c_str(), s.takenProfit);

		if (timeSeries.size() < 1) continue;
		else if (timeSeries.size() == 1) {
			time_t elapse = time(NULL) - timeSeries[0].fillTime;
			int jump = 120; //every 2 minutes;
			if (elapse < jump) continue;
			int count = (int)elapse / jump;
			if (elapse >= jump * count && elapse < jump*count + 20 && s.takenProfit > 4.5) {
				s.takenProfit -= 1.0;
				printf("Decrease %s profit taken to %g\n", s.symbol.c_str(), s.takenProfit);
			}
		}
		else if (timeSeries.size() >= 2) {
			int fastFill = 0;
			for (unsigned int c = 1; c < timeSeries.size(); c++) {
				if (abs(timeSeries[0].fillTime - timeSeries[c].fillTime) < 60) fastFill++;  //less than 1 minutes.
			}
			time_t elapse = time(NULL) - timeSeries[0].fillTime;
			int jump = 120; //every 2 minutes;
			if (elapse < 20 && fastFill >=1 && s.takenProfit < 9.5) { //less than 2 minutes, we can increase the profit taken time.
				s.takenProfit += fastFill;
				if (s.takenProfit > 10.0) s.takenProfit = 10.0;
				printf("Increase %s profit taken to %g\n", s.symbol.c_str(), s.takenProfit);
			}
			if (elapse < jump) continue;
			int count = (int)elapse / jump;
			if (elapse >= jump * count && elapse < jump*count + 20 && s.takenProfit > 4.5) {
				s.takenProfit -= 1.0;
				printf("Decrease %s profit taken to %g\n", s.symbol.c_str(), s.takenProfit);
			}
		}
	}
}


void TestCppClient::wangSleep(int nMillisecs)
{
	for (int i = 0; i < nMillisecs / 25; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		m_pReader->processMsgs();
	}
}

void TestCppClient::wangJob()
{
	static int count = 0;
	static time_t now;
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	int hour = tm_struct->tm_hour;
	int minute = tm_struct->tm_min;
	int second = tm_struct->tm_sec;

	wangProcessControlFile();
	wangRequestMarketData();
	wangUpdateConrolFile();
	for (int stockindex = 0; stockindex < m_stocknum; stockindex++) {
		Stock& s = m_stock[stockindex];
		double rate = 0;
		if (s.startTime < s.endTime) rate = (hour * 60 + minute - s.startTime + tm_struct->tm_sec/10*10 / 60.0) / (s.endTime - s.startTime);
		if (rate > 1.0) rate = 1.0;
		if (rate < 0) rate = 0;
		s.topPrice = s.oldTopPrice + (s.targetTopPrice - s.oldTopPrice) * rate;
		s.bottomPrice = s.oldBottomPrice + (s.targetBottomPrice - s.oldBottomPrice) * rate;
		s.multiplier = s.oldMultiplier + (s.targetMultiplier - s.oldMultiplier) * rate;
		//if (s.multiplier < 0.01) s.multiplier = 0.01;
		if (s.topPrice <= s.bottomPrice) s.topPrice = s.bottomPrice + 0.02;
		s.offset = s.oldOffset + (s.targetOffset - s.oldOffset) * rate;
		update_stock_range(s.symbol, s.topPrice, s.bottomPrice, s.multiplier, s.offset, s.previousClose, s.stdVariation, s.lowBoundDiscount);
	}

	if (hour >= 20 || hour < 4 || (hour == 4 && minute < 1)) {  //out of trading hours: empty order list, and just wait...
		m_pReader->processMsgs();
		printf("current time is %d:%d\n", hour, minute);
		if (!m_orderList.empty()) {
			for (int i = m_orderList.size() - 1; i >= 0; i--) {
				MyOrder * o = m_orderList[i];
				delete o;
				m_orderList.erase(m_orderList.begin() + i);
			}
		}
		for (int i = 0; i < m_stocknum; i++) {
			m_stock[i].totalProfit = m_stock[i].totalCommission = 0.0;
			//output the one with different
			if (fabs(m_stock[i].targetBottomPrice - m_stock[i].oldBottomPrice) > 0.001) {
				printf("%s Bottom price different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].oldBottomPrice, m_stock[i].targetBottomPrice);
			}
			if (fabs(m_stock[i].targetTopPrice - m_stock[i].oldTopPrice) > 0.001) {
				printf("%s Top price different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].oldTopPrice, m_stock[i].targetTopPrice);
			}
			if (fabs(m_stock[i].targetMultiplier - m_stock[i].oldMultiplier) > 0.01) {
				printf("%s Multiplier different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].oldMultiplier, m_stock[i].targetMultiplier);
			}
			if (fabs(m_stock[i].targetOffset - m_stock[i].oldOffset) > 0.1) {
				printf("%s Offset different: %g, %g\n", m_stock[i].symbol.c_str(), m_stock[i].oldOffset, m_stock[i].targetOffset);
			}
		}
		clearSharedTickAndOrders();
		std::this_thread::sleep_for(std::chrono::seconds(30));
		return;
	}

	/*
	for (int ia = 0; ia < allStockNum; ia++) {
		if (allStock[ia].closePrice == 0.0) {
			//m_pClient->cancelMktData(allStock[ia].tikerID);
			allStock[ia].dataRequested = false;
		}
		else {
			int i;
			for (i = 0; i < m_stocknum; i++) {
				if (m_stock[i].tikerID == allStock[ia].tikerID) break;
			}
			if (i < m_stocknum) {
				Stock& s = m_stock[i];
				if (fabs(s.closePrice - s.previousClose) > 0.001) {
					rprintf("%s Close Price does not match, %g, %g", m_stock[i].symbol.c_str(), m_stock[i].closePrice, m_stock[i].previousClose);
				}
			}
		}
	}
	*/

	if (count == 0) {
		now = time(NULL);
	}
	if (count == 0 || time(NULL) - now > 20) {// every 20 seconds: Update orders and positions, ProfitTakenAdjust, change the orders +/-1%, to see if it is ok.
		count++;
		now = time(NULL);
		wangUpdateOrdersAndPositions();
		for (int i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder * o = m_orderList[i];
			if (o->status == "Cancelled") {
				delete o;
				m_orderList.erase(m_orderList.begin() + i);
			}
		}
		if ((hour == 9 && minute >= 30) || (hour > 9 && hour < 16)) {
			//ProfitTakenAdjust();
			for (int index = 0; index < m_stocknum; index++) 
				m_stock[index].takenProfit = 10.0;
		}
		else if (hour == 9 && minute == 29) {
			for (int index = 0; index < m_stocknum; index++) 
				m_stock[index].takenProfit = 10.0;
		}
		else {
			for (int index = 0; index < m_stocknum; index++)
				m_stock[index].takenProfit = 10.0;
		}

		if (count % 6 == 0) {  //every 2 minutes
			wangPrintOrders();  
			double hedgedAmount = 0.0;
			for (int sinedex = 0; sinedex < m_stocknum; sinedex++) {
				Stock & s = m_stock[sinedex];
				hedgedAmount += (s.position[0] + s.position[1])*(s.askPrice + s.bidPrice)*0.5 * s.hedgeRatio;
			}
			printf("Total hedging amount that still needed is %g (wan)\n", round(hedgedAmount*0.0001, 0.01));
		}

		bool modified = false;
		for (int stockindex = 0; stockindex < m_stocknum; stockindex++) {
			Stock & s = m_stock[stockindex];
			if (s.askPrice < 1.0 || s.bidPrice < 1.0) continue;
			if (hour >= 16 || hour < 9 || (hour == 9 && minute < 30)) {  //out of regular trading hours
				if (s.symbol.size() == 5 && (s.symbol[4] == 'Y' || s.symbol[4] == 'F')) continue;    // do not trade OTC stocks out of regular trading hours.
			}
			//any repeating patterns
			if (repeatPatterns(s.actions) && difftime(time(NULL), s.actions[0].t) < 30) continue;

			for (unsigned int i = 0; i < m_orderList.size(); i++) {
				MyOrder * o = m_orderList[i];
				if (o->symbol != s.symbol) continue;
				if (o->status == "Submitted" && (o->remaining == o->totalQty || o->remaining * o->lmtPrice > 8000)) {
					if (o->action == "BUY" && o->lmtPrice >= s.bidPrice-0.001 && s.bidSize == floor(o->remaining / 100) ) {
						modifyLmtOrder(o->orderId, o->remaining, round(o->lmtPrice*0.99, 0.01));
						modified = true;
					}
					else if (o->action == "SELL" && o->lmtPrice <= s.askPrice+0.001 && s.askSize == floor(o->remaining / 100)) {
						modifyLmtOrder(o->orderId, o->remaining, round(o->lmtPrice*1.01, 0.01));
						modified = true;
					}
				}
			}
		}
		if (modified) {
			m_osSignal.waitForSignal();
			m_pReader->processMsgs();
			wangSleep(200);
		}
		//wangExportControlFile();
	}

	errno = 0;
	m_pReader->processMsgs();

	/* China Stock real time information retrieval. I comment it since I do not trade it now.	

	if (count % 300 == 1) { //adjust this number 300
		for (int stockindex = 0; stockindex < m_stocknum; stockindex++) {
			Stock & s = m_stock[stockindex];
			if (s.currency == "CNH") {
				getChinaStockPrice(s.symbol.c_str(), s.askPrice, s.bidPrice, s.askSize, s.bidSize, s.lastPrice, s.dayHigh, s.dayLow, s.closePrice, s.dayVolume);
				printf("%s Price is %g<%d>:%g<%d>\n", s.symbol.c_str(), s.askPrice, s.askSize, s.bidPrice, s.bidSize);
			}
		}
	}*/


	//check for fatTail 9:50 - 13:30
	if ((hour < 13 || (hour == 13 && minute < 30))  && (hour > 9 || (hour == 9 && minute > 50))) {  
		bool update = false;
		for (int stockindex = 0; stockindex < m_stocknum; stockindex++) {
			Stock& s = m_stock[stockindex];
			if (s.stdVariation == 0.0) continue;
			if (fabs(s.targetBottomPrice - s.bottomPrice) > 0.005 || fabs(s.targetTopPrice - s.topPrice) > 0.005 || fabs(s.targetMultiplier - s.multiplier) > 0.001 || fabs(s.targetOffset - s.offset) > 0.005) continue;
			s.oldBottomPrice = s.bottomPrice; s.oldTopPrice = s.topPrice; s.oldMultiplier = s.multiplier; s.oldOffset = s.offset;
			double buyPrice = s.previousClose * (1 + s.stdVariation);
			double sellPrice = s.previousClose * (1 - s.stdVariation);
			double delta = round(s.multiplier * 100 / log(s.topPrice / s.bottomPrice) * log(1 + s.stdVariation) * 5);
			if (s.fatTailBuyEnbled && !s.fatTailBuyDone) {
				if (s.dayHigh > buyPrice-0.001 && s.askPrice < buyPrice+0.011) {
					s.targetOffset = s.offset + delta;
					s.startTime = hour * 60 + minute + 1;
					s.endTime = hour * 60 + minute + 1 + round(delta * buyPrice / 25000);
					if (s.endTime == s.startTime) s.endTime = s.startTime + 1;
					s.fatTailBuyDone = true;
					update = true;
					continue;
				}
			}
			if (s.fatTailShortEnbled && !s.fatTailShortDone) {
				if (s.dayLow < sellPrice+0.001 && s.bidPrice > sellPrice-0.011) {
					s.targetOffset = s.offset - delta;
					s.startTime = hour * 60 + minute + 1;
					s.endTime = hour * 60 + minute + 1 + round(delta * sellPrice / 25000);
					if (s.endTime == s.startTime) s.endTime = s.startTime + 1;
					s.fatTailShortDone = true;
					update = true;
					continue;
				}
			}
		}
		if (update) wangExportControlFile(true);
	}
	//after 3:30, close fatTail positions
	if (hour == 15 && minute > 30 && minute < 59) {  
		bool update = false;
		for (int stockindex = 0; stockindex < m_stocknum; stockindex++) {
			Stock& s = m_stock[stockindex];
			if (s.stdVariation == 0.0) continue;
			if (fabs(s.targetBottomPrice - s.bottomPrice) > 0.005 || fabs(s.targetTopPrice - s.topPrice) > 0.005 || fabs(s.targetMultiplier - s.multiplier) > 0.001 || fabs(s.targetOffset - s.offset) > 0.005) continue;
			s.oldBottomPrice = s.bottomPrice; s.oldTopPrice = s.topPrice; s.oldMultiplier = s.multiplier; s.oldOffset = s.offset;
			double buyPrice = s.previousClose * (1 + s.stdVariation);
			double sellPrice = s.previousClose * (1 - s.stdVariation);
			double delta = round(s.multiplier * 100 / log(s.topPrice / s.bottomPrice) * log(1 + s.stdVariation) * 5);
			double shares = 0;
			if (s.fatTailBuyEnbled && s.fatTailBuyDone) shares += delta;
			if (s.fatTailShortEnbled && s.fatTailShortDone) shares -= delta;
			if (fabs(shares) > 0.01) {
				s.targetOffset = s.offset - shares;
				s.endTime = 16 * 60 - 1;
				s.startTime = s.endTime - round(fabs(shares) * (s.previousClose) / 25000);
				if (s.endTime == s.startTime) s.startTime = s.endTime - 1;
				s.fatTailBuyDone = false;
				s.fatTailShortDone = false;
				update = true;
				continue;
			}
		}
		if (update) wangExportControlFile(true);
	}


	for (int cp = 0; cp < 2; cp++)          //for comparison
	for (int stockindex = 0; stockindex < m_stocknum; stockindex++) {
		Stock & s = m_stock[stockindex];
		if (s.askPrice < 1.0 || s.bidPrice < 1.0) continue;
		//if (s.previousClose == 0.0 || s.stdVariation == 0.0 || s.lowBoundDiscount > 0.5) continue;
		if (hour >= 16 || hour < 9 || (hour == 9 && minute < 30)) {  //out of regular trading hours
			if (s.symbol.size() == 5 && (s.symbol[4] == 'Y' || s.symbol[4] == 'F')) continue;    // do not trade OTC stocks out of regular trading hours.
		}
		if (cp == 1 && !s.comparison) continue;

		//any repeating patterns
		if (repeatPatterns(s.actions) && difftime(time(NULL), s.actions[0].t) < 5) continue;

		double bottomPrice = s.bottomPrice;		if (bottomPrice == 0) bottomPrice = 0.01;
		double topPrice = s.topPrice;       
		double multiplier = s.multiplier;		
		double offset = s.offset;
		double profit = s.takenProfit;  //the profit for each round (one buy plus one sell)
		double position = s.position[0] + s.position[1];
		double tdqnt = 0;
		if (s.shared) {
			if (read_Td_position(s.symbol, tdqnt)) position += tdqnt;
			else continue;
		}

		//get current buy order price and sell order price. Adjust bidPrice and askPrice.
		double currentBuyPrice = -1.0, currentBuyQty = 0;
		double currentSellPrice = -1.0, currentSellQty = 0;
		for (unsigned int i = 0; i < m_orderList.size(); i++) {
			MyOrder * o = m_orderList[i];
			if (o->symbol != s.symbol) continue;
			if (o->status == "Cancelled") continue;
			if (o->status == "PendingCancel") continue;
			if (o->status == "Filled") continue;
			if (o->status == "Inactive") continue;
			if (s.comparison) {
				if (o->account == ACCOUNT1 && cp != 0) continue;
				if (o->account == ACCOUNT2 && cp != 1) continue;
			}

			if (o->action == "BUY") {
				if (o->remaining > 0 && currentBuyPrice < 0) {
					currentBuyPrice = o->lmtPrice;
					currentBuyQty = o->remaining;
				}
			}
			else if (o->action == "SELL") {
				if (o->remaining > 0 && currentSellPrice < 0) {
					currentSellPrice = o->lmtPrice;
					currentSellQty = o->remaining;
				}
			}
		}

		//do not count myself. Adjust bidPrice and askPrice.
		if (currentBuyPrice == s.bidPrice && !s.reverse) {
			if (s.bidSize * 100 <= currentBuyQty || s.bidSize * 100 - currentBuyQty < 100) {
				s.bidPrice -= 0.01;
			}
		}
		if (currentSellPrice == s.askPrice && !s.reverse) {
			if (s.askSize * 100 <= currentSellQty || s.askSize * 100 - currentSellQty < 100) {
				s.askPrice += 0.01;
			}
		}

		if (s.comparison) {
			multiplier /= 2;
			offset /= 2;
			if (cp == 0) {
				position = s.position[0];
				offset += (s.a1 - s.a2) / 2;
			}
			else {
				profit = 20.0;
				position = s.position[1];
				offset += (s.a2 - s.a1) / 2;
			}
		}

		double cPrice = (multiplier == 0)? (s.askPrice+s.bidPrice)/2 : priceFromDelta(position, bottomPrice, topPrice, multiplier, offset, s.periods);
		double spreadPercent = (multiplier == 0) ? 0.0001 : sqrt(profit / (multiplier * 100.0 * cPrice));  //the minimum spread to get the profit. 
		if (spreadPercent > 0.05) spreadPercent = 0.05;  //5% maximum
		double spread = spreadPercent * cPrice;
		//if (spread < 0.01) spread = 0.01;
		double minDelta = ceil(profit / spread); double rate = minDelta / (profit / spread);   //increase profit so that delta to be an integer. 
		spread *= rate;
		if (s.currency == "CNH") {
			spreadPercent = 0.01;
			spread = round(spreadPercent * (s.askPrice + s.bidPrice)*0.5 + 0.005, 0.01);
			minDelta = 30 / spread;
		}
		if (s.askPrice > 0 && s.bidPrice > 0 && (s.askPrice - s.bidPrice < (s.askPrice + s.bidPrice)*0.5 *0.15 )) {  //the condition to submit oders.
			double buyPrice, buyDelta, sellPrice, sellDelta;

			if (s.reverse) {
				buyDelta = 0; buyPrice = 0.01;
				sellDelta = 0; sellPrice = 10000000;

				double posPrice = priceFromDelta(position, bottomPrice, topPrice, multiplier, offset, s.periods);
				double bidDelta = deltaCalculation(s.bidPrice, bottomPrice, topPrice, multiplier, offset, s.periods);
				double askDelta = deltaCalculation(s.askPrice, bottomPrice, topPrice, multiplier, offset, s.periods);
				if (bidDelta > position+0.5 && (bidDelta - position) * (s.bidPrice - posPrice) > s.takenProfit && fabs(posPrice - s.bidPrice) > posPrice * 0.005) {
					buyPrice = s.bidPrice;
					buyDelta = bidDelta - position;
				}
				else if (askDelta < position - 0.5 && (position - askDelta) * (posPrice - s.askPrice) > s.takenProfit && fabs(posPrice - s.askPrice) > posPrice * 0.005) {
					sellPrice = s.askPrice;
					sellDelta = position - askDelta;
				}
				if (buyPrice > s.buyBelow) buyPrice = s.buyBelow;
				if (buyPrice < s.buyAbove) buyDelta = 0;
				if (sellPrice < s.sellAbove) sellPrice = s.sellAbove;
				if (sellPrice > s.sellBelow) sellDelta = 0;
				if (!s.canBuy || !m_canBuy) buyDelta = 0;
				if (!s.canSell || !m_canSell) sellDelta = 0;
				if (!s.canShort && position <= 0.01) sellDelta = 0;
			}
			else {

				buyPrice = floor((cPrice - spread) * 100) / 100;
				if (buyPrice > s.bidPrice + 0.01) buyPrice = s.bidPrice + 0.01; if (buyPrice > s.askPrice - 0.01) buyPrice = s.askPrice - 0.01;
				if (buyPrice > s.buyBelow) buyPrice = s.buyBelow;
				buyDelta = deltaCalculation(buyPrice, bottomPrice, topPrice, multiplier, offset, s.periods) - position; if (buyDelta < 1 && buyDelta > 0.2)  buyDelta = 1;
				sellPrice = ceil((cPrice + spread) * 100) / 100;
				if (sellPrice < s.askPrice - 0.01) sellPrice = s.askPrice - 0.01; if (sellPrice < s.bidPrice + 0.01) sellPrice = s.bidPrice + 0.01;
				if (sellPrice < s.sellAbove) sellPrice = s.sellAbove;
				sellDelta = position - deltaCalculation(sellPrice, bottomPrice, topPrice, multiplier, offset, s.periods); if (sellDelta < 1 && sellDelta > 0.2) sellDelta = 1;

				while (sellDelta < minDelta && sellPrice < topPrice) {
					sellPrice += 0.01;
					sellDelta = position - deltaCalculation(sellPrice, bottomPrice, topPrice, multiplier, offset, s.periods); if (sellDelta < 1 && sellDelta > 0.2) sellDelta = 1;
				}

				while (buyDelta < minDelta && buyPrice > bottomPrice) {
					buyPrice -= 0.01;
					buyDelta = deltaCalculation(buyPrice, bottomPrice, topPrice, multiplier, offset, s.periods) - position; if (buyDelta < 1 && buyDelta > 0.2)  buyDelta = 1;
				}

				if (s.stdVariation != 0.0) {
					double maxPosition = deltaCalculation(s.previousClose * (1 - s.stdVariation * (1 + s.lowBoundDiscount)), bottomPrice, topPrice, multiplier, offset, s.periods);
					double minPosition = deltaCalculation(s.previousClose * (1 + s.stdVariation), bottomPrice, topPrice, multiplier, offset, s.periods);
					if (buyDelta > maxPosition - position) buyDelta = round(maxPosition - position); if (buyDelta < 0.5)  buyDelta = 0;
					if (sellDelta > position - minPosition) sellDelta = round(position - minPosition); if (sellDelta < 0.5) sellDelta = 0;
				}

				if (buyPrice > s.buyBelow) buyPrice = s.buyBelow;
				if (buyPrice < s.buyAbove) buyDelta = 0;
				if (sellPrice < s.sellAbove) sellPrice = s.sellAbove;
				if (sellPrice > s.sellBelow) sellDelta = 0;
				if (!s.canBuy || !m_canBuy) buyDelta = 0;
				if (!s.canSell || !m_canSell) sellDelta = 0;
				if (!s.canShort && position <= 0.01) sellDelta = 0;

				if (multiplier == 0) {
					if (s.position[0] + s.position[1] - offset > 0.01) {
						sellDelta = s.position[0] + s.position[1] - offset;
						sellPrice = s.askPrice;
						buyDelta = 0; buyPrice = 0.01;
					}
					else if (s.position[0] + s.position[1] - offset < -0.01) {
						buyDelta = -(s.position[0] + s.position[1] - offset);
						buyPrice = s.bidPrice;
						sellDelta = 0; sellPrice = 10000000;
					}
					else {
						buyDelta = sellDelta = 0;
						buyPrice = 0.01; sellPrice = 10000000;
					}
				}
			}


//			if ((hour >= 15 && hour <= 17) || (hour == 14 && minute >= 1)) {  //from 9:02 to 18:00pm
			if (!s.alpacaTrading || !m_alpacaAllTrading) {
				//trading
			}
			else if (hour == 9 && minute * 60 + second >= 29 * 60 + 30 && minute <= 33) {  //from 9:29:30 to 9:34
				//trading...
			}
			/*
			else if (hour == 9 && minute < 1) {  //from 9:00 to 9:01
				//trading...
			}
			else if (hour < 9 || hour >= 18) {
				//trading...
			}*/
			else {
				if (s.shared) {
					bool tdBuy = true;
					if ((tdqnt + 1.01) * s.askPrice > s.maxAlpacaMarketValue * (106 - minute / 5) / 100 || (position < -0.1 && tdqnt >-position + 0.1) || (position > 0.1 && tdqnt > position - 0.1) || multiplier == 0) {
						tdBuy = false;
					}
					if (tdBuy) {  //do not buy ; *3/3 is to stabalize it.
						buyDelta = 0;
					}
					if (tdqnt > 0.1) {  //do not sell if tdnq will sell
						sellDelta = 0;
					}
				}
			}

			if (s.reverse && (hour < 9 || hour >= 16 || (hour == 9 && minute < 45) || (hour == 3 && minute == 59 && second > 30))) {
				buyDelta = 0;
				sellDelta = 0;
			}

			if (buyDelta * buyPrice > 25000) buyDelta = round(25000 / buyPrice);  if (buyDelta < 1 && buyDelta > 0.2)  buyDelta = 1;
			if (sellDelta * sellPrice > 25000) sellDelta = round(25000 / sellPrice); if (sellDelta < 1 && sellDelta > 0.2) sellDelta = 1;
			if (!s.comparison && s.position[1] < s.longTermHolding-0.01) {
				if (buyDelta > s.longTermHolding - s.position[1]) buyDelta = round(s.longTermHolding - s.position[1]);
			}
			if (!s.comparison && s.position[1] > s.longTermHolding+0.01) {
				if (sellDelta > s.position[1] - s.longTermHolding) sellDelta = round(s.position[1] - s.longTermHolding);
			}
			if ((!s.comparison && s.position[1] <= s.longTermHolding+0.01 && s.position[0] > 0 && !s.canShort)) {
				if (sellDelta > s.position[0]) sellDelta = round(s.position[0]);
			}


			bool toBuy = true;
			bool toSell = true;
			bool toModifyBuy = false; int modifyBuyOrderID = 0;
			bool toModifySell = false; int modifySellOrderID = 0;
			for (unsigned int i = 0; i < m_orderList.size(); i++) {
					MyOrder * o = m_orderList[i];
					if (o->symbol != s.symbol) continue;
					if (o->status == "Cancelled") continue;
					if (o->status == "ApiCancelled") {
						printf("ApiCancelled\n");
						//continue;
					}
					if (o->status == "PendingCancel") continue;
					if (o->status == "Filled") continue;
					if (o->status == "Inactive") continue;
					if (o->status != "Submitted" && o->status != "PreSubmitted" && o->status != "PendingSubmit") printf("%s order %s\n", o->symbol.c_str(), o->status.c_str());
					if (s.comparison) {
						if (o->account == ACCOUNT1 && cp != 0) continue;
						if (o->account == ACCOUNT2 && cp != 1) continue;
					}

					if (o->action == "BUY") {
						if (toModifyBuy || !m_canBuy || !s.canBuy || buyPrice > s.buyBelow + 0.001 || buyPrice < s.buyAbove - 0.001 || buyDelta < 0.1) {
							m_pClient->cancelOrder(o->orderId);
							soundPlay.push_back(3);
							s.actions.push_front(Action(3, o->action, o->lmtPrice, o->totalQty));
							//toBuy = false; //not buy this time, wait for more guaranttee.
							m_osSignal.waitForSignal();
							m_pReader->processMsgs();
							continue;
						}
						double deltaDiff = fabs(buyDelta - o->remaining);
						if (toBuy && deltaDiff < 1 && o->remaining > 0 && round(o->lmtPrice*100 - buyPrice*100) == 0)   //same order, no new buy order
						{
							toBuy = false;
						}
						else {
							if (toModifyBuy == false && (o->status == "Submitted" || o->status == "PreSubmitted" || o->status == "PendingSubmit")) {
								modifyBuyOrderID = o->orderId;
								toModifyBuy = true;
							}
						}
					}
					else if (o->action == "SELL") {
						if (toModifySell || !m_canSell || !s.canSell || sellPrice < s.sellAbove - 0.001 || sellPrice > s.sellBelow + 0.001 || sellDelta < 0.1
							|| (!s.comparison && s.position[1] <= s.longTermHolding+0.01 && s.position[0] <= 0.01 && !s.canShort && position < 0.1)) {
							m_pClient->cancelOrder(o->orderId);
							soundPlay.push_back(3);
							s.actions.push_front(Action(3, o->action, o->lmtPrice, o->totalQty));
							//toSell = false;   //not sell this time, wait for more guaranttee.
							m_osSignal.waitForSignal();
							m_pReader->processMsgs();
							continue;
						}
						double deltaDiff = fabs(sellDelta - o->remaining);
						if (toSell && deltaDiff < 1 && o->remaining > 0 && round(o->lmtPrice*100) == round(sellPrice*100)) //same order, no new buy order
						{
							toSell = false;
						}
						else {
							if (toModifySell == false && (o->status == "Submitted" || o->status == "PreSubmitted" || o->status == "PendingSubmit")) {
								modifySellOrderID = o->orderId;
								toModifySell = true;
							}
						}
					}
			}
			if (toBuy || toSell) {
				Contract contract;
				contract.symbol = s.symbol;
				contract.secType = "STK";
				contract.currency = s.currency;
				//In the API side, NASDAQ is always defined as ISLAND
				if (contract.currency == "USD") {
					//contract.primaryExchange = "ARCA";
					contract.exchange = "ISLAND";
				}
				else contract.exchange = "SEHKSZSE";
				if (toBuy && buyDelta > 0.9 && buyPrice < s.buyBelow+0.001 && buyPrice > s.buyAbove - 0.001 && s.canBuy && m_canBuy) {
					if (toModifyBuy) {
						modifyLmtOrder(modifyBuyOrderID, buyDelta, buyPrice);
						s.actions.push_front(Action(2, "BUY", buyPrice, buyDelta));
					}
					else {
						MyOrder * p_neworder = new MyOrder;
						m_orderList.push_back(p_neworder);
						if (!s.comparison && s.position[1] < s.longTermHolding) {
							p_neworder->account = ACCOUNT2;//ACCOUNT2
						}
						else
							p_neworder->account = ACCOUNT1;

						//p_neworder->account = ACCOUNT2;

						p_neworder->symbol = s.symbol;
						p_neworder->orderId = m_orderId;
						p_neworder->action = "BUY";
						p_neworder->status = "PreSubmitted";
						p_neworder->orderType = "LMT";
						p_neworder->lmtPrice = buyPrice;
						p_neworder->totalQty = buyDelta;
						p_neworder->remaining = buyDelta;
						p_neworder->filledQty = 0;
						wangShareOrderInfo(p_neworder);

						if (s.comparison) {
							if (cp == 0) p_neworder->account = ACCOUNT1;
							if (cp == 1) p_neworder->account = ACCOUNT2;;
						}
						printf("Place Order Buy %s, %g, %g\n", s.symbol.c_str(), buyPrice, buyDelta);
						m_pClient->placeOrder(m_orderId++, contract, OrderSamples::LimitOrder("buy", buyDelta, buyPrice, p_neworder->account));
						s.actions.push_front(Action(1, "BUY", buyPrice, buyDelta));
						p_neworder->tryPrice = buyPrice;
						p_neworder->trySize = buyDelta;
						p_neworder->tryTime = time(NULL);
					}
					m_osSignal.waitForSignal();
					m_pReader->processMsgs();
				}

				if ((!s.comparison && s.position[1] <= s.longTermHolding + 0.01 && s.position[0] <= 0.01 && !s.canShort && position < 0.1)) {
					toSell = false;
				}
				if (toSell && sellDelta > 0.9 && sellPrice > s.sellAbove - 0.001 && sellPrice < s.sellBelow + 0.001 && s.canSell && m_canSell) {
					if (toModifySell) {
						modifyLmtOrder(modifySellOrderID, sellDelta, sellPrice);
						s.actions.push_front(Action(2, "SELL", sellPrice, sellDelta));
					}
					else {
						MyOrder * p_neworder = new MyOrder;
						m_orderList.push_back(p_neworder);
						if (!s.comparison && s.position[1] > s.longTermHolding) {
							p_neworder->account = ACCOUNT2;//ACCOUNT2
						}
						else
							p_neworder->account = ACCOUNT1;

						//p_neworder->account = ACCOUNT1;

						p_neworder->symbol = s.symbol;
						p_neworder->orderId = m_orderId;
						p_neworder->action = "SELL";
						p_neworder->status = "PreSubmitted";
						p_neworder->orderType = "LMT";
						p_neworder->lmtPrice = sellPrice;
						p_neworder->totalQty = sellDelta;
						p_neworder->remaining = sellDelta;
						//p_neworder->account = ACCOUNT1;
						p_neworder->filledQty = 0;
						wangShareOrderInfo(p_neworder);

						if (s.comparison) {
							if (cp == 0) p_neworder->account = ACCOUNT1;
							if (cp == 1) p_neworder->account = ACCOUNT2;;
						}
						printf("Place Order Sell %s, %g, %g\n", s.symbol.c_str(), sellPrice, sellDelta);
						m_pClient->placeOrder(m_orderId++, contract, OrderSamples::LimitOrder("sell", sellDelta, sellPrice, p_neworder->account));
						s.actions.push_front(Action(1, "SELL", sellPrice, sellDelta));
						p_neworder->tryPrice = buyPrice;
						p_neworder->trySize = buyDelta;
						p_neworder->tryTime = time(NULL);
					}
					m_osSignal.waitForSignal();
					m_pReader->processMsgs();
				}
			}
		}
	}

	errno = 0;
	wangSleep(200);
}


//////////////////////////////////////////////////////////////////
// methods
//! [connectack]
void TestCppClient::connectAck() {
	if (!m_extraAuth && m_pClient->asyncEConnect())
        m_pClient->startApi();
}
//! [connectack]

void TestCppClient::reqCurrentTime()
{
	printf( "Requesting Current Time\n");

	// set ping deadline to "now + n seconds"
	m_sleepDeadline = time( NULL) + PING_DEADLINE;

	m_state = ST_PING_ACK;

	m_pClient->reqCurrentTime();
}

void TestCppClient::pnlOperation()
{
	//! [reqpnl]
    m_pClient->reqPnL(7001, "DUD00029", "");
	//! [reqpnl]
	
    std::this_thread::sleep_for(std::chrono::seconds(2));

	//! [cancelpnl]
    m_pClient->cancelPnL(7001);
	//! [cancelpnl] 
	
    m_state = ST_PNL_ACK;
}

void TestCppClient::pnlSingleOperation()
{
	//! [reqpnlsingle]
    m_pClient->reqPnLSingle(7002, "DUD00029", "", 268084);
	//! [reqpnlsingle]
	
    std::this_thread::sleep_for(std::chrono::seconds(2));

	//! [cancelpnlsingle]
    m_pClient->cancelPnLSingle(7002);
	//! [cancelpnlsingle]
	
    m_state = ST_PNLSINGLE_ACK;
}

void TestCppClient::tickDataOperation()
{
	/*** Requesting real time market data ***/
    std::this_thread::sleep_for(std::chrono::seconds(1));
    //! [reqmktdata]
	m_pClient->reqMktData(1001, ContractSamples::StockComboContract(), "", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1002, ContractSamples::OptionWithLocalSymbol(), "", false, false, TagValueListSPtr());
	//! [reqmktdata]
	//! [reqmktdata_snapshot]
	m_pClient->reqMktData(1003, ContractSamples::FutureComboContract(), "", true, false, TagValueListSPtr());
	//! [reqmktdata_snapshot]

	/*
	//! [regulatorysnapshot]
	// Each regulatory snapshot incurs a fee of 0.01 USD
	m_pClient->reqMktData(1013, ContractSamples::USStock(), "", false, true, TagValueListSPtr());
	//! [regulatorysnapshot]
	*/
	
	//! [reqmktdata_genticks]
	//Requesting RTVolume (Time & Sales), shortable and Fundamental Ratios generic ticks
	m_pClient->reqMktData(1004, ContractSamples::USStockAtSmart(), "233,236,258", false, false, TagValueListSPtr());
	//! [reqmktdata_genticks]

	//! [reqmktdata_contractnews]
	// Without the API news subscription this will generate an "invalid tick type" error
	m_pClient->reqMktData(1005, ContractSamples::USStock(), "mdoff,292:BZ", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1006, ContractSamples::USStock(), "mdoff,292:BT", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1007, ContractSamples::USStock(), "mdoff,292:FLY", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1008, ContractSamples::USStock(), "mdoff,292:MT", false, false, TagValueListSPtr());
	//! [reqmktdata_contractnews]
	//! [reqmktdata_broadtapenews]
	m_pClient->reqMktData(1009, ContractSamples::BTbroadtapeNewsFeed(), "mdoff,292", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1010, ContractSamples::BZbroadtapeNewsFeed(), "mdoff,292", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1011, ContractSamples::FLYbroadtapeNewsFeed(), "mdoff,292", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1012, ContractSamples::MTbroadtapeNewsFeed(), "mdoff,292", false, false, TagValueListSPtr());
	//! [reqmktdata_broadtapenews]

	//! [reqoptiondatagenticks]
	//Requesting data for an option contract will return the greek values
	m_pClient->reqMktData(1013, ContractSamples::USOptionContract(), "", false, false, TagValueListSPtr());
	//! [reqoptiondatagenticks]
	
	//! [reqfuturesopeninterest]
	//Requesting data for a futures contract will return the futures open interest
	m_pClient->reqMktData(1014, ContractSamples::SimpleFuture(), "mdoff,588", false, false, TagValueListSPtr());
	//! [reqfuturesopeninterest]

	//! [reqpreopenbidask]
	//Requesting data for a futures contract will return the pre-open bid/ask flag
	m_pClient->reqMktData(1015, ContractSamples::SimpleFuture(), "", false, false, TagValueListSPtr());
	//! [reqpreopenbidask]

	//! [reqavgoptvolume]
	//Requesting data for a stock will return the average option volume
	m_pClient->reqMktData(1016, ContractSamples::USStockAtSmart(), "mdoff,105", false, false, TagValueListSPtr());
	//! [reqavgoptvolume]

	std::this_thread::sleep_for(std::chrono::seconds(1));
	/*** Canceling the market data subscription ***/
	//! [cancelmktdata]
	m_pClient->cancelMktData(1001);
	m_pClient->cancelMktData(1002);
	m_pClient->cancelMktData(1003);
	m_pClient->cancelMktData(1014);
	m_pClient->cancelMktData(1015);
	m_pClient->cancelMktData(1016);
	//! [cancelmktdata]

	m_state = ST_TICKDATAOPERATION_ACK;
}

void TestCppClient::tickOptionComputationOperation()
{
	/*** Requesting real time market data ***/
	std::this_thread::sleep_for(std::chrono::seconds(1));
	//! [reqmktdata]
	m_pClient->reqMktData(2001, ContractSamples::FuturesOnOptions(), "", false, false, TagValueListSPtr());
	//! [reqmktdata]

	std::this_thread::sleep_for(std::chrono::seconds(10));
	/*** Canceling the market data subscription ***/
	//! [cancelmktdata]
	m_pClient->cancelMktData(2001);
	//! [cancelmktdata]

	m_state = ST_TICKOPTIONCOMPUTATIONOPERATION_ACK;
}

void TestCppClient::delayedTickDataOperation()
{
	/*** Requesting delayed market data ***/

	//! [reqmktdata_delayedmd]
	m_pClient->reqMarketDataType(4); // send delayed-frozen (4) market data type
	m_pClient->reqMktData(1013, ContractSamples::HKStk(), "", false, false, TagValueListSPtr());
	m_pClient->reqMktData(1014, ContractSamples::USOptionContract(), "", false, false, TagValueListSPtr());
	//! [reqmktdata_delayedmd]

	std::this_thread::sleep_for(std::chrono::seconds(10));

	/*** Canceling the delayed market data subscription ***/
	//! [cancelmktdata_delayedmd]
	m_pClient->cancelMktData(1013);
	m_pClient->cancelMktData(1014);
	//! [cancelmktdata_delayedmd]

	m_state = ST_DELAYEDTICKDATAOPERATION_ACK;
}

void TestCppClient::marketDepthOperations()
{
	/*** Requesting the Deep Book ***/
	//! [reqmarketdepth]
	m_pClient->reqMktDepth(2001, ContractSamples::EurGbpFx(), 5, false, TagValueListSPtr());
	//! [reqmarketdepth]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	/*** Canceling the Deep Book request ***/
	//! [cancelmktdepth]
	m_pClient->cancelMktDepth(2001, false);
	//! [cancelmktdepth]

	/*** Requesting the Deep Book ***/
	//! [reqmarketdepth]
	m_pClient->reqMktDepth(2002, ContractSamples::EuropeanStock(), 5, true, TagValueListSPtr());
	//! [reqmarketdepth]
	std::this_thread::sleep_for(std::chrono::seconds(5));
	/*** Canceling the Deep Book request ***/
	//! [cancelmktdepth]
	m_pClient->cancelMktDepth(2002, true);
	//! [cancelmktdepth]

	m_state = ST_MARKETDEPTHOPERATION_ACK;
}

void TestCppClient::realTimeBars()
{
	/*** Requesting real time bars ***/
	//! [reqrealtimebars]
	m_pClient->reqRealTimeBars(3001, ContractSamples::EurGbpFx(), 5, "MIDPOINT", true, TagValueListSPtr());
	//! [reqrealtimebars]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	/*** Canceling real time bars ***/
    //! [cancelrealtimebars]
	m_pClient->cancelRealTimeBars(3001);
    //! [cancelrealtimebars]

	m_state = ST_REALTIMEBARS_ACK;
}

void TestCppClient::marketDataType()
{
	//! [reqmarketdatatype]
	/*** By default only real-time (1) market data is enabled
		 Sending frozen (2) enables frozen market data
		 Sending delayed (3) enables delayed market data and disables delayed-frozen market data
		 Sending delayed-frozen (4) enables delayed and delayed-frozen market data
		 Sending real-time (1) disables frozen, delayed and delayed-frozen market data ***/
	m_pClient->reqMarketDataType(2);
	//! [reqmarketdatatype]

	m_state = ST_MARKETDATATYPE_ACK;
}

void TestCppClient::historicalDataRequests()
{
	/*** Requesting historical data ***/
	//! [reqhistoricaldata]
	std::time_t rawtime;
    std::tm* timeinfo;
    char queryTime [80];

	std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);
	std::strftime(queryTime, 80, "%Y%m%d %H:%M:%S", timeinfo);

	m_pClient->reqHistoricalData(4001, ContractSamples::EurGbpFx(), queryTime, "1 M", "1 day", "MIDPOINT", 1, 1, false, TagValueListSPtr());
	m_pClient->reqHistoricalData(4002, ContractSamples::EuropeanStock(), queryTime, "10 D", "1 min", "TRADES", 1, 1, false, TagValueListSPtr());
	//! [reqhistoricaldata]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	/*** Canceling historical data requests ***/
	m_pClient->cancelHistoricalData(4001);
	m_pClient->cancelHistoricalData(4002);

	m_state = ST_HISTORICALDATAREQUESTS_ACK;
}

void TestCppClient::optionsOperations()
{
	//! [reqsecdefoptparams]
	m_pClient->reqSecDefOptParams(0, "IBM", "", "STK", 8314);
	//! [reqsecdefoptparams]

	//! [calculateimpliedvolatility]
	m_pClient->calculateImpliedVolatility(5001, ContractSamples::NormalOption(), 5, 85, TagValueListSPtr());
	//! [calculateimpliedvolatility]

	//** Canceling implied volatility ***
	m_pClient->cancelCalculateImpliedVolatility(5001);

	//! [calculateoptionprice]
	m_pClient->calculateOptionPrice(5002, ContractSamples::NormalOption(), 0.22, 85, TagValueListSPtr());
	//! [calculateoptionprice]

	//** Canceling option's price calculation ***
	m_pClient->cancelCalculateOptionPrice(5002);

	//! [exercise_options]
	//** Exercising options ***
	m_pClient->exerciseOptions(5003, ContractSamples::OptionWithTradingClass(), 1, 1, "", 1);
	//! [exercise_options]

	m_state = ST_OPTIONSOPERATIONS_ACK;
}

void TestCppClient::contractOperations()
{
	m_pClient->reqContractDetails(209, ContractSamples::EurGbpFx());
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [reqcontractdetails]
	m_pClient->reqContractDetails(210, ContractSamples::OptionForQuery());
	m_pClient->reqContractDetails(212, ContractSamples::IBMBond());
	m_pClient->reqContractDetails(213, ContractSamples::IBKRStk());
	m_pClient->reqContractDetails(214, ContractSamples::Bond());
	m_pClient->reqContractDetails(215, ContractSamples::FuturesOnOptions());
	m_pClient->reqContractDetails(216, ContractSamples::SimpleFuture());
	//! [reqcontractdetails]

	//! [reqcontractdetailsnews]
	m_pClient->reqContractDetails(211, ContractSamples::NewsFeedForQuery());
	//! [reqcontractdetailsnews]

	m_state = ST_CONTRACTOPERATION_ACK;
}

void TestCppClient::marketScanners()
{
	/*** Requesting all available parameters which can be used to build a scanner request ***/
	//! [reqscannerparameters]
	m_pClient->reqScannerParameters();
	//! [reqscannerparameters]
	std::this_thread::sleep_for(std::chrono::seconds(2));

	/*** Triggering a scanner subscription ***/
	//! [reqscannersubscription]
	m_pClient->reqScannerSubscription(7001, ScannerSubscriptionSamples::HotUSStkByVolume(), TagValueListSPtr(), TagValueListSPtr());
	
	TagValueSPtr t1(new TagValue("usdMarketCapAbove", "10000"));
	TagValueSPtr t2(new TagValue("optVolumeAbove", "1000"));
	TagValueSPtr t3(new TagValue("usdMarketCapAbove", "100000000"));

	TagValueListSPtr TagValues(new TagValueList());
	TagValues->push_back(t1);
	TagValues->push_back(t2);
	TagValues->push_back(t3);

	m_pClient->reqScannerSubscription(7002, ScannerSubscriptionSamples::HotUSStkByVolume(), TagValueListSPtr(), TagValues); // requires TWS v973+
	
	//! [reqscannersubscription]

	//! [reqcomplexscanner]

	TagValueSPtr t(new TagValue("underConID", "265598"));
	TagValueListSPtr AAPLConIDTag(new TagValueList());
	AAPLConIDTag->push_back(t);
	m_pClient->reqScannerSubscription(7003, ScannerSubscriptionSamples::ComplexOrdersAndTrades(), TagValueListSPtr(), AAPLConIDTag); // requires TWS v975+

	//! [reqcomplexscanner]

	std::this_thread::sleep_for(std::chrono::seconds(2));
	/*** Canceling the scanner subscription ***/
	//! [cancelscannersubscription]
	m_pClient->cancelScannerSubscription(7001);
	m_pClient->cancelScannerSubscription(7002);
	//! [cancelscannersubscription]

	m_state = ST_MARKETSCANNERS_ACK;
}

void TestCppClient::fundamentals()
{
	/*** Requesting Fundamentals ***/
	//! [reqfundamentaldata]
	m_pClient->reqFundamentalData(8001, ContractSamples::USStock(), "ReportsFinSummary", TagValueListSPtr());
	//! [reqfundamentaldata]
	std::this_thread::sleep_for(std::chrono::seconds(2));

	/*** Canceling fundamentals request ***/
	//! [cancelfundamentaldata]
	m_pClient->cancelFundamentalData(8001);
	//! [cancelfundamentaldata]

	m_state = ST_FUNDAMENTALS_ACK;
}

void TestCppClient::bulletins()
{
	/*** Requesting Interactive Broker's news bulletins */
	//! [reqnewsbulletins]
	m_pClient->reqNewsBulletins(true);
	//! [reqnewsbulletins]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	/*** Canceling IB's news bulletins ***/
	//! [cancelnewsbulletins]
	m_pClient->cancelNewsBulletins();
	//! [cancelnewsbulletins]

	m_state = ST_BULLETINS_ACK;
}

void TestCppClient::accountOperations()
{
	/*** Requesting managed accounts***/
	//! [reqmanagedaccts]
	m_pClient->reqManagedAccts();
	//! [reqmanagedaccts]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	/*** Requesting accounts' summary ***/
	//! [reqaaccountsummary]
	m_pClient->reqAccountSummary(9001, "All", AccountSummaryTags::getAllTags());
	//! [reqaaccountsummary]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [reqaaccountsummaryledger]
	m_pClient->reqAccountSummary(9002, "All", "$LEDGER");
	//! [reqaaccountsummaryledger]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [reqaaccountsummaryledgercurrency]
	m_pClient->reqAccountSummary(9003, "All", "$LEDGER:EUR");
	//! [reqaaccountsummaryledgercurrency]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [reqaaccountsummaryledgerall]
	m_pClient->reqAccountSummary(9004, "All", "$LEDGER:ALL");
	//! [reqaaccountsummaryledgerall]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [cancelaaccountsummary]
	m_pClient->cancelAccountSummary(9001);
	m_pClient->cancelAccountSummary(9002);
	m_pClient->cancelAccountSummary(9003);
	m_pClient->cancelAccountSummary(9004);
	//! [cancelaaccountsummary]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	/*** Subscribing to an account's information. Only one at a time! ***/
	//! [reqaaccountupdates]
	m_pClient->reqAccountUpdates(true, "U150462");
	//! [reqaaccountupdates]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [cancelaaccountupdates]
	m_pClient->reqAccountUpdates(false, "U150462");
	//! [cancelaaccountupdates]
	std::this_thread::sleep_for(std::chrono::seconds(2));

	//! [reqaaccountupdatesmulti]
	m_pClient->reqAccountUpdatesMulti(9002, "U150462", "EUstocks", true);
	//! [reqaaccountupdatesmulti]
	std::this_thread::sleep_for(std::chrono::seconds(2));

	/*** Requesting all accounts' positions. ***/
	//! [reqpositions]
	m_pClient->reqPositions();
	//! [reqpositions]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [cancelpositions]
	m_pClient->cancelPositions();
	//! [cancelpositions]

	//! [reqpositionsmulti]
	m_pClient->reqPositionsMulti(9003, "U150462", "EUstocks");
	//! [reqpositionsmulti]

	m_state = ST_ACCOUNTOPERATIONS_ACK;
}

void TestCppClient::orderOperations()
{
	/*** Requesting the next valid id ***/
	//! [reqids]
	//The parameter is always ignored.
	m_pClient->reqIds(-1);
	//! [reqids]
	//! [reqallopenorders]
	m_pClient->reqAllOpenOrders();
	//! [reqallopenorders]
	//! [reqautoopenorders]
	m_pClient->reqAutoOpenOrders(true);
	//! [reqautoopenorders]
	//! [reqopenorders]
	m_pClient->reqOpenOrders();
	//! [reqopenorders]
	m_pClient->reqPositions();

	/*** Placing/modifying an order - remember to ALWAYS increment the nextValidId after placing an order so it can be used for the next one! ***/
    //! [order_submission]
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::LimitOrder("SELL", 1, 50));
    //! [order_submission]

	//m_pClient->placeOrder(m_orderId++, ContractSamples::OptionAtBox(), OrderSamples::Block("BUY", 50, 20));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::OptionAtBox(), OrderSamples::BoxTop("SELL", 10));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::FutureComboContract(), OrderSamples::ComboLimitOrder("SELL", 1, 1, false));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::StockComboContract(), OrderSamples::ComboMarketOrder("BUY", 1, false));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::OptionComboContract(), OrderSamples::ComboMarketOrder("BUY", 1, true));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::StockComboContract(), OrderSamples::LimitOrderForComboWithLegPrices("BUY", 1, std::vector<double>(10, 5), true));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::Discretionary("SELL", 1, 45, 0.5));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::OptionAtBox(), OrderSamples::LimitIfTouched("BUY", 1, 30, 34));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::LimitOnClose("SELL", 1, 34));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::LimitOnOpen("BUY", 1, 35));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::MarketIfTouched("BUY", 1, 35));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::MarketOnClose("SELL", 1));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::MarketOnOpen("BUY", 1));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::MarketOrder("SELL", 1));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::MarketToLimit("BUY", 1));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::OptionAtIse(), OrderSamples::MidpointMatch("BUY", 1));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::Stop("SELL", 1, 34.4));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::StopLimit("BUY", 1, 35, 33));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::StopWithProtection("SELL", 1, 45));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::SweepToFill("BUY", 1, 35));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::TrailingStop("SELL", 1, 0.5, 30));
	//m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), OrderSamples::TrailingStopLimit("BUY", 1, 2, 5, 50));
	
	//! [place_midprice]
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), OrderSamples::Midprice("BUY", 1, 150));
	//! [place_midprice]
	
	//! [place order with cashQty]
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), OrderSamples::LimitOrderWithCashQty("BUY", 1, 30, 5000));
	//! [place order with cashQty]

	std::this_thread::sleep_for(std::chrono::seconds(1));

	/*** Cancel one order ***/
	//! [cancelorder]
	m_pClient->cancelOrder(m_orderId-1);
	//! [cancelorder]
	
	/*** Cancel all orders for all accounts ***/
	//! [reqglobalcancel]
	m_pClient->reqGlobalCancel();
	//! [reqglobalcancel]

	/*** Request the day's executions ***/
	//! [reqexecutions]
	m_pClient->reqExecutions(10001, ExecutionFilter());
	//! [reqexecutions]

	//! [reqcompletedorders]
	m_pClient->reqCompletedOrders(false);
	//! [reqcompletedorders]

	m_state = ST_ORDEROPERATIONS_ACK;
}

void TestCppClient::ocaSamples()
{
	//OCA ORDER
	//! [ocasubmit]
	std::vector<Order> ocaOrders;
	ocaOrders.push_back(OrderSamples::LimitOrder("BUY", 1, 10));
	ocaOrders.push_back(OrderSamples::LimitOrder("BUY", 1, 11));
	ocaOrders.push_back(OrderSamples::LimitOrder("BUY", 1, 12));
	for(unsigned int i = 0; i < ocaOrders.size(); i++){
		OrderSamples::OneCancelsAll("TestOca", ocaOrders[i], 2);
		m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), ocaOrders[i]);
	}
	//! [ocasubmit]

	m_state = ST_OCASAMPLES_ACK;
}

void TestCppClient::conditionSamples()
{
	//! [order_conditioning_activate]
	Order lmt = OrderSamples::LimitOrder("BUY", 100, 10);
	//Order will become active if conditioning criteria is met
	PriceCondition* priceCondition = dynamic_cast<PriceCondition *>(OrderSamples::Price_Condition(208813720, "SMART", 600, false, false));
	ExecutionCondition* execCondition = dynamic_cast<ExecutionCondition *>(OrderSamples::Execution_Condition("EUR.USD", "CASH", "IDEALPRO", true));
	MarginCondition* marginCondition = dynamic_cast<MarginCondition *>(OrderSamples::Margin_Condition(30, true, false));
	PercentChangeCondition* pctChangeCondition = dynamic_cast<PercentChangeCondition *>(OrderSamples::Percent_Change_Condition(15.0, 208813720, "SMART", true, true));
	TimeCondition* timeCondition = dynamic_cast<TimeCondition *>(OrderSamples::Time_Condition("20160118 23:59:59", true, false));
	VolumeCondition* volumeCondition = dynamic_cast<VolumeCondition *>(OrderSamples::Volume_Condition(208813720, "SMART", false, 100, true));

	lmt.conditions.push_back(std::shared_ptr<PriceCondition>(priceCondition));
	lmt.conditions.push_back(std::shared_ptr<ExecutionCondition>(execCondition));
	lmt.conditions.push_back(std::shared_ptr<MarginCondition>(marginCondition));
	lmt.conditions.push_back(std::shared_ptr<PercentChangeCondition>(pctChangeCondition));
	lmt.conditions.push_back(std::shared_ptr<TimeCondition>(timeCondition));
	lmt.conditions.push_back(std::shared_ptr<VolumeCondition>(volumeCondition));
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(),lmt);
	//! [order_conditioning_activate]

	//Conditions can make the order active or cancel it. Only LMT orders can be conditionally canceled.
	//! [order_conditioning_cancel]
	Order lmt2 = OrderSamples::LimitOrder("BUY", 100, 20);
	//The active order will be cancelled if conditioning criteria is met
	lmt2.conditionsCancelOrder = true;
	PriceCondition* priceCondition2 = dynamic_cast<PriceCondition *>(OrderSamples::Price_Condition(208813720, "SMART", 600, false, false));
	lmt2.conditions.push_back(std::shared_ptr<PriceCondition>(priceCondition2));
	m_pClient->placeOrder(m_orderId++, ContractSamples::EuropeanStock(), lmt2);
	//! [order_conditioning_cancel]

	m_state = ST_CONDITIONSAMPLES_ACK;
}

void TestCppClient::bracketSample(){
	Order parent;
	Order takeProfit;
	Order stopLoss;
	//! [bracketsubmit]
	OrderSamples::BracketOrder(m_orderId++, parent, takeProfit, stopLoss, "BUY", 100, 30, 40, 20);
	m_pClient->placeOrder(parent.orderId, ContractSamples::EuropeanStock(), parent);
	m_pClient->placeOrder(takeProfit.orderId, ContractSamples::EuropeanStock(), takeProfit);
	m_pClient->placeOrder(stopLoss.orderId, ContractSamples::EuropeanStock(), stopLoss);
	//! [bracketsubmit]

	m_state = ST_BRACKETSAMPLES_ACK;
}

void TestCppClient::hedgeSample(){
	//F Hedge order
	//! [hedgesubmit]
	//Parent order on a contract which currency differs from your base currency
	Order parent = OrderSamples::LimitOrder("BUY", 100, 10);
	parent.orderId = m_orderId++;
	parent.transmit = false;
	//Hedge on the currency conversion
	Order hedge = OrderSamples::MarketFHedge(parent.orderId, "BUY");
	//Place the parent first...
	m_pClient->placeOrder(parent.orderId, ContractSamples::EuropeanStock(), parent);
	//Then the hedge order
	m_pClient->placeOrder(m_orderId++, ContractSamples::EurGbpFx(), hedge);
	//! [hedgesubmit]

	m_state = ST_HEDGESAMPLES_ACK;
}

void TestCppClient::testAlgoSamples(){
	//! [algo_base_order]
	Order baseOrder = OrderSamples::LimitOrder("BUY", 1000, 1);
	//! [algo_base_order]

	//! [arrivalpx]
	AvailableAlgoParams::FillArrivalPriceParams(baseOrder, 0.1, "Aggressive", "09:00:00 CET", "16:00:00 CET", true, true, 100000);
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	//! [arrivalpx]

	//! [darkice]
	AvailableAlgoParams::FillDarkIceParams(baseOrder, 10, "09:00:00 CET", "16:00:00 CET", true, 100000);
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	//! [darkice]

	//! [ad]
	// The Time Zone in "startTime" and "endTime" attributes is ignored and always defaulted to GMT
	AvailableAlgoParams::FillAccumulateDistributeParams(baseOrder, 10, 60, true, true, 1, true, true, "20161010-12:00:00 GMT", "20161010-16:00:00 GMT");
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	//! [ad]

	//! [twap]
	AvailableAlgoParams::FillTwapParams(baseOrder, "Marketable", "09:00:00 CET", "16:00:00 CET", true, 100000);
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	//! [twap]

	//! [vwap]
	AvailableAlgoParams::FillBalanceImpactRiskParams(baseOrder, 0.1, "Aggressive", true);
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	 //! [vwap]

	//! [balanceimpactrisk]
	AvailableAlgoParams::FillBalanceImpactRiskParams(baseOrder, 0.1, "Aggressive", true);
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	//! [balanceimpactrisk]

	//! [minimpact]
	AvailableAlgoParams::FillMinImpactParams(baseOrder, 0.3);
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	//! [minimpact]

	//! [adaptive]
	AvailableAlgoParams::FillAdaptiveParams(baseOrder, "Normal");
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
	//! [adaptive]

	//! [closepx]
    AvailableAlgoParams::FillClosePriceParams(baseOrder, 0.5, "Neutral", "12:00:00 EST", true, 100000);
    m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
    //! [closepx]
    
    //! [pctvol]
    AvailableAlgoParams::FillPctVolParams(baseOrder, 0.5, "12:00:00 EST", "14:00:00 EST", true, 100000);
    m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
    //! [pctvol]               
    
    //! [pctvolpx]
    AvailableAlgoParams::FillPriceVariantPctVolParams(baseOrder, 0.1, 0.05, 0.01, 0.2, "12:00:00 EST", "14:00:00 EST", true, 100000);
    m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
    //! [pctvolpx]
    
    //! [pctvolsz]
    AvailableAlgoParams::FillSizeVariantPctVolParams(baseOrder, 0.2, 0.4, "12:00:00 EST", "14:00:00 EST", true, 100000);
    m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
    //! [pctvolsz]
    
    //! [pctvoltm]
    AvailableAlgoParams::FillTimeVariantPctVolParams(baseOrder, 0.2, 0.4, "12:00:00 EST", "14:00:00 EST", true, 100000);
    m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), baseOrder);
    //! [pctvoltm]

	//! [jeff_vwap_algo]
	AvailableAlgoParams::FillJefferiesVWAPParams(baseOrder, "10:00:00 EST", "16:00:00 EST", 10, 10, "Exclude_Both", 130, 135, 1, 10, "Patience", false, "Midpoint");
	m_pClient->placeOrder(m_orderId++, ContractSamples::JefferiesContract(), baseOrder);
	//! [jeff_vwap_algo]

	//! [csfb_inline_algo]
	AvailableAlgoParams::FillCSFBInlineParams(baseOrder, "10:00:00 EST", "16:00:00 EST", "Patient", 10, 20, 100, "Default", false, 40, 100, 100, 35);
	m_pClient->placeOrder(m_orderId++, ContractSamples::CSFBContract(), baseOrder);
	//! [csfb_inline_algo]
	
	m_state = ST_TESTALGOSAMPLES_ACK;
}

void TestCppClient::financialAdvisorOrderSamples()
{
	//! [faorderoneaccount]
	Order faOrderOneAccount = OrderSamples::MarketOrder("BUY", 100);
	// Specify the Account Number directly
	faOrderOneAccount.account = "DU119915";
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), faOrderOneAccount);
	//! [faorderoneaccount]
	std::this_thread::sleep_for(std::chrono::seconds(1));

	//! [faordergroupequalquantity]
	Order faOrderGroupEQ = OrderSamples::LimitOrder("SELL", 200, 2000);
	faOrderGroupEQ.faGroup = "Group_Equal_Quantity";
	faOrderGroupEQ.faMethod = "EqualQuantity";
	m_pClient->placeOrder(m_orderId++, ContractSamples::SimpleFuture(), faOrderGroupEQ);
	//! [faordergroupequalquantity]
	std::this_thread::sleep_for(std::chrono::seconds(1));

	//! [faordergrouppctchange]
	Order faOrderGroupPC;
	faOrderGroupPC.action = "BUY";
	faOrderGroupPC.orderType = "MKT";
	// You should not specify any order quantity for PctChange allocation method
	faOrderGroupPC.faGroup = "Pct_Change";
	faOrderGroupPC.faMethod = "PctChange";
	faOrderGroupPC.faPercentage = "100";
	m_pClient->placeOrder(m_orderId++, ContractSamples::EurGbpFx(), faOrderGroupPC);
	//! [faordergrouppctchange]
	std::this_thread::sleep_for(std::chrono::seconds(1));

	//! [faorderprofile]
	Order faOrderProfile = OrderSamples::LimitOrder("BUY", 200, 100);
	faOrderProfile.faProfile = "Percent_60_40";
	m_pClient->placeOrder(m_orderId++, ContractSamples::EuropeanStock(), faOrderProfile);
	//! [faorderprofile]

	//! [modelorder]
	Order modelOrder = OrderSamples::LimitOrder("BUY", 200, 100);
	modelOrder.account = "DF12345";
	modelOrder.modelCode = "Technology";
	m_pClient->placeOrder(m_orderId++, ContractSamples::USStock(), modelOrder);
	//! [modelorder]

	m_state = ST_FAORDERSAMPLES_ACK;
}

void TestCppClient::financialAdvisorOperations()
{
	/*** Requesting FA information ***/
	//! [requestfaaliases]
	m_pClient->requestFA(faDataType::ALIASES);
	//! [requestfaaliases]

	//! [requestfagroups]
	m_pClient->requestFA(faDataType::GROUPS);
	//! [requestfagroups]

	//! [requestfaprofiles]
	m_pClient->requestFA(faDataType::PROFILES);
	//! [requestfaprofiles]

	/*** Replacing FA information - Fill in with the appropriate XML string. ***/
	//! [replacefaonegroup]
	m_pClient->replaceFA(faDataType::GROUPS, FAMethodSamples::FAOneGroup());
	//! [replacefaonegroup]

	//! [replacefatwogroups]
	m_pClient->replaceFA(faDataType::GROUPS, FAMethodSamples::FATwoGroups());
	//! [replacefatwogroups]

	//! [replacefaoneprofile]
	m_pClient->replaceFA(faDataType::PROFILES, FAMethodSamples::FAOneProfile());
	//! [replacefaoneprofile]

	//! [replacefatwoprofiles]
	m_pClient->replaceFA(faDataType::PROFILES, FAMethodSamples::FATwoProfiles());
	//! [replacefatwoprofiles]

	//! [reqSoftDollarTiers]
	m_pClient->reqSoftDollarTiers(4001);
	//! [reqSoftDollarTiers]

	m_state = ST_FAOPERATIONS_ACK;
}

void TestCppClient::testDisplayGroups(){
	//! [querydisplaygroups]
	m_pClient->queryDisplayGroups(9001);
	//! [querydisplaygroups]

	std::this_thread::sleep_for(std::chrono::seconds(1));

	//! [subscribetogroupevents]
	m_pClient->subscribeToGroupEvents(9002, 1);
	//! [subscribetogroupevents]

	std::this_thread::sleep_for(std::chrono::seconds(1));

	//! [updatedisplaygroup]
	m_pClient->updateDisplayGroup(9002, "8314@SMART");
	//! [updatedisplaygroup]

	std::this_thread::sleep_for(std::chrono::seconds(1));

	//! [subscribefromgroupevents]
	m_pClient->unsubscribeFromGroupEvents(9002);
	//! [subscribefromgroupevents]

	m_state = ST_TICKDATAOPERATION_ACK;
}

void TestCppClient::miscelaneous()
{
	/*** Request TWS' current time ***/
	m_pClient->reqCurrentTime();
	/*** Setting TWS logging level  ***/
	m_pClient->setServerLogLevel(5);

	m_state = ST_MISCELANEOUS_ACK;
}

void TestCppClient::reqFamilyCodes()
{
	/*** Request TWS' family codes ***/
	//! [reqfamilycodes]
	m_pClient->reqFamilyCodes();
	//! [reqfamilycodes]

	m_state = ST_FAMILYCODES_ACK;
}

void TestCppClient::reqMatchingSymbols()
{
	/*** Request TWS' mathing symbols ***/
	//! [reqmatchingsymbols]
	m_pClient->reqMatchingSymbols(11001, "IBM");
	//! [reqmatchingsymbols]
	m_state = ST_SYMBOLSAMPLES_ACK;
}

void TestCppClient::reqMktDepthExchanges()
{
	/*** Request TWS' market depth exchanges ***/
	//! [reqMktDepthExchanges]
	m_pClient->reqMktDepthExchanges();
	//! [reqMktDepthExchanges]

	m_state = ST_REQMKTDEPTHEXCHANGES_ACK;
}

void TestCppClient::reqNewsTicks()
{
	//! [reqmktdata_ticknews]
	m_pClient->reqMktData(12001, ContractSamples::USStockAtSmart(), "mdoff,292", false, false, TagValueListSPtr());
	//! [reqmktdata_ticknews]

	std::this_thread::sleep_for(std::chrono::seconds(5));

	//! [cancelmktdata2]
	m_pClient->cancelMktData(12001);
	//! [cancelmktdata2]

	m_state = ST_REQNEWSTICKS_ACK;
}

void TestCppClient::reqSmartComponents()
{
	static bool bFirstRun = true;

	if (bFirstRun) {
		m_pClient->reqMktData(13001, ContractSamples::USStockAtSmart(), "", false, false, TagValueListSPtr());

		bFirstRun = false;
	}

	std::this_thread::sleep_for(std::chrono::seconds(5));

	if (m_bboExchange.size() > 0) {
		m_pClient->cancelMktData(13001);

		//! [reqsmartcomponents]
		m_pClient->reqSmartComponents(13002, m_bboExchange);
		//! [reqsmartcomponents]
		m_state = ST_REQSMARTCOMPONENTS_ACK;
	}
}

void TestCppClient::reqNewsProviders()
{
	/*** Request TWS' news providers ***/
	//! [reqNewsProviders]
	m_pClient->reqNewsProviders();
	//! [reqNewsProviders]

	m_state = ST_NEWSPROVIDERS_ACK;
}

void TestCppClient::reqNewsArticle()
{
	/*** Request TWS' news article ***/
	//! [reqNewsArticle]
	TagValueList* list = new TagValueList();
	// list->push_back((TagValueSPtr)new TagValue("manual", "1"));
	m_pClient->reqNewsArticle(12001, "MST", "MST$06f53098", TagValueListSPtr(list));
	//! [reqNewsArticle]

	m_state = ST_REQNEWSARTICLE_ACK;
}

void TestCppClient::reqHistoricalNews(){

	//! [reqHistoricalNews]
	TagValueList* list = new TagValueList();
	list->push_back((TagValueSPtr)new TagValue("manual", "1"));
	m_pClient->reqHistoricalNews(12001, 8314, "BZ+FLY", "", "", 5, TagValueListSPtr(list));
	//! [reqHistoricalNews]

	std::this_thread::sleep_for(std::chrono::seconds(1));

	m_state = ST_REQHISTORICALNEWS_ACK;
}


void TestCppClient::reqHeadTimestamp() {
	//! [reqHeadTimeStamp]
	m_pClient->reqHeadTimestamp(14001, ContractSamples::EurGbpFx(), "MIDPOINT", 1, 1);
	//! [reqHeadTimeStamp]	
	std::this_thread::sleep_for(std::chrono::seconds(1));
	//! [cancelHeadTimestamp]
	m_pClient->cancelHeadTimestamp(14001);
	//! [cancelHeadTimestamp]
	
	m_state = ST_REQHEADTIMESTAMP_ACK;
}

void TestCppClient::reqHistogramData() {
	//! [reqHistogramData]
	m_pClient->reqHistogramData(15001, ContractSamples::IBMUSStockAtSmart(), false, "1 weeks");
	//! [reqHistogramData]
	std::this_thread::sleep_for(std::chrono::seconds(2));
	//! [cancelHistogramData]
	m_pClient->cancelHistogramData(15001);
	//! [cancelHistogramData]
	m_state = ST_REQHISTOGRAMDATA_ACK;
}

void TestCppClient::rerouteCFDOperations()
{
    //! [reqmktdatacfd]
	m_pClient->reqMktData(16001, ContractSamples::USStockCFD(), "", false, false, TagValueListSPtr());
    std::this_thread::sleep_for(std::chrono::seconds(1));
	m_pClient->reqMktData(16002, ContractSamples::EuropeanStockCFD(), "", false, false, TagValueListSPtr());
    std::this_thread::sleep_for(std::chrono::seconds(1));
	m_pClient->reqMktData(16003, ContractSamples::CashCFD(), "", false, false, TagValueListSPtr());
	std::this_thread::sleep_for(std::chrono::seconds(1));
	//! [reqmktdatacfd]

    //! [reqmktdepthcfd]
	m_pClient->reqMktDepth(16004, ContractSamples::USStockCFD(), 10, false, TagValueListSPtr());
    std::this_thread::sleep_for(std::chrono::seconds(1));
	m_pClient->reqMktDepth(16005, ContractSamples::EuropeanStockCFD(), 10, false, TagValueListSPtr());
    std::this_thread::sleep_for(std::chrono::seconds(1));
	m_pClient->reqMktDepth(16006, ContractSamples::CashCFD(), 10, false, TagValueListSPtr());
	std::this_thread::sleep_for(std::chrono::seconds(1));
	//! [reqmktdepthcfd]

	m_state = ST_REROUTECFD_ACK;
}

void TestCppClient::marketRuleOperations()
{
	m_pClient->reqContractDetails(17001, ContractSamples::IBMBond());
	m_pClient->reqContractDetails(17002, ContractSamples::IBKRStk());

    std::this_thread::sleep_for(std::chrono::seconds(2));

	//! [reqmarketrule]
	m_pClient->reqMarketRule(26);
	m_pClient->reqMarketRule(635);
	m_pClient->reqMarketRule(1388);
	//! [reqmarketrule]

	m_state = ST_MARKETRULE_ACK;
}

void TestCppClient::continuousFuturesOperations()
{
	m_pClient->reqContractDetails(18001, ContractSamples::ContFut());

	//! [reqhistoricaldatacontfut]
	std::time_t rawtime;
    std::tm* timeinfo;
    char queryTime [80];

	std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);
	std::strftime(queryTime, 80, "%Y%m%d %H:%M:%S", timeinfo);

	m_pClient->reqHistoricalData(18002, ContractSamples::ContFut(), queryTime, "1 Y", "1 month", "TRADES", 0, 1, false, TagValueListSPtr());

    std::this_thread::sleep_for(std::chrono::seconds(10));

	m_pClient->cancelHistoricalData(18002);
	//! [reqhistoricaldatacontfut]

	m_state = ST_CONTFUT_ACK;
}

void TestCppClient::reqHistoricalTicks() 
{
	//! [reqhistoricalticks]
    m_pClient->reqHistoricalTicks(19001, ContractSamples::IBMUSStockAtSmart(), "20170621 09:38:33", "", 10, "BID_ASK", 1, true, TagValueListSPtr());
    m_pClient->reqHistoricalTicks(19002, ContractSamples::IBMUSStockAtSmart(), "20170621 09:38:33", "", 10, "MIDPOINT", 1, true, TagValueListSPtr());
    m_pClient->reqHistoricalTicks(19003, ContractSamples::IBMUSStockAtSmart(), "20170621 09:38:33", "", 10, "TRADES", 1, true, TagValueListSPtr());
    //! [reqhistoricalticks]
    m_state = ST_REQHISTORICALTICKS_ACK;
}

void TestCppClient::reqTickByTickData() 
{
    /*** Requesting tick-by-tick data (only refresh) ***/
    
    m_pClient->reqTickByTickData(20001, ContractSamples::EuropeanStock(), "Last", 0, false);
    m_pClient->reqTickByTickData(20002, ContractSamples::EuropeanStock(), "AllLast", 0, false);
    m_pClient->reqTickByTickData(20003, ContractSamples::EuropeanStock(), "BidAsk", 0, true);
    m_pClient->reqTickByTickData(20004, ContractSamples::EurGbpFx(), "MidPoint", 0, false);

    std::this_thread::sleep_for(std::chrono::seconds(10));

	//! [canceltickbytick]
    m_pClient->cancelTickByTickData(20001);
    m_pClient->cancelTickByTickData(20002);
    m_pClient->cancelTickByTickData(20003);
    m_pClient->cancelTickByTickData(20004);
    //! [canceltickbytick]
	
    /*** Requesting tick-by-tick data (historical + refresh) ***/
    //! [reqtickbytick]
    m_pClient->reqTickByTickData(20005, ContractSamples::EuropeanStock(), "Last", 10, false);
    m_pClient->reqTickByTickData(20006, ContractSamples::EuropeanStock(), "AllLast", 10, false);
    m_pClient->reqTickByTickData(20007, ContractSamples::EuropeanStock(), "BidAsk", 10, false);
    m_pClient->reqTickByTickData(20008, ContractSamples::EurGbpFx(), "MidPoint", 10, true);
	//! [reqtickbytick]
	
    std::this_thread::sleep_for(std::chrono::seconds(10));

    m_pClient->cancelTickByTickData(20005);
    m_pClient->cancelTickByTickData(20006);
    m_pClient->cancelTickByTickData(20007);
    m_pClient->cancelTickByTickData(20008);

    m_state = ST_REQTICKBYTICKDATA_ACK;
}

void TestCppClient::whatIfSamples()
{
    /*** Placing waht-if order ***/
    //! [whatiforder]
    m_pClient->placeOrder(m_orderId++, ContractSamples::USStockAtSmart(), OrderSamples::WhatIfLimitOrder("BUY", 200, 120));
    //! [whatiforder]

    m_state = ST_WHATIFSAMPLES_ACK;
}

//! [nextvalidid]
void TestCppClient::nextValidId( OrderId orderId)
{
	printf("Next Valid Id: %ld\n", orderId);
	m_orderId = orderId;
	//! [nextvalidid]

    //m_state = ST_TICKOPTIONCOMPUTATIONOPERATION; 
    //m_state = ST_TICKDATAOPERATION; 
    //m_state = ST_REQTICKBYTICKDATA; 
    //m_state = ST_REQHISTORICALTICKS; 
    //m_state = ST_CONTFUT; 
    //m_state = ST_PNLSINGLE; 
    //m_state = ST_PNL; 
	//m_state = ST_DELAYEDTICKDATAOPERATION; 
	//m_state = ST_MARKETDEPTHOPERATION;
	//m_state = ST_REALTIMEBARS;
	//m_state = ST_MARKETDATATYPE;
	//m_state = ST_HISTORICALDATAREQUESTS;
	//m_state = ST_CONTRACTOPERATION;
	//m_state = ST_MARKETSCANNERS;
	//m_state = ST_FUNDAMENTALS;
	//m_state = ST_BULLETINS;
	//m_state = ST_ACCOUNTOPERATIONS;
	  m_state = ST_ORDEROPERATIONS;    //1
	//m_state = ST_OCASAMPLES;
	//m_state = ST_CONDITIONSAMPLES;
	//m_state = ST_BRACKETSAMPLES;
	//m_state = ST_HEDGESAMPLES;
	//m_state = ST_TESTALGOSAMPLES;
	//m_state = ST_FAORDERSAMPLES;
	//m_state = ST_FAOPERATIONS;
	//m_state = ST_DISPLAYGROUPS;
	//m_state = ST_MISCELANEOUS;
	//m_state = ST_FAMILYCODES;
	//m_state = ST_SYMBOLSAMPLES;
	//m_state = ST_REQMKTDEPTHEXCHANGES;
	//m_state = ST_REQNEWSTICKS;
	//m_state = ST_REQSMARTCOMPONENTS;
	//m_state = ST_NEWSPROVIDERS;
	//m_state = ST_REQNEWSARTICLE;
	//m_state = ST_REQHISTORICALNEWS;
	//m_state = ST_REQHEADTIMESTAMP;
	//m_state = ST_REQHISTOGRAMDATA;
	//m_state = ST_REROUTECFD;
	//m_state = ST_MARKETRULE;
	//m_state = ST_PING;
	//m_state = ST_WHATIFSAMPLES;
}


void TestCppClient::currentTime( long time)
{
	if ( m_state == ST_PING_ACK) {
		time_t t = ( time_t)time;
		struct tm * timeinfo = localtime ( &t);
		printf( "The current date/time is: %s", asctime( timeinfo));

		time_t now = ::time(NULL);
		m_sleepDeadline = now + SLEEP_BETWEEN_PINGS;

		m_state = ST_PING_ACK;
	}
}

//! [error]
void TestCppClient::error(int id, int errorCode, const std::string& errorString)
{
	if (errorCode == 105) {  //Code: 105, Msg: Order being modified does not match original order.
		rprintf("Cancel Order %d because Code: 105, Msg: Order being modified does not match original order. \n", id);
		m_pClient->cancelOrder(id);
	}
	else if (errorCode == 104) {  //Code: 104, Msg: Cannot modify a filled order.
		printf("%d is filled and can not be modified\n", id);
		for (int i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder * o = m_orderList[i];
			if (o->orderId == id) {
				o->status = "Filled";
				wangShareOrderInfo(o);
			}
		}
	}
	else if (errorCode == 103) {  //Code: 103, Msg: Duplicate order id
		rprintf("Remove Order %d because Code: 103, Msg: Duplicate order id. \n", id);
		for (int i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder* o = m_orderList[i];
			if (o->orderId == id) {
				delete o;
				m_orderList.erase(m_orderList.begin() + i);
			}
		}
	}
	else if (errorCode == 200) {  //Code: 200, Msg: The contract description specified is ambiguous.
		rprintf("The contract description specified is ambiguous\n", id);
		for (int i = 0; i < allStockNum; i++) {
			if (allStock[i].tikerID != id) continue;
			Contract contract;
			contract.symbol = allStock[i].symbol;
			contract.secType = "STK";
			contract.currency = "USD";
			if (allStock[i].primaryExchange == "") contract.primaryExchange = allStock[i].primaryExchange = "ARCA";
			else contract.primaryExchange = allStock[i].primaryExchange = "ISLAND";
			m_pClient->reqMktData(allStock[i].tikerID, contract, "", false, false, TagValueListSPtr());
			rprintf("Request Market Data AGAIN of %s, tikerID %d after specifying the primary exachange to %s\n", contract.symbol.c_str(), allStock[i].tikerID, allStock[i].primaryExchange.c_str());
			allStock[i].dataRequested = true;
			break;
		}
	}
	else if (errorCode == 202) {  //Code: 202, Msg: Order Canceled
		printf("%d is cancelled. Error code 202\n", id);
		for (int i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder* o = m_orderList[i];
			if (o->orderId == id  && o->status != "Cancelled") {
				o->status = "Cancelled";
				wangShareOrderInfo(o);
				soundPlay.push_back(3);
			}
		}
	}
	else if (errorCode == 1100) {  //Msg: Connectivity between IB and Trader Workstation has been lost.
		rprintf("Connectivity between IB and Trader Workstation has been lost. \n");
	}
	else if (errorCode == 1101) {  //Connectivity between IB and TWS has been restored- data lost
	}
	else if (errorCode == 10147) {  //OrderId  that needs to be cancelled is not found.
		rprintf("Remove Order %d because Code: 10147, Msg: OrderId  that needs to be cancelled is not found.\n ", id);
		for (int i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder * o = m_orderList[i];
			if (o->orderId == id) {
				o->status = "Cancelled";
				wangShareOrderInfo(o);
				delete o;
				m_orderList.erase(m_orderList.begin() + i);
			}
		}
	}
	else if (errorCode == 10148) {  //	OrderId that needs to be cancelled cannot be cancelled, state: PendingSubmit.
		for (int i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder * o = m_orderList[i];
			if (o->orderId == id) {
				if (o->filledQty < 0.01) {
					o->status = "Cancelled";
					wangShareOrderInfo(o);
					delete o;
					m_orderList.erase(m_orderList.begin() + i);
					rprintf("Remove Order %d because Code: 10148, Msg: OrderId that needs to be cancelled cannot be cancelled.\n ", id);
				}
				break;
			}
		}
	}
	else if (id == -1 && errorString.find("data farm connection is OK") != std::string::npos) {   //nothing here.

	}
	else if (id > 0 && errorString.find("The contract is not available for short sale") != std::string::npos) {   //No short sale
		rprintf("Stop the short sale of stock");
		for (auto order : m_orderList) {
			if (order->orderId == id) {
				for (int i = 0; i < m_stocknum; i++) {
					if (m_stock[i].symbol == order->symbol) {
						rprintf("%s \n", order->symbol.c_str());
						m_stock[i].canShort = false;
						m_stock[i].canSell = false;
						break;
					}
				}
			}
		}
	}
	else
		rprintf("Error. Id: %d, Code: %d, Msg: %s\n", id, errorCode, errorString.c_str());
}
//! [error]

//! [tickprice]
void TestCppClient::tickPrice( TickerId tickerId, TickType field, double price, const TickAttrib& attribs) {
	int i;
	for (i = 0; i < allStockNum; i++) {
		if (allStock[i].tikerID == tickerId) break;
	}
	bool print = false;
	if (i < allStockNum) {
		WangStock& s = allStock[i];
		add_shared_tick(s.symbol, (int)field, price);
		if (print) printf("Share %s %d %g\n", s.symbol.c_str(), (int)field, price);
	}
	else {
		if (print) rprintf("Wrong tickerID %d %d %g\n", tickerId, (int)field, price);
		if (print) printf("Wang Tick Price. Ticker Id: %ld, Field: %d, Price: %g, CanAutoExecute: %d, PastLimit: %d, PreOpen: %d\n", tickerId, (int)field, price, attribs.canAutoExecute, attribs.pastLimit, attribs.preOpen);
	}

	for (i = 0; i < m_stocknum; i++) {
		if (m_stock[i].tikerID == tickerId) break;
	}
	if (i < m_stocknum) {
		Stock& s = m_stock[i];
		switch ((int)field) {
		case 1:
			s.bidPrice = price;
			if (print) printf("bidPrice = %g\n", price);
			break;
		case 2:
			s.askPrice = price;
			if (print) printf("askPrice = %g\n", price);
			break;
		case 4:
			s.lastPrice = price;
			if (print) printf("lastPrice = %g\n", price);
			break;
		case 6:
			s.dayHigh = price;
			if (print) printf("dayHigh = %g\n", price);
			break;
		case 7:
			s.dayLow = price;
			if (print) printf("dayLow = %g\n", price);
			break;
		case 9:
			s.closePrice = price;
			if (print) printf("closePrice = %g\n", price);
			break;
		default:
			if (print) printf("Wang Tick Price. Ticker Id: %ld, Field: %d, Price: %g, CanAutoExecute: %d, PastLimit: %d, PreOpen: %d\n", tickerId, (int)field, price, attribs.canAutoExecute, attribs.pastLimit, attribs.preOpen);
			break;
		}
	}
	else {
		if (print) printf("Wang Tick Price. Ticker Id: %ld, Field: %d, Price: %g, CanAutoExecute: %d, PastLimit: %d, PreOpen: %d\n", tickerId, (int)field, price, attribs.canAutoExecute, attribs.pastLimit, attribs.preOpen);
	}
}
//! [tickprice]

//! [ticksize]
void TestCppClient::tickSize( TickerId tickerId, TickType field, int size) {
	int i;

	for (i = 0; i < allStockNum; i++) {
		if (allStock[i].tikerID == tickerId) break;
	}
	bool print = false;
	if (i < allStockNum) {
		WangStock &s = allStock[i];
		add_shared_tick(s.symbol, (int)field, size);
	}
	else {
		if (print) rprintf("Tick Size. Ticker Id: %ld, Field: %d, Size: %d\n", tickerId, (int)field, size);
	}


	for (i = 0; i < m_stocknum; i++) {
		if (m_stock[i].tikerID == tickerId) break;
	}
	if (i < m_stocknum) {
		Stock& s = m_stock[i];
		switch ((int)field) {
		case 0:
			s.bidSize = size;
			if (print) printf("bidSize = %d\n", size);
			break;
		case 3:
			s.askSize = size;
			if (print) printf("askSize = %d\n", size);
			break;
		case 5:
			s.lastSize = size;
			if (print) printf("lastSize = %d\n", size);
			break;
		case 8:
			s.dayVolume = size;
			if (print) printf("dayVolume = %d\n", size);
			break;
		default:
			if (print) printf("Tick Size. Ticker Id: %ld, Field: %d, Size: %d\n", tickerId, (int)field, size);
			break;

		}
	}
	else {
		if (print) printf("Tick Size. Ticker Id: %ld, Field: %d, Size: %d\n", tickerId, (int)field, size);
	}
}
//! [ticksize]

//! [tickoptioncomputation]
void TestCppClient::tickOptionComputation( TickerId tickerId, TickType tickType, double impliedVol, double delta,
                                          double optPrice, double pvDividend,
                                          double gamma, double vega, double theta, double undPrice) {
	printf( "TickOptionComputation. Ticker Id: %ld, Type: %d, ImpliedVolatility: %g, Delta: %g, OptionPrice: %g, pvDividend: %g, Gamma: %g, Vega: %g, Theta: %g, Underlying Price: %g\n", tickerId, (int)tickType, impliedVol, delta, optPrice, pvDividend, gamma, vega, theta, undPrice);
}
//! [tickoptioncomputation]

//! [tickgeneric]
void TestCppClient::tickGeneric(TickerId tickerId, TickType tickType, double value) {
	//printf( "Tick Generic. Ticker Id: %ld, Type: %d, Value: %g\n", tickerId, (int)tickType, value);
}
//! [tickgeneric]

//! [tickstring]
void TestCppClient::tickString(TickerId tickerId, TickType tickType, const std::string& value) {
	//printf( "Tick String. Ticker Id: %ld, Type: %d, Value: %s\n", tickerId, (int)tickType, value.c_str());
}
//! [tickstring]

void TestCppClient::tickEFP(TickerId tickerId, TickType tickType, double basisPoints, const std::string& formattedBasisPoints,
                            double totalDividends, int holdDays, const std::string& futureLastTradeDate, double dividendImpact, double dividendsToLastTradeDate) {
	printf( "TickEFP. %ld, Type: %d, BasisPoints: %g, FormattedBasisPoints: %s, Total Dividends: %g, HoldDays: %d, Future Last Trade Date: %s, Dividend Impact: %g, Dividends To Last Trade Date: %g\n", tickerId, (int)tickType, basisPoints, formattedBasisPoints.c_str(), totalDividends, holdDays, futureLastTradeDate.c_str(), dividendImpact, dividendsToLastTradeDate);
}

//! [orderstatus]
void TestCppClient::orderStatus(OrderId orderId, const std::string& status, double filled,
		double remaining, double avgFillPrice, int permId, int parentId,
		double lastFillPrice, int clientId, const std::string& whyHeld, double mktCapPrice){

	if (orderId == 0) {
#ifndef NO_OUTPUT
		printf("OrderStatus. Id: %ld, Status: %s, Filled: %g, Remaining: %g, AvgFillPrice: %g, PermId: %d, LastFillPrice: %g, ClientId: %d, WhyHeld: %s, MktCapPrice: %g\n", orderId, status.c_str(), filled, remaining, avgFillPrice, permId, lastFillPrice, clientId, whyo->profit =Held.c_str(), mktCapPrice);
#endif
		return;
	}

	MyOrder * o = NULL;
	for (unsigned int i = 0; i < m_orderList.size(); i++) {
		if (m_orderList[i]->orderId == orderId) {
			o = m_orderList[i];
			break;
		}
	}
	if (!o) {
		o = new MyOrder;
		m_orderList.push_back(o);
		rprintf("Update New Order in Order Status, something wrong\n");
	}

	o->orderId = orderId;
	o->status = status;
	o->filledQty = filled;
	o->remaining = remaining;
	o->avgFillPrice = avgFillPrice;
	if (o->status == "Filled" && o->fillTime == 0) {
		o->fillTime = time(NULL);
		ExecTime e;
		e.action = o->action;
		e.fillTime = o->fillTime;
		e.qty = o->filledQty;

		int i;
		for (i = 0; i < m_stocknum; i++) {
			if (m_stock[i].symbol == o->symbol) break;
		}
		if (i < m_stocknum) {
			Stock& s = m_stock[i];
			if (o->action == "SELL") {
				double delta = deltaCalculation(o->avgFillPrice, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
				delta += o->filledQty;
				double price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
				o->profit = o->filledQty * (o->avgFillPrice - price) * 0.5;
			}
			else {
				double delta = deltaCalculation(o->avgFillPrice, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
				delta -= o->filledQty;
				double price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
				o->profit = - o->filledQty * (o->avgFillPrice - price) * 0.5;
			}
			s.actions.push_front(Action(4, o->action, o->lmtPrice, o->lmtPrice));
		}
	}
	wangShareOrderInfo(o);

#ifndef NO_OUTPUT
	printf("OrderStatus. Id: %ld, Status: %s, Filled: %g, Remaining: %g, AvgFillPrice: %g, PermId: %d, LastFillPrice: %g, ClientId: %d, WhyHeld: %s, MktCapPrice: %g\n", orderId, status.c_str(), filled, remaining, avgFillPrice, permId, lastFillPrice, clientId, whyHeld.c_str(), mktCapPrice);
#endif
}
//! [orderstatus]

//! [openorder]
void TestCppClient::openOrder( OrderId orderId, const Contract& contract, const Order& order, const OrderState& orderState) {
	if (orderId == 0) {
	}
	else if (contract.secType == "STK") {
		MyOrder * o = NULL;
		for (unsigned int i = 0; i < m_orderList.size(); i++) {
			if (m_orderList[i]->orderId == orderId) {
				o = m_orderList[i];
				break;
			}
		}
		if (!o) {
			o = new MyOrder;
			m_orderList.push_back(o);
		}

		o->orderId = orderId;
		o->symbol = contract.symbol;
		o->account = order.account;
		o->action = order.action;
		o->orderType = order.orderType;
		o->totalQty = order.totalQuantity;
		o->lmtPrice = order.lmtPrice;
		o->status = orderState.status;
		o->exchange = contract.exchange;
		o->currency = contract.currency;
		wangShareOrderInfo(o);
	}

#ifndef NO_OUTPUT
	printf("OpenOrder. PermId: %ld, ClientId: %ld, OrderId: %ld, Account: %s, Symbol: %s, SecType: %s, Exchange: %s:, Action: %s, OrderType:%s, TotalQty: %g, FilledQty: %g, CashQty: %g, "
			"LmtPrice: %g, AuxPrice: %g, Status: %s\n",
			order.permId, order.clientId, orderId, order.account.c_str(), contract.symbol.c_str(), contract.secType.c_str(), contract.exchange.c_str(),
			order.action.c_str(), order.orderType.c_str(), order.totalQuantity, order.filledQuantity, order.cashQty == UNSET_DOUBLE ? 0 : order.cashQty, order.lmtPrice, order.auxPrice, orderState.status.c_str());
#endif
}
//! [openorder]

//! [openorderend]
void TestCppClient::openOrderEnd() {
#ifndef NO_OUTPUT
	printf( "OpenOrderEnd\n");
#endif
	m_openOrderRequestStatus = 0;
}
//! [openorderend]

void TestCppClient::winError( const std::string& str, int lastError) {}
void TestCppClient::connectionClosed() {
	rprintf( "Connection Closed\n");
}

//! [updateaccountvalue]
void TestCppClient::updateAccountValue(const std::string& key, const std::string& val,
                                       const std::string& currency, const std::string& accountName) {
	printf("UpdateAccountValue. Key: %s, Value: %s, Currency: %s, Account Name: %s\n", key.c_str(), val.c_str(), currency.c_str(), accountName.c_str());
}
//! [updateaccountvalue]

//! [updateportfolio]
void TestCppClient::updatePortfolio(const Contract& contract, double position,
                                    double marketPrice, double marketValue, double averageCost,
                                    double unrealizedPNL, double realizedPNL, const std::string& accountName){
	printf("UpdatePortfolio. %s, %s @ %s: Position: %g, MarketPrice: %g, MarketValue: %g, AverageCost: %g, UnrealizedPNL: %g, RealizedPNL: %g, AccountName: %s\n", (contract.symbol).c_str(), (contract.secType).c_str(), (contract.primaryExchange).c_str(), position, marketPrice, marketValue, averageCost, unrealizedPNL, realizedPNL, accountName.c_str());
}
//! [updateportfolio]

//! [updateaccounttime]
void TestCppClient::updateAccountTime(const std::string& timeStamp) {
	printf( "UpdateAccountTime. Time: %s\n", timeStamp.c_str());
}
//! [updateaccounttime]

//! [accountdownloadend]
void TestCppClient::accountDownloadEnd(const std::string& accountName) {
	printf( "Account download finished: %s\n", accountName.c_str());
}
//! [accountdownloadend]

//! [contractdetails]
void TestCppClient::contractDetails( int reqId, const ContractDetails& contractDetails) {
	printf( "ContractDetails begin. ReqId: %d\n", reqId);
	printContractMsg(contractDetails.contract);
	printContractDetailsMsg(contractDetails);
	printf( "ContractDetails end. ReqId: %d\n", reqId);
}
//! [contractdetails]

//! [bondcontractdetails]
void TestCppClient::bondContractDetails( int reqId, const ContractDetails& contractDetails) {
	printf( "BondContractDetails begin. ReqId: %d\n", reqId);
	printBondContractDetailsMsg(contractDetails);
	printf( "BondContractDetails end. ReqId: %d\n", reqId);
}
//! [bondcontractdetails]

void TestCppClient::printContractMsg(const Contract& contract) {
	printf("\tConId: %ld\n", contract.conId);
	printf("\tSymbol: %s\n", contract.symbol.c_str());
	printf("\tSecType: %s\n", contract.secType.c_str());
	printf("\tLastTradeDateOrContractMonth: %s\n", contract.lastTradeDateOrContractMonth.c_str());
	printf("\tStrike: %g\n", contract.strike);
	printf("\tRight: %s\n", contract.right.c_str());
	printf("\tMultiplier: %s\n", contract.multiplier.c_str());
	printf("\tExchange: %s\n", contract.exchange.c_str());
	printf("\tPrimaryExchange: %s\n", contract.primaryExchange.c_str());
	printf("\tCurrency: %s\n", contract.currency.c_str());
	printf("\tLocalSymbol: %s\n", contract.localSymbol.c_str());
	printf("\tTradingClass: %s\n", contract.tradingClass.c_str());
}

void TestCppClient::printContractDetailsMsg(const ContractDetails& contractDetails) {
	printf("\tMarketName: %s\n", contractDetails.marketName.c_str());
	printf("\tMinTick: %g\n", contractDetails.minTick);
	printf("\tPriceMagnifier: %ld\n", contractDetails.priceMagnifier);
	printf("\tOrderTypes: %s\n", contractDetails.orderTypes.c_str());
	printf("\tValidExchanges: %s\n", contractDetails.validExchanges.c_str());
	printf("\tUnderConId: %d\n", contractDetails.underConId);
	printf("\tLongName: %s\n", contractDetails.longName.c_str());
	printf("\tContractMonth: %s\n", contractDetails.contractMonth.c_str());
	printf("\tIndystry: %s\n", contractDetails.industry.c_str());
	printf("\tCategory: %s\n", contractDetails.category.c_str());
	printf("\tSubCategory: %s\n", contractDetails.subcategory.c_str());
	printf("\tTimeZoneId: %s\n", contractDetails.timeZoneId.c_str());
	printf("\tTradingHours: %s\n", contractDetails.tradingHours.c_str());
	printf("\tLiquidHours: %s\n", contractDetails.liquidHours.c_str());
	printf("\tEvRule: %s\n", contractDetails.evRule.c_str());
	printf("\tEvMultiplier: %g\n", contractDetails.evMultiplier);
	printf("\tMdSizeMultiplier: %d\n", contractDetails.mdSizeMultiplier);
	printf("\tAggGroup: %d\n", contractDetails.aggGroup);
	printf("\tUnderSymbol: %s\n", contractDetails.underSymbol.c_str());
	printf("\tUnderSecType: %s\n", contractDetails.underSecType.c_str());
	printf("\tMarketRuleIds: %s\n", contractDetails.marketRuleIds.c_str());
	printf("\tRealExpirationDate: %s\n", contractDetails.realExpirationDate.c_str());
	printf("\tLastTradeTime: %s\n", contractDetails.lastTradeTime.c_str());
	printContractDetailsSecIdList(contractDetails.secIdList);
}

void TestCppClient::printContractDetailsSecIdList(const TagValueListSPtr &secIdList) {
	const int secIdListCount = secIdList.get() ? secIdList->size() : 0;
	if (secIdListCount > 0) {
		printf("\tSecIdList: {");
		for (int i = 0; i < secIdListCount; ++i) {
			const TagValue* tagValue = ((*secIdList)[i]).get();
			printf("%s=%s;",tagValue->tag.c_str(), tagValue->value.c_str());
		}
		printf("}\n");
	}
}

void TestCppClient::printBondContractDetailsMsg(const ContractDetails& contractDetails) {
	printf("\tSymbol: %s\n", contractDetails.contract.symbol.c_str());
	printf("\tSecType: %s\n", contractDetails.contract.secType.c_str());
	printf("\tCusip: %s\n", contractDetails.cusip.c_str());
	printf("\tCoupon: %g\n", contractDetails.coupon);
	printf("\tMaturity: %s\n", contractDetails.maturity.c_str());
	printf("\tIssueDate: %s\n", contractDetails.issueDate.c_str());
	printf("\tRatings: %s\n", contractDetails.ratings.c_str());
	printf("\tBondType: %s\n", contractDetails.bondType.c_str());
	printf("\tCouponType: %s\n", contractDetails.couponType.c_str());
	printf("\tConvertible: %s\n", contractDetails.convertible ? "yes" : "no");
	printf("\tCallable: %s\n", contractDetails.callable ? "yes" : "no");
	printf("\tPutable: %s\n", contractDetails.putable ? "yes" : "no");
	printf("\tDescAppend: %s\n", contractDetails.descAppend.c_str());
	printf("\tExchange: %s\n", contractDetails.contract.exchange.c_str());
	printf("\tCurrency: %s\n", contractDetails.contract.currency.c_str());
	printf("\tMarketName: %s\n", contractDetails.marketName.c_str());
	printf("\tTradingClass: %s\n", contractDetails.contract.tradingClass.c_str());
	printf("\tConId: %ld\n", contractDetails.contract.conId);
	printf("\tMinTick: %g\n", contractDetails.minTick);
	printf("\tMdSizeMultiplier: %d\n", contractDetails.mdSizeMultiplier);
	printf("\tOrderTypes: %s\n", contractDetails.orderTypes.c_str());
	printf("\tValidExchanges: %s\n", contractDetails.validExchanges.c_str());
	printf("\tNextOptionDate: %s\n", contractDetails.nextOptionDate.c_str());
	printf("\tNextOptionType: %s\n", contractDetails.nextOptionType.c_str());
	printf("\tNextOptionPartial: %s\n", contractDetails.nextOptionPartial ? "yes" : "no");
	printf("\tNotes: %s\n", contractDetails.notes.c_str());
	printf("\tLong Name: %s\n", contractDetails.longName.c_str());
	printf("\tEvRule: %s\n", contractDetails.evRule.c_str());
	printf("\tEvMultiplier: %g\n", contractDetails.evMultiplier);
	printf("\tAggGroup: %d\n", contractDetails.aggGroup);
	printf("\tMarketRuleIds: %s\n", contractDetails.marketRuleIds.c_str());
	printf("\tTimeZoneId: %s\n", contractDetails.timeZoneId.c_str());
	printf("\tLastTradeTime: %s\n", contractDetails.lastTradeTime.c_str());
	printContractDetailsSecIdList(contractDetails.secIdList);
}

//! [contractdetailsend]
void TestCppClient::contractDetailsEnd( int reqId) {
	printf( "ContractDetailsEnd. %d\n", reqId);
}
//! [contractdetailsend]

//! [execdetails]
void TestCppClient::execDetails( int reqId, const Contract& contract, const Execution& execution) {
	if (contract.secType == "STK") {  //current only for Stock.
		for (int i = m_orderList.size() - 1; i >= 0; i--) {
			MyOrder * o = m_orderList[i];
			if (o->orderId == execution.orderId) {
				o->remaining -= execution.shares;
				o->filledQty += execution.shares;
				if (fabs(o->remaining) < 0.01) {
					printf("Order Filled ");
					soundPlay.push_back(1);
				}
				else {
					printf("Order Partially Filled ");
					o->partiallyFilled = true;
					soundPlay.push_back(2);
				}
				for (int index = 0; index < m_stocknum; index++) {
					Stock& s = m_stock[index];
					if (s.symbol == o->symbol) {
						double price;
						if (o->action == "SELL") {
							double delta = deltaCalculation(execution.price, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
							delta += execution.shares;
							price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
							o->profit += execution.shares * (execution.price - price) * 0.5;
							s.totalProfit += o->profit;
						}
						else {
							double delta = deltaCalculation(execution.price, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
							delta -= execution.shares;
							price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
							o->profit += -execution.shares * (execution.price - price) * 0.5;
							s.totalProfit += o->profit;
						}
						printf("%s, total Qty %g. - Profit %g (expected %g) \n", o->action.c_str(), o->totalQty, round(o->profit, 0.01), s.takenProfit / 2);
						ExecPair p; p.execId = execution.execId; p.orderId = o->orderId; p.stockIndex = index;
						m_execPairs.push_back(p);
						break;
					}
				}
				wangShareOrderInfo(o);
				break;
			}
		}
	}

	printf(" %g %s %s at $%g*****\n",execution.shares, contract.symbol.c_str(), contract.secType.c_str(), execution.price);
	//printf( "ExecDetails. ReqId: %d - %s, %s, %s - %s, %ld, %g, %d\n", reqId, contract.symbol.c_str(), contract.secType.c_str(), contract.currency.c_str(), execution.execId.c_str(), execution.orderId, execution.shares, execution.lastLiquidity);
}
//! [execdetails]

//! [execdetailsend]
void TestCppClient::execDetailsEnd( int reqId) {
	printf( "ExecDetailsEnd. %d\n", reqId);
}
//! [execdetailsend]

//! [updatemktdepth]
void TestCppClient::updateMktDepth(TickerId id, int position, int operation, int side,
                                   double price, int size) {
	printf( "UpdateMarketDepth. %ld - Position: %d, Operation: %d, Side: %d, Price: %g, Size: %d\n", id, position, operation, side, price, size);
}
//! [updatemktdepth]

//! [updatemktdepthl2]
void TestCppClient::updateMktDepthL2(TickerId id, int position, const std::string& marketMaker, int operation,
                                     int side, double price, int size, bool isSmartDepth) {
	printf( "UpdateMarketDepthL2. %ld - Position: %d, Operation: %d, Side: %d, Price: %g, Size: %d, isSmartDepth: %d\n", id, position, operation, side, price, size, isSmartDepth);
}
//! [updatemktdepthl2]

//! [updatenewsbulletin]
void TestCppClient::updateNewsBulletin(int msgId, int msgType, const std::string& newsMessage, const std::string& originExch) {
	printf( "News Bulletins. %d - Type: %d, Message: %s, Exchange of Origin: %s\n", msgId, msgType, newsMessage.c_str(), originExch.c_str());
}
//! [updatenewsbulletin]

//! [managedaccounts]
void TestCppClient::managedAccounts( const std::string& accountsList) {
	printf( "Account List: %s\n", accountsList.c_str());
}
//! [managedaccounts]

//! [receivefa]
void TestCppClient::receiveFA(faDataType pFaDataType, const std::string& cxml) {
	std::cout << "Receiving FA: " << (int)pFaDataType << std::endl << cxml << std::endl;
}
//! [receivefa]

//! [historicaldata]
void TestCppClient::historicalData(TickerId reqId, const Bar& bar) {
	printf( "HistoricalData. ReqId: %ld - Date: %s, Open: %g, High: %g, Low: %g, Close: %g, Volume: %lld, Count: %d, WAP: %g\n", reqId, bar.time.c_str(), bar.open, bar.high, bar.low, bar.close, bar.volume, bar.count, bar.wap);
}
//! [historicaldata]

//! [historicaldataend]
void TestCppClient::historicalDataEnd(int reqId, const std::string& startDateStr, const std::string& endDateStr) {
	std::cout << "HistoricalDataEnd. ReqId: " << reqId << " - Start Date: " << startDateStr << ", End Date: " << endDateStr << std::endl;	
}
//! [historicaldataend]

//! [scannerparameters]
void TestCppClient::scannerParameters(const std::string& xml) {
	printf( "ScannerParameters. %s\n", xml.c_str());
}
//! [scannerparameters]

//! [scannerdata]
void TestCppClient::scannerData(int reqId, int rank, const ContractDetails& contractDetails,
                                const std::string& distance, const std::string& benchmark, const std::string& projection,
                                const std::string& legsStr) {
	printf( "ScannerData. %d - Rank: %d, Symbol: %s, SecType: %s, Currency: %s, Distance: %s, Benchmark: %s, Projection: %s, Legs String: %s\n", reqId, rank, contractDetails.contract.symbol.c_str(), contractDetails.contract.secType.c_str(), contractDetails.contract.currency.c_str(), distance.c_str(), benchmark.c_str(), projection.c_str(), legsStr.c_str());
}
//! [scannerdata]

//! [scannerdataend]
void TestCppClient::scannerDataEnd(int reqId) {
	printf( "ScannerDataEnd. %d\n", reqId);
}
//! [scannerdataend]

//! [realtimebar]
void TestCppClient::realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
                                long volume, double wap, int count) {
	printf( "RealTimeBars. %ld - Time: %ld, Open: %g, High: %g, Low: %g, Close: %g, Volume: %ld, Count: %d, WAP: %g\n", reqId, time, open, high, low, close, volume, count, wap);
}
//! [realtimebar]

//! [fundamentaldata]
void TestCppClient::fundamentalData(TickerId reqId, const std::string& data) {
	printf( "FundamentalData. ReqId: %ld, %s\n", reqId, data.c_str());
}
//! [fundamentaldata]

void TestCppClient::deltaNeutralValidation(int reqId, const DeltaNeutralContract& deltaNeutralContract) {
	printf( "DeltaNeutralValidation. %d, ConId: %ld, Delta: %g, Price: %g\n", reqId, deltaNeutralContract.conId, deltaNeutralContract.delta, deltaNeutralContract.price);
}

//! [ticksnapshotend]
void TestCppClient::tickSnapshotEnd(int reqId) {
	printf( "TickSnapshotEnd: %d\n", reqId);
}
//! [ticksnapshotend]

//! [marketdatatype]
void TestCppClient::marketDataType(TickerId reqId, int marketDataType) {
	printf( "MarketDataType. ReqId: %ld, Type: %d\n", reqId, marketDataType);
}
//! [marketdatatype]

//! [commissionreport]
void TestCppClient::commissionReport( const CommissionReport& commissionReport) {
	for (int i = m_execPairs.size() - 1; i >= 0; i--) {
		if (m_execPairs[i].execId == commissionReport.execId) {
			m_stock[m_execPairs[i].stockIndex].totalCommission += commissionReport.commission;
			for (auto o : m_orderList) {
				if (o->orderId == m_execPairs[i].orderId) {
					o->commission += commissionReport.commission;
					wangShareOrderInfo(o);
					break;
				}
			}
			printf("commission cost %g\n", round(commissionReport.commission, 0.01));
			break;
		}
	}

	//printf( "CommissionReport. %s - %g %s RPNL %g\n", commissionReport.execId.c_str(), commissionReport.commission, commissionReport.currency.c_str(), commissionReport.realizedPNL);
}
//! [commissionreport]

//! [position]
void TestCppClient::position( const std::string& account, const Contract& contract, double position, double avgCost) {
	int i;
	for (i = 0; i < m_stocknum; i++) {
		if (m_stock[i].symbol == contract.symbol) break;
	}
	if (i < m_stocknum) {
		Stock& s = m_stock[i];
		if (contract.secType == "STK") {
			if (account == ACCOUNT1) {
				s.position[0] = position;
				s.avgCost[0] = avgCost;
			}
			else {
				s.position[1] = position;
				s.avgCost[1] = avgCost;
			}
			write_IB_position(s.symbol, s.position[0] + s.position[1]);
		}
		else if (contract.secType == "OPT")
		{
			if (account == ACCOUNT1) {
				s.opPosition[0].insert(s.symbol, contract.lastTradeDateOrContractMonth, contract.strike, contract.right, position, avgCost);
			}
			else {
				s.opPosition[1].insert(s.symbol, contract.lastTradeDateOrContractMonth, contract.strike, contract.right, position, avgCost);
			}
			//printf("Position. %s - Symbol: %s, SecType: %s, Position: %g, Avg Cost: %g\n", account.c_str(), contract.symbol.c_str(), contract.secType.c_str(), position, avgCost);
			//printf("strike %g - localSymbol: %s, multiplier: %s, secType: %s, right: %s, last date: %s\n", contract.strike, contract.localSymbol.c_str(), contract.multiplier.c_str(), contract.secType.c_str(), contract.right.c_str(), contract.lastTradeDateOrContractMonth.c_str());
			//printf("secId: %s, secIdType: %s, tradingClass: %s\n", contract.secId.c_str(), contract.secIdType.c_str(), contract.tradingClass.c_str());
		}
#ifndef NO_OUTPUT
		printf("wang %s holding Stock %s, Position: %g, Avg Cost: %g\n", account.c_str(), s.symbol.c_str(), position, avgCost);
#endif
	}
	else {
		if (contract.secType == "STK") {
		}

#ifndef NO_OUTPUT
		printf("Position. %s - Symbol: %s, SecType: %s, Currency: %s, Position: %g, Avg Cost: %g\n", account.c_str(), contract.symbol.c_str(), contract.secType.c_str(), contract.currency.c_str(), position, avgCost);
#endif
	}
}
//! [position]

//! [positionend]
void TestCppClient::positionEnd() {
#ifndef NO_OUTPUT
	printf( "PositionEnd\n");
#endif
	int i;
	for (i = 0; i < m_stocknum; i++) {
		if (m_stock[i].shared) write_IB_position(m_stock[i].symbol, m_stock[i].position[0] + m_stock[i].position[1]);
	}

	m_positionRequestStatus = 0;
}
//! [positionend]

//! [accountsummary]
void TestCppClient::accountSummary( int reqId, const std::string& account, const std::string& tag, const std::string& value, const std::string& currency) {
	int i = 0;
	if (account == ACCOUNT2) i = 1;
	

	if (tag == "NetLiquidation") m_account[i].netLiquidation = atof(value.c_str());
	if (tag == "AvailableFunds") m_account[i].availableFunds = atof(value.c_str());
	if (tag == "ExcessLiquidity") m_account[i].excessLiquidity = atof(value.c_str());
	//printf( "Acct Summary. ReqId: %d, Account: %s, Tag: %s, Value: %s, Currency: %s\n", reqId, account.c_str(), tag.c_str(), value.c_str(), currency.c_str());
}
//! [accountsummary]

//! [accountsummaryend]
void TestCppClient::accountSummaryEnd( int reqId) {
	printf( "AccountSummaryEnd. Req Id: %d\n", reqId);
}
//! [accountsummaryend]

void TestCppClient::verifyMessageAPI( const std::string& apiData) {
	printf("verifyMessageAPI: %s\b", apiData.c_str());
}

void TestCppClient::verifyCompleted( bool isSuccessful, const std::string& errorText) {
	printf("verifyCompleted. IsSuccessfule: %d - Error: %s\n", isSuccessful, errorText.c_str());
}

void TestCppClient::verifyAndAuthMessageAPI( const std::string& apiDatai, const std::string& xyzChallenge) {
	printf("verifyAndAuthMessageAPI: %s %s\n", apiDatai.c_str(), xyzChallenge.c_str());
}

void TestCppClient::verifyAndAuthCompleted( bool isSuccessful, const std::string& errorText) {
	printf("verifyAndAuthCompleted. IsSuccessful: %d - Error: %s\n", isSuccessful, errorText.c_str());
    if (isSuccessful)
        m_pClient->startApi();
}

//! [displaygrouplist]
void TestCppClient::displayGroupList( int reqId, const std::string& groups) {
	printf("Display Group List. ReqId: %d, Groups: %s\n", reqId, groups.c_str());
}
//! [displaygrouplist]

//! [displaygroupupdated]
void TestCppClient::displayGroupUpdated( int reqId, const std::string& contractInfo) {
	std::cout << "Display Group Updated. ReqId: " << reqId << ", Contract Info: " << contractInfo << std::endl;
}
//! [displaygroupupdated]

//! [positionmulti]
void TestCppClient::positionMulti( int reqId, const std::string& account,const std::string& modelCode, const Contract& contract, double pos, double avgCost) {
	printf("Position Multi. Request: %d, Account: %s, ModelCode: %s, Symbol: %s, SecType: %s, Currency: %s, Position: %g, Avg Cost: %g\n", reqId, account.c_str(), modelCode.c_str(), contract.symbol.c_str(), contract.secType.c_str(), contract.currency.c_str(), pos, avgCost);
}
//! [positionmulti]

//! [positionmultiend]
void TestCppClient::positionMultiEnd( int reqId) {
	printf("Position Multi End. Request: %d\n", reqId);
}
//! [positionmultiend]

//! [accountupdatemulti]
void TestCppClient::accountUpdateMulti( int reqId, const std::string& account, const std::string& modelCode, const std::string& key, const std::string& value, const std::string& currency) {
	printf("AccountUpdate Multi. Request: %d, Account: %s, ModelCode: %s, Key, %s, Value: %s, Currency: %s\n", reqId, account.c_str(), modelCode.c_str(), key.c_str(), value.c_str(), currency.c_str());
}
//! [accountupdatemulti]

//! [accountupdatemultiend]
void TestCppClient::accountUpdateMultiEnd( int reqId) {
	printf("Account Update Multi End. Request: %d\n", reqId);
}
//! [accountupdatemultiend]

//! [securityDefinitionOptionParameter]
void TestCppClient::securityDefinitionOptionalParameter(int reqId, const std::string& exchange, int underlyingConId, const std::string& tradingClass,
                                                        const std::string& multiplier, const std::set<std::string>& expirations, const std::set<double>& strikes) {
	printf("Security Definition Optional Parameter. Request: %d, Trading Class: %s, Multiplier: %s\n", reqId, tradingClass.c_str(), multiplier.c_str());
}
//! [securityDefinitionOptionParameter]

//! [securityDefinitionOptionParameterEnd]
void TestCppClient::securityDefinitionOptionalParameterEnd(int reqId) {
	printf("Security Definition Optional Parameter End. Request: %d\n", reqId);
}
//! [securityDefinitionOptionParameterEnd]

//! [softDollarTiers]
void TestCppClient::softDollarTiers(int reqId, const std::vector<SoftDollarTier> &tiers) {
	printf("Soft dollar tiers (%lu):", tiers.size());

	for (unsigned int i = 0; i < tiers.size(); i++) {
		printf("%s\n", tiers[i].displayName().c_str());
	}
}
//! [softDollarTiers]

//! [familyCodes]
void TestCppClient::familyCodes(const std::vector<FamilyCode> &familyCodes) {
	printf("Family codes (%lu):\n", familyCodes.size());

	for (unsigned int i = 0; i < familyCodes.size(); i++) {
		printf("Family code [%d] - accountID: %s familyCodeStr: %s\n", i, familyCodes[i].accountID.c_str(), familyCodes[i].familyCodeStr.c_str());
	}
}
//! [familyCodes]

//! [symbolSamples]
void TestCppClient::symbolSamples(int reqId, const std::vector<ContractDescription> &contractDescriptions) {
	printf("Symbol Samples (total=%lu) reqId: %d\n", contractDescriptions.size(), reqId);

	for (unsigned int i = 0; i < contractDescriptions.size(); i++) {
		Contract contract = contractDescriptions[i].contract;
		std::vector<std::string> derivativeSecTypes = contractDescriptions[i].derivativeSecTypes;
		printf("Contract (%u): %ld %s %s %s %s, ", i, contract.conId, contract.symbol.c_str(), contract.secType.c_str(), contract.primaryExchange.c_str(), contract.currency.c_str());
		printf("Derivative Sec-types (%lu):", derivativeSecTypes.size());
		for (unsigned int j = 0; j < derivativeSecTypes.size(); j++) {
			printf(" %s", derivativeSecTypes[j].c_str());
		}
		printf("\n");
	}
}
//! [symbolSamples]

//! [mktDepthExchanges]
void TestCppClient::mktDepthExchanges(const std::vector<DepthMktDataDescription> &depthMktDataDescriptions) {
	printf("Mkt Depth Exchanges (%lu):\n", depthMktDataDescriptions.size());

	for (unsigned int i = 0; i < depthMktDataDescriptions.size(); i++) {
		printf("Depth Mkt Data Description [%d] - exchange: %s secType: %s listingExch: %s serviceDataType: %s aggGroup: %s\n", i, 
			depthMktDataDescriptions[i].exchange.c_str(), 
			depthMktDataDescriptions[i].secType.c_str(), 
			depthMktDataDescriptions[i].listingExch.c_str(), 
			depthMktDataDescriptions[i].serviceDataType.c_str(), 
			depthMktDataDescriptions[i].aggGroup != INT_MAX ? std::to_string(depthMktDataDescriptions[i].aggGroup).c_str() : "");
	}
}
//! [mktDepthExchanges]

//! [tickNews]
void TestCppClient::tickNews(int tickerId, time_t timeStamp, const std::string& providerCode, const std::string& articleId, const std::string& headline, const std::string& extraData) {
	printf("News Tick. TickerId: %d, TimeStamp: %s, ProviderCode: %s, ArticleId: %s, Headline: %s, ExtraData: %s\n", tickerId, ctime(&(timeStamp /= 1000)), providerCode.c_str(), articleId.c_str(), headline.c_str(), extraData.c_str());
}
//! [tickNews]

//! [smartcomponents]]
void TestCppClient::smartComponents(int reqId, const SmartComponentsMap& theMap) {
	printf("Smart components: (%lu):\n", theMap.size());

	for (SmartComponentsMap::const_iterator i = theMap.begin(); i != theMap.end(); i++) {
		printf(" bit number: %d exchange: %s exchange letter: %c\n", i->first, std::get<0>(i->second).c_str(), std::get<1>(i->second));
	}
}
//! [smartcomponents]

//! [tickReqParams]
void TestCppClient::tickReqParams(int tickerId, double minTick, const std::string& bboExchange, int snapshotPermissions) {
	printf("tickerId: %d, minTick: %g, bboExchange: %s, snapshotPermissions: %u\n", tickerId, minTick, bboExchange.c_str(), snapshotPermissions);

	m_bboExchange = bboExchange;
}
//! [tickReqParams]

//! [newsProviders]
void TestCppClient::newsProviders(const std::vector<NewsProvider> &newsProviders) {
	printf("News providers (%lu):\n", newsProviders.size());

	for (unsigned int i = 0; i < newsProviders.size(); i++) {
		printf("News provider [%d] - providerCode: %s providerName: %s\n", i, newsProviders[i].providerCode.c_str(), newsProviders[i].providerName.c_str());
	}
}
//! [newsProviders]

//! [newsArticle]
void TestCppClient::newsArticle(int requestId, int articleType, const std::string& articleText) {
	printf("News Article. Request Id: %d, Article Type: %d\n", requestId, articleType);
	if (articleType == 0) {
		printf("News Article Text (text or html): %s\n", articleText.c_str());
	} else if (articleType == 1) {
		std::string path;
		#if defined(IB_WIN32)
			TCHAR s[200];
			GetCurrentDirectory(200, s);
			path = s + std::string("\\MST$06f53098.pdf");
		#elif defined(IB_POSIX)
			char s[1024];
			if (getcwd(s, sizeof(s)) == NULL) {
				printf("getcwd() error\n");
				return;
			}
			path = s + std::string("/MST$06f53098.pdf");
		#endif
		std::vector<std::uint8_t> bytes = Utils::base64_decode(articleText);
		std::ofstream outfile(path, std::ios::out | std::ios::binary); 
		outfile.write((const char*)bytes.data(), bytes.size());
		printf("Binary/pdf article was saved to: %s\n", path.c_str());
	}
}
//! [newsArticle]

//! [historicalNews]
void TestCppClient::historicalNews(int requestId, const std::string& time, const std::string& providerCode, const std::string& articleId, const std::string& headline) {
	printf("Historical News. RequestId: %d, Time: %s, ProviderCode: %s, ArticleId: %s, Headline: %s\n", requestId, time.c_str(), providerCode.c_str(), articleId.c_str(), headline.c_str());
}
//! [historicalNews]

//! [historicalNewsEnd]
void TestCppClient::historicalNewsEnd(int requestId, bool hasMore) {
	printf("Historical News End. RequestId: %d, HasMore: %s\n", requestId, (hasMore ? "true" : " false"));
}
//! [historicalNewsEnd]

//! [headTimestamp]
void TestCppClient::headTimestamp(int reqId, const std::string& headTimestamp) {
	printf( "Head time stamp. ReqId: %d - Head time stamp: %s,\n", reqId, headTimestamp.c_str());

}
//! [headTimestamp]
 
//! [histogramData]
void TestCppClient::histogramData(int reqId, const HistogramDataVector& data) {
	printf("Histogram. ReqId: %d, data length: %lu\n", reqId, data.size());

	for (auto item : data) {
		printf("\t price: %f, size: %lld\n", item.price, item.size);
	}
}
//! [histogramData]

//! [historicalDataUpdate]
void TestCppClient::historicalDataUpdate(TickerId reqId, const Bar& bar) {
	printf( "HistoricalDataUpdate. ReqId: %ld - Date: %s, Open: %g, High: %g, Low: %g, Close: %g, Volume: %lld, Count: %d, WAP: %g\n", reqId, bar.time.c_str(), bar.open, bar.high, bar.low, bar.close, bar.volume, bar.count, bar.wap);
}
//! [historicalDataUpdate]

//! [rerouteMktDataReq]
void TestCppClient::rerouteMktDataReq(int reqId, int conid, const std::string& exchange) {
	printf( "Re-route market data request. ReqId: %d, ConId: %d, Exchange: %s\n", reqId, conid, exchange.c_str());
}
//! [rerouteMktDataReq]

//! [rerouteMktDepthReq]
void TestCppClient::rerouteMktDepthReq(int reqId, int conid, const std::string& exchange) {
	printf( "Re-route market depth request. ReqId: %d, ConId: %d, Exchange: %s\n", reqId, conid, exchange.c_str());
}
//! [rerouteMktDepthReq]

//! [marketRule]
void TestCppClient::marketRule(int marketRuleId, const std::vector<PriceIncrement> &priceIncrements) {
	printf("Market Rule Id: %d\n", marketRuleId);
	for (unsigned int i = 0; i < priceIncrements.size(); i++) {
		printf("Low Edge: %g, Increment: %g\n", priceIncrements[i].lowEdge, priceIncrements[i].increment);
	}
}
//! [marketRule]

//! [pnl]
void TestCppClient::pnl(int reqId, double dailyPnL, double unrealizedPnL, double realizedPnL) {
	printf("PnL. ReqId: %d, daily PnL: %g, unrealized PnL: %g, realized PnL: %g\n", reqId, dailyPnL, unrealizedPnL, realizedPnL);
}
//! [pnl]

//! [pnlsingle]
void TestCppClient::pnlSingle(int reqId, int pos, double dailyPnL, double unrealizedPnL, double realizedPnL, double value) {
	printf("PnL Single. ReqId: %d, pos: %d, daily PnL: %g, unrealized PnL: %g, realized PnL: %g, value: %g\n", reqId, pos, dailyPnL, unrealizedPnL, realizedPnL, value);
}
//! [pnlsingle]

//! [historicalticks]
void TestCppClient::historicalTicks(int reqId, const std::vector<HistoricalTick>& ticks, bool done) {
    for (HistoricalTick tick : ticks) {
	std::time_t t = tick.time;
        std::cout << "Historical tick. ReqId: " << reqId << ", time: " << ctime(&t) << ", price: "<< tick.price << ", size: " << tick.size << std::endl;
    }
}
//! [historicalticks]

//! [historicalticksbidask]
void TestCppClient::historicalTicksBidAsk(int reqId, const std::vector<HistoricalTickBidAsk>& ticks, bool done) {
    for (HistoricalTickBidAsk tick : ticks) {
	std::time_t t = tick.time;
        std::cout << "Historical tick bid/ask. ReqId: " << reqId << ", time: " << ctime(&t) << ", price bid: "<< tick.priceBid <<
            ", price ask: "<< tick.priceAsk << ", size bid: " << tick.sizeBid << ", size ask: " << tick.sizeAsk <<
            ", bidPastLow: " << tick.tickAttribBidAsk.bidPastLow << ", askPastHigh: " << tick.tickAttribBidAsk.askPastHigh << std::endl;
    }
}
//! [historicalticksbidask]

//! [historicaltickslast]
void TestCppClient::historicalTicksLast(int reqId, const std::vector<HistoricalTickLast>& ticks, bool done) {
    for (HistoricalTickLast tick : ticks) {
	std::time_t t = tick.time;
        std::cout << "Historical tick last. ReqId: " << reqId << ", time: " << ctime(&t) << ", price: "<< tick.price <<
            ", size: " << tick.size << ", exchange: " << tick.exchange << ", special conditions: " << tick.specialConditions <<
            ", unreported: " << tick.tickAttribLast.unreported << ", pastLimit: " << tick.tickAttribLast.pastLimit << std::endl;
    }
}
//! [historicaltickslast]

//! [tickbytickalllast]
void TestCppClient::tickByTickAllLast(int reqId, int tickType, time_t time, double price, int size, const TickAttribLast& tickAttribLast, const std::string& exchange, const std::string& specialConditions) {
    printf("Tick-By-Tick. ReqId: %d, TickType: %s, Time: %s, Price: %g, Size: %d, PastLimit: %d, Unreported: %d, Exchange: %s, SpecialConditions:%s\n", 
        reqId, (tickType == 1 ? "Last" : "AllLast"), ctime(&time), price, size, tickAttribLast.pastLimit, tickAttribLast.unreported, exchange.c_str(), specialConditions.c_str());
}
//! [tickbytickalllast]

//! [tickbytickbidask]
void TestCppClient::tickByTickBidAsk(int reqId, time_t time, double bidPrice, double askPrice, int bidSize, int askSize, const TickAttribBidAsk& tickAttribBidAsk) {
    printf("Tick-By-Tick. ReqId: %d, TickType: BidAsk, Time: %s, BidPrice: %g, AskPrice: %g, BidSize: %d, AskSize: %d, BidPastLow: %d, AskPastHigh: %d\n", 
        reqId, ctime(&time), bidPrice, askPrice, bidSize, askSize, tickAttribBidAsk.bidPastLow, tickAttribBidAsk.askPastHigh);
}
//! [tickbytickbidask]

//! [tickbytickmidpoint]
void TestCppClient::tickByTickMidPoint(int reqId, time_t time, double midPoint) {
    printf("Tick-By-Tick. ReqId: %d, TickType: MidPoint, Time: %s, MidPoint: %g\n", reqId, ctime(&time), midPoint);
}
//! [tickbytickmidpoint]

//! [orderbound]
void TestCppClient::orderBound(long long orderId, int apiClientId, int apiOrderId) {
#ifndef NO_OUTPUT
	printf("Order bound. OrderId: %lld, ApiClientId: %d, ApiOrderId: %d\n", orderId, apiClientId, apiOrderId);
#endif
}
//! [orderbound]

//! [completedorder]
void TestCppClient::completedOrder(const Contract& contract, const Order& order, const OrderState& orderState) {
	printf( "CompletedOrder. PermId: %ld, ParentPermId: %lld, Account: %s, Symbol: %s, SecType: %s, Exchange: %s:, Action: %s, OrderType: %s, TotalQty: %g, CashQty: %g, FilledQty: %g, "
		"LmtPrice: %g, AuxPrice: %g, Status: %s, CompletedTime: %s, CompletedStatus: %s\n", 
		order.permId, order.parentPermId == UNSET_LONG ? 0 : order.parentPermId, order.account.c_str(), contract.symbol.c_str(), contract.secType.c_str(), contract.exchange.c_str(), 
		order.action.c_str(), order.orderType.c_str(), order.totalQuantity, order.cashQty == UNSET_DOUBLE ? 0 : order.cashQty, order.filledQuantity, 
		order.lmtPrice, order.auxPrice, orderState.status.c_str(), orderState.completedTime.c_str(), orderState.completedStatus.c_str());
}
//! [completedorder]

//! [completedordersend]
void TestCppClient::completedOrdersEnd() {
	printf( "CompletedOrdersEnd\n");
}
//! [completedordersend]

