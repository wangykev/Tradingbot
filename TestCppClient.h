/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#pragma once
#ifndef TWS_API_SAMPLES_TESTCPPCLIENT_TESTCPPCLIENT_H
#define TWS_API_SAMPLES_TESTCPPCLIENT_TESTCPPCLIENT_H

#include "EWrapper.h"
#include "EReaderOSSignal.h"
#include "EReader.h"
#include "Order.h"

#include <memory>
#include <vector>
#include <time.h>
#include "Action.h"
#include "Option.h"

class EClientSocket;

double round(double a, double precision);

enum State {
	ST_CONNECT,
	ST_TICKDATAOPERATION,
	ST_TICKDATAOPERATION_ACK,
	ST_TICKOPTIONCOMPUTATIONOPERATION,
	ST_TICKOPTIONCOMPUTATIONOPERATION_ACK,
	ST_DELAYEDTICKDATAOPERATION,
	ST_DELAYEDTICKDATAOPERATION_ACK,
	ST_MARKETDEPTHOPERATION,
	ST_MARKETDEPTHOPERATION_ACK,
	ST_REALTIMEBARS,
	ST_REALTIMEBARS_ACK,
	ST_MARKETDATATYPE,
	ST_MARKETDATATYPE_ACK,
	ST_HISTORICALDATAREQUESTS,
	ST_HISTORICALDATAREQUESTS_ACK,
	ST_OPTIONSOPERATIONS,
	ST_OPTIONSOPERATIONS_ACK,
	ST_CONTRACTOPERATION,
	ST_CONTRACTOPERATION_ACK,
	ST_MARKETSCANNERS,
	ST_MARKETSCANNERS_ACK,
	ST_FUNDAMENTALS,
	ST_FUNDAMENTALS_ACK,
	ST_BULLETINS,
	ST_BULLETINS_ACK,
	ST_ACCOUNTOPERATIONS,
	ST_ACCOUNTOPERATIONS_ACK,
	ST_ORDEROPERATIONS,
	ST_ORDEROPERATIONS_ACK,
	ST_OCASAMPLES,
	ST_OCASAMPLES_ACK,
	ST_CONDITIONSAMPLES,
	ST_CONDITIONSAMPLES_ACK,
	ST_BRACKETSAMPLES,
	ST_BRACKETSAMPLES_ACK,
	ST_HEDGESAMPLES,
	ST_HEDGESAMPLES_ACK,
	ST_TESTALGOSAMPLES,
	ST_TESTALGOSAMPLES_ACK,
	ST_FAORDERSAMPLES,
	ST_FAORDERSAMPLES_ACK,
	ST_FAOPERATIONS,
	ST_FAOPERATIONS_ACK,
	ST_DISPLAYGROUPS,
	ST_DISPLAYGROUPS_ACK,
	ST_MISCELANEOUS,
	ST_MISCELANEOUS_ACK,
	ST_CANCELORDER,
	ST_CANCELORDER_ACK,
	ST_FAMILYCODES,
	ST_FAMILYCODES_ACK,
	ST_SYMBOLSAMPLES,
	ST_SYMBOLSAMPLES_ACK,
	ST_REQMKTDEPTHEXCHANGES,
	ST_REQMKTDEPTHEXCHANGES_ACK,
	ST_REQNEWSTICKS,
	ST_REQNEWSTICKS_ACK,
	ST_REQSMARTCOMPONENTS,
	ST_REQSMARTCOMPONENTS_ACK,
	ST_NEWSPROVIDERS,
	ST_NEWSPROVIDERS_ACK,
	ST_REQNEWSARTICLE,
	ST_REQNEWSARTICLE_ACK,
	ST_REQHISTORICALNEWS,
	ST_REQHISTORICALNEWS_ACK,
	ST_REQHEADTIMESTAMP,
	ST_REQHEADTIMESTAMP_ACK,
	ST_REQHISTOGRAMDATA,
	ST_REQHISTOGRAMDATA_ACK,
	ST_REROUTECFD,
	ST_REROUTECFD_ACK,
	ST_MARKETRULE,
	ST_MARKETRULE_ACK,
    ST_PNL,
    ST_PNL_ACK,
    ST_PNLSINGLE,
    ST_PNLSINGLE_ACK,
    ST_CONTFUT,
    ST_CONTFUT_ACK,
	ST_PING,
	ST_PING_ACK,
    ST_REQHISTORICALTICKS,
    ST_REQHISTORICALTICKS_ACK,
    ST_REQTICKBYTICKDATA,
    ST_REQTICKBYTICKDATA_ACK,
	ST_WHATIFSAMPLES,
	ST_WHATIFSAMPLES_ACK,
	ST_IDLE
};

