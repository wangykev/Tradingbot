#pragma once

#ifndef STOCKANDACCOUNT_H
#define STOCKANDACCOUNT_H

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


class WangStock {
public:
	WangStock()
	{
		symbol = "";
		type = "STK";
		primaryExchange = "";
		currency = "USD";
		dataRequested = false;
		//		account = "";
		bidPrice = askPrice = lastPrice = dayHigh = dayLow = closePrice = dayHigh = dayLow = 0.0;
		askSize = bidSize = lastSize = dayVolume = 0;
	}
	std::string symbol;
	std::string type;
	std::string primaryExchange;
	std::string currency;
	//	std::string account;
	int tikerID;
	bool dataRequested;
	double bidPrice;
	double askPrice;
	int askSize;
	int bidSize;
	double lastPrice;
	int lastSize;
	double dayHigh;
	double dayLow;
	double closePrice;
	int dayVolume;  //multiplier 100
};

extern WangStock allStock[];
extern int allStockNum;


class WangOrder {
public:
	WangOrder() {
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
		fillTime = 0.0;
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
	double fillTime;
//	double tryPrice;
//	double trySize;
//	time_t tryTime;
	bool partiallyFilled;
};

class WangPosition {
public:
	WangPosition() {
		symbol = "";
		currency = "USD";
		quanty = 0;
		targetBottomPrice = targetTopPrice = 0;
		bottomPrice = topPrice = multiplier = offset = 0;
		oldBottomPrice = oldTopPrice = oldMultiplier = oldOffset = 0;
		previousClose = stdVariation = 0;
		lowBoundDiscount = 0.05;
		targetMultiplier = -1; targetOffset = 1000000001;
		startTime = 9 * 60 + 30; //9:30
		endTime = 13 * 60 + 30; //13:30
		buyBelow = 10000.0;
		sellAbove = 0;
		buyAbove = 0;
		sellBelow = 10000.0;
		takenProfit = 4.0;
		hedgeRatio = 0.0;
		totalProfit = totalCommission = totalShares = totalCash = 0.0;
		canSell = canBuy = false;
		canShort = false;   //canShort means that when long term account holding some position the short term account can short it. 
		shared = false; //shared between IB and Td positions.
		stockIndex = 99;
		newOrderSettleTime = 0;
		newOrderSettled = false;
		activeBuyOrder = activeSellOrder = NULL;
		maxAlpacaMarketValue = 20000.0;
	}

	std::string symbol;
	std::string currency;
	std::string type;
	//	std::string account;
	int stockIndex;
	double quanty;
	double & askPrice() {
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
	double spreadPercent;
	double offset, targetOffset, oldOffset;
	double maxAlpacaMarketValue;
	double buyBelow, buyAbove;
	double sellBelow, sellAbove;
	double hedgeRatio;
	double totalProfit;
	double totalCommission;
	double totalShares, totalCash;
	bool canSell, canBuy, canShort, shared;
	double startTime, endTime;
	WangOrder * activeBuyOrder, * activeSellOrder;
	time_t newOrderSettleTime;
	bool newOrderSettled;

	double takenProfit;   //the amount to take propit

};

//Only used for update account changes.
struct StreamMessage {
	int type;
	std::string orderid;
	std::string status;
	std::string action;
	std::string sym;
	double price;
	double qnt;
	double canceledQnt;
	double filled;
	double execPrice;
	double remaining;
	double time;
	double position_qty;
	int rejectionReason;
	std::string aid;
};

void RealTimeStreamCallback(int cb_type, int ss_type, unsigned long long timestamp, const char* msg);
int addOneStock(std::string sym, std::string type = "STK");
int getStockIndex(std::string sym, std::string type = "STK");
void read_shared_symbol_to_allStock();
double GetTimeInSeconds();
int rprintf(const char* format, ...);
void PushMessage(StreamMessage& msg);
std::string getXmlString(const char* xml, const char* tag);
double getXmlTime(const char* xml, const char* tag);
double getXmlValue(const char* xml, const char* tag);


#define INDIVIDUALACCCOLOR 10

class WangAccount {
public:
	friend void PushMessage(StreamMessage& msg);
	WangAccount(const char * aid, const char* cred, const char* pwd, const char* cf, int c):account_id(aid), creds_path(cred), password(pwd), control_file(cf)
	{
		cprintf("Account ID: %s\n", account_id.c_str());
		switch (c) {
		case 0: color = INDIVIDUALACCCOLOR; break;
		case 1: color = 11; break;
		case 2: color = 13; break;
		case 3: color = 14; break;
		case 4: color = 9; break;
		}
		InitializeCriticalSection(&critSec);
		pCManager = new tdma::CredentialsManager(creds_path, password);
		streamSession = tdma::StreamingSession::Create(pCManager->credentials, RealTimeStreamCallback);
			//, std::chrono::milliseconds(3000),  //connection timeout
			//std::chrono::milliseconds(15000),     //listening_timeout
			//tdma::StreamingSession::DEF_SUBSCRIBE_TIMEOUT);   //subscribe_timeout

		reading_tick_index = 0;
		msgTimeSecs = 0;
		lastPositionChangeTime = 0;
	}
	~WangAccount()
	{
		for (WangPosition* p : positions) delete p;
		for (WangOrder* p : orders) delete p;
		if (pCManager) delete pCManager;
	}
	std::string brokername;
	std::string account_id;
	std::string creds_path;
	std::string password;
	std::string control_file;
	tdma::CredentialsManager * pCManager;
	std::deque<StreamMessage> messageQueue;
	int color;
	WangPosition& getPosition(std::string sym);
	time_t lastPositionChangeTime;

	void ProcessControlFile();
	void UpdateControlFile();
	void ProcessMessages();
	void requestMarketData_nouse();  //do not use
	void updatePositions();
	void updateAccount();
	void wangJob();
	bool getOrderInfo(WangOrder *p);
	void connect();
	bool isConnected() {
		return streamSession->is_active();
	}
	 

private:
	int cprintf(const char* format, ...);
	bool scanValue(std::string& line, const char* targetString, double& value1, double& value2);
	bool scanValue(std::string& line, const char* targetString, double& value);
	bool scanValue(std::string& line, const char* targetString, bool& value);
	bool scanValue(std::string& line, const char* targetString, std::string& value);
	void updateOnePosition(std::string sym, double qnt, double cost);
	void addToPosition(std::string sym, std::string action, double qnt, double cost);
	void updateOneOrder(std::string orderid, std::string status, std::string buySell, std::string sym, double price, double qnt, double filled, double remaining, double avgPrice);
	void printPositions();
	void cancelOrder(std::string orderid);
	void placeOrder(std::string sym, std::string action, double price, double qnt);
	void replaceOrder(std::string orderId, std::string sym, std::string action, double price, double qnt);
	void modifyLmtOrder(std::string orderId, double newSize, double newPrice);
	void updateOrderProfit(WangOrder* o, double shares, double cash);
	void zeroActiveOrders(WangOrder * o);
	bool updateReplacedToOrders(WangOrder * p, double partiallFill);

	std::vector<WangPosition *> positions;
	std::vector<WangOrder *> orders;

	double netLiquidation;
	double availableFunds;
	double excessLiquidity;
	double buyingPower;

	bool m_canBuy, m_canSell;
	bool m_alpacaTrading;
	FILETIME m_fileTime;

	std::shared_ptr<tdma::StreamingSession> streamSession;
//	std::mutex m_mutex;
	CRITICAL_SECTION critSec;
	double msgTimeSecs;

	void read_shared_tick();
	int reading_tick_index;
};



#endif
