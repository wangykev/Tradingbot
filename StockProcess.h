#pragma once


#include <vector>

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
		previousClose = stdVariation = 0;
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

	double bottomPrice;
	double topPrice;
	double multiplier;
	double offset;
	double previousClose, stdVariation, lowBoundDiscount;
};


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

struct StockAndOrders {
	WangStock * stock;
	std::vector<int> orders;
	double IBpos, TDpos;
	double marketValue;
};

class StockProcess {
public:
	StockProcess() {
		processTime = 0;
		reading_tick_index = 0;
	}
	~StockProcess(){
		clear();
	}
	std::vector<StockAndOrders> positions;
	std::vector<int> allOrders;
	void process();
private:
	void read_shared_positions();
	void read_shared_orders();
	void clear() {
		allOrders.clear();
		for (auto p = positions.begin(); p != positions.end(); p++) {
			p->orders.clear();
			delete p->stock;
		}
		positions.clear();
		processTime = 0;
		reading_tick_index = 0;
	}
	int reading_tick_index;
	time_t processTime;
};