class MyOrder {
public:
	MyOrder() {
		orderId = 0;
		action = "";
		orderType = "";
		status = "";
		symbol = "";
		currency = "";
		exchange = "";
		account = "";
		totalQty = filledQty = remaining = lmtPrice = avgFillPrice = 0.0;
		fillTime = 0;
		tryPrice = trySize = 0.0;
		tryTime = 0;
		commission = 0.0;
		profit = 0.0;
		partiallyFilled = false;
	}
	int orderId;
	std::string symbol;
	std::string action;
	std::string orderType;
	std::string account;
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
	time_t fillTime;
	double tryPrice;
	double trySize;
	time_t tryTime;
	bool partiallyFilled;
};

class ExecTime {
public:
	std::string action;
	time_t fillTime;
	double qty;
};

class ExecPair {
public:
	std::string execId;
	int orderId;
	int stockIndex;
};

struct Account {
	double netLiquidation;
	double availableFunds;
	double excessLiquidity;
};

#define NACCOUNTS 2



class Stock {
public:
	Stock()
	{
		symbol = "";
		primaryExchange = "";
		currency = "USD";
//		account = "";
		bidPrice = askPrice = lastPrice = dayHigh = dayLow = closePrice = dayHigh = dayLow = 0.0;
		for (int i = 0; i < NACCOUNTS; i++) {
			position[i] = avgCost[i] = 0.0;
		}
		askSize = bidSize = lastSize = dayVolume = 0;
		bottomPrice = topPrice = multiplier = offset = 0;
		targetBottomPrice = targetTopPrice = 0;
		oldBottomPrice = oldTopPrice = oldMultiplier = oldOffset = 0;
		targetMultiplier = -1; targetOffset = 1000000001;
		previousClose = stdVariation = 0;
		lowBoundDiscount = 0.05;
		startTime = 9 * 60 + 30; //9:30
		endTime = 13 * 60 + 30; //13:30
		buyBelow = 10000.0;
		sellAbove = 0;
		buyAbove = 0;
		sellBelow = 10000.0;
		longTermHolding = 0;
		takenProfit = 10.0;
		hedgeRatio = 0.0;
		totalProfit = totalCommission = 0;
		a1 = a2 = 0; comparison = false;
		canSell = canBuy = false;    
		canShort = false;   //canShort means that when long term account holding some position the short term account can short it. 
		shared = false;  //shared between IB and Td postions.
		reverse = false;
		alpacaTrading = true;
		maxAlpacaMarketValue = 10000.0;
		impliedVolatility = 0.5;
		fatTailBuyEnbled = fatTailShortEnbled = fatTailBuyDone = fatTailShortDone = false;
		periods.clear();
	}
	std::string symbol;
	std::string primaryExchange;
	std::string currency;
//	std::string account;
	int tikerID;
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

	double position[NACCOUNTS]; //two accounts, two positions.
	double avgCost[NACCOUNTS]; //two 

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
	double a1, a2;
	bool comparison;
	bool canSell, canBuy, canShort, shared;
	bool reverse;
	bool alpacaTrading;
	double maxAlpacaMarketValue;
	double longTermHolding;
	double startTime, endTime;
	double impliedVolatility;

	double takenProfit;   //the amount to take propit

	std::deque<class Action> actions;

