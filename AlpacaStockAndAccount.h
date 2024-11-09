#pragma once

#ifndef ALPACASTOCKANDACCOUNT_H
#define ALPACASTOCKANDACCOUNT_H

#include "Order.h"
#include <memory>
#include <vector>
#include <time.h>
#include <mutex>
#include <thread>
#include <chrono>
#include "tdma_common.h"
#include "tdma_api_get.h"
#include "tdma_api_execute.h"
#include "tdma_api_streaming.h"
#include "Alpaca.h"
#include "AlpacaTradeApi.h"
#include "AlpacaStream.h"
#include "Action.h"
#include "Delta.h"


class AlpacaOrder {
public:
	AlpacaOrder() {
		orderId = "";
		action = "";
		orderType = "LMT";
		status = "";
		symbol = "";
		currency = "";
		exchange = "";
		account = "";
		replacedto = "";
		totalQty = filledQty = remaining = lmtPrice = avgFillPrice = 0.0;
		profit = 0.0;
		fillTime = pendingCancelTime = 0.0;
		commission = 0.0;
		partiallyFilled = false;
	}
	std::string orderId;
	std::string symbol;
	std::string action;
	std::string orderType;
	std::string account;
	std::string replacedto;
	double totalQty;
	double filledQty;
	double remaining;
	double lmtPrice;
	double avgFillPrice;
	double commission;
	double profit;
	std::string status;
	std::string currency;
	std::string exchange;
	double fillTime, pendingCancelTime;
//	double tryPrice;
//	double trySize;
//	time_t tryTime;
	bool partiallyFilled;
};

class AlpacaPosition {
public:
	AlpacaPosition() {
		symbol = "";
		currency = "USD";
		quanty = 0;
		init();
		takenProfit = 5.0;
		hedgeRatio = 0.0;
		totalProfit = totalCommission = totalShares = totalCash = 0.0;
		canSell = canBuy = false;
		shared = false; //shared between IB and Td positions.
		reverse = false;
		alpacaTrading = true; //for this stock only, Alpaca is trading.
		stockIndex = 99;
		newOrderSettleTime = 0;
		newOrderSettled = false;
		activeBuyOrder = activeSellOrder = NULL;
		maxAlpacaMarketValue = 10000.0;
	}

	void init()
	{
		canSell = canBuy = false;
		bottomPrice = topPrice = multiplier = offset = 0;
		targetBottomPrice = targetTopPrice = 0;
		oldBottomPrice = oldTopPrice = oldMultiplier = oldOffset = 0;
		previousClose = stdVariation = 0;
		lowBoundDiscount = 0.05;
		targetMultiplier = -1; targetOffset = 1000000001;
		longTermHolding = 0.0;
		startTime = 9 * 60 + 30; //9:30
		endTime = 13 * 60 + 30; //13:30
		buyBelow = 10000.0;
		sellAbove = 0;
		buyAbove = 0;
		sellBelow = 10000.0;
		canShort = false;   //canShort means that when long term account holding some position the short term account can short it. 
		reverse = false;
		fatTailBuyEnbled = fatTailShortEnbled = fatTailBuyDone = fatTailShortDone = false;
		periods.clear();
	}

	std::string symbol;
	std::string currency;
	std::string type;
	//	std::string account;
	int stockIndex;
	double quanty;
	double& lastPrice() {
		if (stockIndex < 0) return allStock[99].lastPrice;
		else return allStock[stockIndex].lastPrice;
	}
	double& askPrice() {
		if (stockIndex < 0) return allStock[99].askPrice;
		else return allStock[stockIndex].askPrice;
	}
	double & bidPrice() {
		if (stockIndex < 0) return allStock[99].bidPrice;
		else return allStock[stockIndex].bidPrice;
	}
	double askSize() {
		if (stockIndex < 0) return 0;
		else return allStock[stockIndex].askSize;
	}
	double bidSize() {
		if (stockIndex < 0) return 0;
		else return allStock[stockIndex].bidSize;
	}


