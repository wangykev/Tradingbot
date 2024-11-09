#include "SharedMemory.h"

#include "StockProcess.h"



void StockProcess::read_shared_positions()
{
	int index;
	for (index = 0; index < MAX_SYMBOL; index++) {
		if (g_pIBTdPositions[index].sym[0] == '\0') return;

		unsigned i;
		for (i = 0; i < positions.size(); i++) {
			if (positions[i].stock->symbol == g_pIBTdPositions[index].sym) break;
		}
		if (i >= positions.size()) {
			StockAndOrders so;
			so.stock = new WangStock;
			so.stock->symbol = g_pIBTdPositions[index].sym;
			positions.push_back(so);
		}
		WangStock& s = *positions[i].stock;
		SharePosition& p = g_pIBTdPositions[index];
		s.askPrice = p.askPrice;
		s.bidPrice = p.bidPrice;
		s.lastPrice = p.lastPrice;
		s.dayHigh = p.dayHigh;
		s.dayLow = p.dayLow;
		s.closePrice = p.closePrice;
		s.bidSize = p.bidSize;
		s.askSize = p.askSize;
		s.lastSize = p.lastSize;
		s.dayVolume = p.dayVolume;
		s.topPrice = p.topPrice;
		s.bottomPrice = p.bottomPrice;
		s.multiplier = p.multiplier;
		s.previousClose = p.previousClose;
		s.stdVariation = p.stdVariation;
		s.lowBoundDiscount = p.lowBoundDiscount;
		s.offset = p.offset;

		positions[i].IBpos = p.IBPosition;
		positions[i].TDpos = p.TdPosition;
		positions[i].marketValue = (positions[i].IBpos + positions[i].TDpos) * (positions[i].stock->askPrice + positions[i].stock->bidPrice) * 0.5;
	}
}

void StockProcess::read_shared_orders()
{
	int maxIndex = round(g_pShareOrders[0].qty);
	if (maxIndex <= 0) { 
		maxIndex = 1;  g_pShareOrders[0].qty = 1.0; 
		allOrders.clear();
		for (auto p = positions.begin(); p != positions.end(); p++) {
			p->orders.clear();
		}
		processTime = 0;
	}
	int i;
	time_t curtime = time(NULL);

	//add new orders to positions and delete orders from positions
	for (i = 1; i <= maxIndex; i++) {
		if (g_pShareOrders[i].time >= processTime || g_pShareOrders[i].sym[0] == '\0') {  //potential new orders
			bool added = false;
			bool erased = false;
			bool findStock = false;
			for (auto p = positions.begin(); p != positions.end(); p++) {
				if (p->stock->symbol == g_pShareOrders[i].sym) {  //find the stock in positions
					findStock = true;
					unsigned index;
					for (index = 0; index < p->orders.size(); index++) {
						if (p->orders[index] == i) break;
					}
					if (index >= p->orders.size()) {
						added = true;
						p->orders.push_back(i);
					}
				}
				else {
					for (auto c = p->orders.begin(); c != p->orders.end(); c++) {
						if (*c == i) {
							p->orders.erase(c);
							erased = true;
							break;
						}
					}
				}
			}

			if (!findStock && g_pShareOrders[i].sym[0] != '\0') {  //find a new stock, this may happen.
				StockAndOrders so;
				so.stock = new WangStock;
				so.stock->symbol = g_pShareOrders[i].sym;
				so.orders.push_back(i);
				added = true;
				positions.push_back(so);
			}

			if (added && !erased) allOrders.push_back(i);
			else if (!added && erased) {
				for (auto c = allOrders.begin(); c != allOrders.end(); c++) {
					if (*c == i) {
						allOrders.erase(c);
						break;
					}
				}
			}
		}
	}
	processTime = curtime;
}


void StockProcess::process()
{
	read_shared_positions();
	read_shared_orders();
}