	OptionPositions opPosition[NACCOUNTS];
	/*
	void updateProfit(double size) {
	}

	void updateCommission(double commission) {
		totalCommission += commission;
	}

	double getExpPrice(double totalPosition) {  //get expected current price
		double price = topPrice / exp((totalPosition - offset - 0.5) / (100 * multiplier) * log(topPrice / bottomPrice));
	}

	double getSpreadPercent(double totalPosition) {
		double price = getExpPrice(totalPosition);
		return sqrt(log(topPrice / bottomPrice) * takenProfit / (multiplier * 100.0 * price));  //the minimum spread to get the profit. 
	}

	double getExpAskPriceSize(double totalPosition, double & askPrice, double & askSize) {
		double price = getExpPrice(totalPosition);
		double spreadPercent = getSpreadPercent(totalPosition);  //the minimum spread to get the profit. 
		return ceil(price * (1 + spreadPercent)*100)/100.0;
	}

	double getExpBidPrice(double totalPosition) {
		double price = getExpPrice(totalPosition);
		double spreadPercent = getSpreadPercent(totalPosition);  //the minimum spread to get the profit. 
		return floor(price * (1 - spreadPercent) * 100) / 100.0;
	}

	double getExpBidSize(double totalPosition) {

	}
	*/

	//	int ordernumber;
//	int orderEventNum;
//	MyOrder order[10];  //maximum 10 orders
};


//! [ewrapperimpl]
class TestCppClient : public EWrapper
{
//! [ewrapperimpl]
public:

	TestCppClient();
	~TestCppClient();

	void setConnectOptions(const std::string&);
	void processMessages();
	void wangSleep(int nMillisecs);
	void wangJob();
	void wangRequest();
	void wangRequestMarketData();
	void ProfitTakenAdjust();
	void wangUpdateOrdersAndPositions();
	void wangPrintOrders();
	void modifyLmtOrder(int orderId, double newSize, double newPrice);
	void wangProcessControlFile(bool requestMarketData = true);
	void wangUpdateConrolFile();
	void wangExportControlFile(bool copyAlpaca = false);
	void wangShareOrderInfo(MyOrder* o);

	Stock  m_stock[100];
	int m_stocknum;
	int m_openOrderRequestStatus;
	int m_positionRequestStatus;
	int m_orderUpdated;
	bool m_canBuy, m_canSell;
	bool m_alpacaAllTrading;
	std::vector<MyOrder *> m_orderList;
	std::vector<ExecPair> m_execPairs;
	Account m_account[NACCOUNTS];
	FILETIME m_fileTime;


public:

	bool connect(const char * host, int port, int clientId = 0);
	void disconnect() const;
	bool isConnected() const;

private:
    void pnlOperation();
    void pnlSingleOperation();
	void tickDataOperation();
	void tickOptionComputationOperation();
	void delayedTickDataOperation();
	void marketDepthOperations();
	void realTimeBars();
	void marketDataType();
	void historicalDataRequests();
	void optionsOperations();
	void accountOperations();
	void orderOperations();
	void ocaSamples();
	void conditionSamples();
	void bracketSample();
	void hedgeSample();
	void contractOperations();
	void marketScanners();
	void fundamentals();
	void bulletins();
	void testAlgoSamples();
	void financialAdvisorOrderSamples();
	void financialAdvisorOperations();
	void testDisplayGroups();
	void miscelaneous();
	void reqFamilyCodes();
	void reqMatchingSymbols();
	void reqMktDepthExchanges();
	void reqNewsTicks();
	void reqSmartComponents();
	void reqNewsProviders();
	void reqNewsArticle();
	void reqHistoricalNews();
	void reqHeadTimestamp();
	void reqHistogramData();
	void rerouteCFDOperations();
	void marketRuleOperations();
	void continuousFuturesOperations();
    void reqHistoricalTicks();
    void reqTickByTickData();
	void whatIfSamples();

	void reqCurrentTime();

public:
	// events
	#include "EWrapper_prototypes.h"


private:
	void printContractMsg(const Contract& contract);
	void printContractDetailsMsg(const ContractDetails& contractDetails);
	void printContractDetailsSecIdList(const TagValueListSPtr &secIdList);
	void printBondContractDetailsMsg(const ContractDetails& contractDetails);

private:
	//! [socket_declare]
	EReaderOSSignal m_osSignal;
	EClientSocket * const m_pClient;
	//! [socket_declare]
	State m_state;
	time_t m_sleepDeadline;

	OrderId m_orderId;
	EReader *m_pReader;
    bool m_extraAuth;
	std::string m_bboExchange;
};

#endif