	double bottomPrice, targetBottomPrice, oldBottomPrice;
	double topPrice, targetTopPrice, oldTopPrice;
	double multiplier, targetMultiplier, oldMultiplier;
	std::vector<double> periods;
	double previousClose, stdVariation, lowBoundDiscount;
	bool fatTailBuyEnbled, fatTailShortEnbled;
	bool fatTailBuyDone, fatTailShortDone;
	double spreadPercent;
	double offset, targetOffset, oldOffset;
	double buyBelow, buyAbove;
	double sellBelow, sellAbove;
	double hedgeRatio;
	double totalProfit;
	double totalCommission;
	double totalShares, totalCash;
	bool canSell, canBuy, canShort, shared;
	bool reverse;
	bool alpacaTrading;
	double maxAlpacaMarketValue;
	double longTermHolding;
	double startTime, endTime;
	AlpacaOrder * activeBuyOrder, * activeSellOrder;
	time_t newOrderSettleTime;
	bool newOrderSettled;

	double takenProfit;   //the amount to take propit

	std::deque<class Action> actions;

};

void AlpacaRealTimeStreamCallback(const web::websockets::client::websocket_incoming_message& msg);

#define INDIVIDUALACCCOLOR 10

class AlpacaAccount {
public:
	friend void AlpacaPushMessage(StreamMessage& msg);
	AlpacaAccount(const char * endpt, const char* keyid, const char* secukey, const char* cf, int c):endPoint(endpt), keyID(keyid), secretKey(secukey), control_file(cf)
	{
		cprintf("Alpaca Account\n");
		switch (c) {
		case 0: color = INDIVIDUALACCCOLOR; break;
		case 1: color = 11; break;
		case 2: color = 13; break;
		case 3: color = 14; break;
		case 4: color = 9; break;
		}
		InitializeCriticalSection(&critSec);
		
		tradeApi = NULL;
		streamSession = NULL;

		reading_tick_index = 0;
		msgTimeSecs = 0;
		lastPositionChangeTime = 0;
		bActive = false;
		pingpang = -1;
	}
	~AlpacaAccount()
	{
		for (AlpacaPosition* p : positions) delete p;
		for (AlpacaOrder* p : orders) delete p;
		if (tradeApi) delete tradeApi;
		if (streamSession) delete streamSession;
	}
	std::string brokername;
	std::string endPoint;
	std::string keyID;
	std::string secretKey;
	std::string control_file;
	alpaca::Tradeapi *tradeApi;
	std::deque<StreamMessage> messageQueue;
	int color;
	AlpacaPosition& getPosition(std::string sym);
	time_t lastPositionChangeTime;

	void ProcessControlFile();
	void UpdateControlFile();
	void ExportControlFile();
	void ProcessMessages();
	void requestMarketData_nouse();  //do not use
	void updatePositions();
	void updateAccount();
	void exportActivities(int year, int mon, int day);
	void wangJob();
	bool getOrderInfo(AlpacaOrder *p);
	void connect();
	void disconnect();
	bool isConnected() {
		return bActive;
	}
	 

private:
	int cprintf(const char* format, ...);
	int rprintf(const char* format, ...);
	bool scanValue(std::string& line, const char* targetString, double& value1, double& value2);
	bool scanValue(std::string& line, const char* targetString, double& value);
	bool scanValue(std::string& line, const char* targetString, bool& value);
	bool scanValue(std::string& line, const char* targetString, std::string& value);
	bool scanValue(std::string& line, const char* targetString, std::vector<double>& periods);
	void updateOnePosition(std::string sym, double qnt, double cost);
	void addToPosition(std::string sym, std::string action, double qnt, double cost);
	void updateOneOrder(std::string orderid, std::string status, std::string buySell, std::string sym, double price, double qnt, double filled, double remaining, double avgPrice);
	void printPositions();
	void cancelOrder(std::string orderid);
	void cancelAllOrders();
	void placeOrder(std::string sym, std::string action, double price, double qnt);
	void replaceOrder(std::string orderId, std::string sym, std::string action, double price, double qnt);
	void modifyLmtOrder(std::string orderId, double newSize, double newPrice);
	void updateOrderProfit(AlpacaOrder* o, double shares, double cash);
	void zeroActiveOrders(AlpacaOrder * o);
	void shareOrderInfo(AlpacaOrder* o);

	std::vector<AlpacaPosition *> positions;
	std::vector<AlpacaOrder *> orders;

	double netLiquidation;
	double availableFunds;
	double excessLiquidity;

	bool m_canBuy, m_canSell;
	bool m_alpacaAllTrading;
	FILETIME m_fileTime;

	alpaca::Stream * streamSession;
//	std::mutex m_mutex;
	CRITICAL_SECTION critSec;
	double msgTimeSecs;

	void read_shared_tick();
	int reading_tick_index;
	bool bActive;
	int pingpang;
};


#endif
