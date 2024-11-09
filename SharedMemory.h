#pragma once

#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H
#include <memory>
#include <vector>
#include <time.h>
#include <mutex>


#define MAX_SYMBOL 100

struct SymbolList {
	int size;
	char sym[MAX_SYMBOL][10];
};


#define MAX_TICK_MSG 1000000

struct TickMsg {
	bool set;
	char sym[10];
	int type;
	double value;
};

struct SharePosition {
	char sym[10];
	bool IBset;
	bool Tdset;
	double IBPosition;
	double TdPosition;

	double bidPrice;
	double askPrice;
	int askSize;
	int bidSize;
	double lastPrice;
	int lastSize;
	double dayHigh;
	double dayLow;
	int dayVolume;  //multiplier 100
	double closePrice;  //previous day close price

	double bottomPrice;
	double topPrice;
	double multiplier;
	double offset;
	double previousClose, stdVariation, lowBoundDiscount;
};

#define MAX_ORDER 10000

struct ShareOrder {
	char sym[10];
	char action[10];
	char status[20];
	double qty;
	double filled_qty;
	double price;
	double profit;
	double commission;
	time_t time;
	char id[50];
	char broker[20];
};

extern SymbolList* g_pSymboList;
extern TickMsg* g_pTickMsg;
extern ShareOrder* g_pShareOrders;
extern int writing_tick_index;
extern SharePosition* g_pIBTdPositions;

extern bool * g_pActive;

bool shared_initialize();
void shared_deinitialize();
void clearSharedTickAndOrders();

void add_shared_symbol(std::string & s);

void add_shared_tick(std::string& sym, int type, double value);
void update_stock_range(std::string& sym, double top, double bottom, double multiplier, double offset, double previousClose, double stdVariation, double lowBoundDiscount);

void write_IB_position(std::string &sym, double position);
void write_Td_position(std::string &sym, double position);
bool read_IB_position(std::string &sym, double &position);
bool read_Td_position(std::string &sym, double &position);
void outputBuffer(const char * filename);


void updateShareOrder(std::string& id, std::string& sym, std::string action, std::string status, double qty, double filled_qty, double price, double profit, double commission, std::string broker);


#endif
