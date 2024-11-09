#include "StockAndAccount.h"
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <ctime>
#include <fstream>
#include <cstdint>
#include "color.h"
#include "tdma_common.h"
#include "tdma_api_get.h"
#include "tdma_api_execute.h"
#include "tdma_api_streaming.h"
#include "SharedMemory.h"
#include "FolderDefinition.h"
#include "AlpacaStockAndAccount.h"
#include "curl_utils.hpp"
#include "time_utils.hpp"

#define OUTPUT_TO_FILE

void AlpacaAccount::read_shared_tick()
{
	bool print = false;
	if (reading_tick_index > 0 && g_pTickMsg[reading_tick_index - 1].set == 0) //reset when the first program erase everything, it will not happen. 
		reading_tick_index = 0;

	while (g_pTickMsg[reading_tick_index].set == 1) {
		int index = -1;
		AlpacaPosition* pPosition = NULL;
		for (unsigned int i = 0; i < positions.size(); i++) {
			pPosition = positions[i];
			if (pPosition->symbol == g_pTickMsg[reading_tick_index].sym) {
				index = pPosition->stockIndex;
				break;
			}
		}

		if (index!= -1 && pPosition) {
			if (print) printf(g_pTickMsg[reading_tick_index].sym);
			WangStock& s = allStock[index];
			double price = g_pTickMsg[reading_tick_index].value;
			int size = round(price);
			switch (g_pTickMsg[reading_tick_index].type) {
			case 1:
				s.bidPrice = price;
				if (price > 0 && pPosition->activeSellOrder && pPosition->activeSellOrder->status == "new" && pPosition->activeSellOrder->lmtPrice < price + 0.001) {  //we guess the order has been filled.
					cprintf("Order %s %s %s is guess filled, because sell order price %g <= bidPrice %g\n", pPosition->activeSellOrder->orderId.c_str(), pPosition->activeSellOrder->action.c_str(), pPosition->activeSellOrder->symbol.c_str(), pPosition->activeSellOrder->lmtPrice, price);
					pPosition->activeSellOrder->status = "GUESS_FILLED";
					pPosition->activeSellOrder->fillTime = GetTimeInSeconds();
				}
				if (print) printf("bidPrice = %g\n", price);
				break;
			case 2:
				s.askPrice = price;
				if (price > 0 && pPosition->activeBuyOrder && pPosition->activeBuyOrder->status == "new" && pPosition->activeBuyOrder->lmtPrice > price - 0.001) {  //we guess the order has been filled.
					cprintf("Order %s %s %s is guess filled, because buy order price %g >= askPrice %g\n", pPosition->activeBuyOrder->orderId.c_str(), pPosition->activeBuyOrder->action.c_str(), pPosition->activeBuyOrder->symbol.c_str(), pPosition->activeBuyOrder->lmtPrice, price);
					pPosition->activeBuyOrder->status = "GUESS_FILLED";
					pPosition->activeBuyOrder->fillTime = GetTimeInSeconds();
				}
				if (print) printf("askPrice = %g\n", price);
				break;
			case 4:
				s.lastPrice = price;
				if (price > 0 && pPosition->activeSellOrder && pPosition->activeSellOrder->status == "new" && pPosition->activeSellOrder->lmtPrice < price + 0.001) {  //we guess the order has been filled.
					cprintf("Order %s %s %s is guess filled, because order price %g <= lastPrice %g\n", pPosition->activeSellOrder->orderId.c_str(), pPosition->activeSellOrder->action.c_str(), pPosition->activeSellOrder->symbol.c_str(), pPosition->activeSellOrder->lmtPrice, price);
					pPosition->activeSellOrder->status = "GUESS_FILLED";
					pPosition->activeSellOrder->fillTime = GetTimeInSeconds();
				}
				else if (price > 0 && pPosition->activeBuyOrder && pPosition->activeBuyOrder->status == "new" && pPosition->activeBuyOrder->lmtPrice > price - 0.001) {  //we guess the order has been filled.
					cprintf("Order %s %s %s is guess filled, because buy order price %g >= lastPrice %g\n", pPosition->activeBuyOrder->orderId.c_str(), pPosition->activeBuyOrder->action.c_str(), pPosition->activeBuyOrder->symbol.c_str(), pPosition->activeBuyOrder->lmtPrice, price);
					pPosition->activeBuyOrder->status = "GUESS_FILLED";
					pPosition->activeBuyOrder->fillTime = GetTimeInSeconds();
				}
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
				if (print) printf("unknown type %d = %g\n", g_pTickMsg[reading_tick_index].type, price);
				break;
			}
		}
		reading_tick_index = (reading_tick_index + 1) % MAX_TICK_MSG;
	}

	for (auto pos : positions) {
		if (pos->activeBuyOrder && pos->activeBuyOrder->status == "GUESS_FILLED") {
			if (!getOrderInfo(pos->activeBuyOrder)) {
				pos->activeBuyOrder->status = "new";
			}
		}
		if (pos->activeSellOrder && pos->activeSellOrder->status == "GUESS_FILLED") {
			if (!getOrderInfo(pos->activeSellOrder)) {
				pos->activeSellOrder->status = "new";
			}
		}
	}
}




int AlpacaAccount::cprintf(const char* format, ...)
{
	//set color
	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, color);
	//print time tag
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
#ifdef OUTPUT_TO_FILE
	FILE* f = fopen(ALPACA_OUTPUT_FOLDER"output.txt", "a");
#else
	FILE* f = NULL;
#endif
	printf("%02d:%02d:%02d ", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);
	if (f) fprintf(f, "%02d%02d-%02d:%02d:%02d ", tm_struct->tm_mon, tm_struct->tm_mday, tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);

	int result;
	va_list args;

	va_start(args, format);
	result = vprintf(format, args);
	if (f) vfprintf(f, format, args);
	va_end(args);

	if (f) fclose(f);

	SetConsoleTextAttribute(hConsole, 7);  //white
	return result;
}

int AlpacaAccount::rprintf(const char* format, ...)
{
	//set color
	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 12);
	//print time tag
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
#ifdef OUTPUT_TO_FILE
	FILE* f = fopen(ALPACA_OUTPUT_FOLDER"output.txt", "a");
#else
	FILE* f = NULL;
#endif
	printf("%02d:%02d:%02d ", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);
	if (f) fprintf(f, "%02d:%02d:%02d ", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);

	int result;
	va_list args;

	va_start(args, format);
	result = vprintf(format, args);
	if (f) vfprintf(f, format, args);
	va_end(args);

	if (f) fclose(f);

	SetConsoleTextAttribute(hConsole, 7);  //white
	return result;
}


void AlpacaPushMessage(StreamMessage& msg)
{
	extern AlpacaAccount* alpacaAccount1;
	if (alpacaAccount1) {
		if (msg.type == -1) { //pong message
			printf("pong\n");
			alpacaAccount1->pingpang = -1;
			return;
		}
		EnterCriticalSection(&alpacaAccount1->critSec);
		alpacaAccount1->messageQueue.push_back(msg);
		LeaveCriticalSection(&alpacaAccount1->critSec);
	}
}

double getEventTime(std::string times)
{
	if (times.find("T") != std::string::npos) {
		int year, mon, day, hour, min;
		double sec;   //2021-06-04T08:33:28.804-05:00
		int ret = sscanf_s(times.c_str(), "%4d-%2d-%2dT%2d:%2d:%lfZ", &year, &mon, &day, &hour, &min, &sec);
		if (ret == 6) return (hour) * 3600.0 + min * 60.0 + sec;
		else return 0;
	}
	else return 0;
}


//it is not real time, maybe lagging
void AlpacaRealTimeStreamCallback(const web::websockets::client::websocket_incoming_message& msg)
{
	if (msg.message_type() == web::websockets::client::websocket_message_type::pong) {
		StreamMessage msg;
		msg.type = -1;
		AlpacaPushMessage(msg);
		return;
	}
	if (msg.length() == 0) return;
	//Parse server response, whether it is trade_update or account_update
	//rprintf("------------start call back----------------\n");
	auto is = msg.body();
	concurrency::streams::container_buffer<std::vector<uint8_t>> ret_data;
	is.read_to_end(ret_data).wait();

	//char bufStr[msg.length()+1];
	char* bufStr = new char[msg.length() + 1];
	memset(bufStr, 0, msg.length() + 1);
	memcpy(&bufStr[0], &(ret_data.collection())[0], msg.length());
	//uncomment if you want to have a class instance with buffer of responses. 
	//However, ret_data_count will increment to a large number if streaming a ton
	//Instead, store responses in vector (or buffer) and let user pull from it
	//memcpy(&bufStr[0], &(ret_data.collection())[0]+ret_data_count, msg.length());
	//ret_data_count += msg.length();


	Json::CharReaderBuilder readerBuilder;
	Json::CharReader* reader = readerBuilder.newCharReader();
	Json::Value noot;
	std::string errs;
	bool parsingSuccessful = reader->parse(bufStr, bufStr + msg.length(), &noot, &errs);
	if (!parsingSuccessful) {
		rprintf("Failed to parse the JSON, errors: %s\n", errs.c_str());
		return;
	}	
	//jsonPrint(noot);
	//std::cout << bufStr << std::endl;
	//std::cout << noot.toStyledString() << std::endl;

	//logged.push_back(noot);
	//printf("\n%s\n", bufStr);
	delete[] bufStr;
	//rprintf("------------end call back----------------\n");

	if (noot["stream"] == "authorization") {
		if (noot["data"]["status"] == "authorized") {
			printf("autorization success!\n");
		}
		else {
			rprintf("autorization failed! Please check the streaming key_id and secret_key\n");
		}
	}
	else if (noot["stream"] == "trade_updates") {
		StreamMessage msg;
		if (noot["data"]["event"] == "new") {
			jsonPrint(noot);
			msg.type = 0;
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "new";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.submitted_at);
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "canceled") {
			msg.type = 1;
			jsonPrint(noot);
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "canceled";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);
			msg.canceledQnt = order.qty - order.filled_qty;
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "fill") {
			msg.type = 2;
			jsonPrint(noot);
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "filled";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);

			msg.filled = std::stod(noot["data"]["qty"].asString());
			msg.execPrice = std::stod(noot["data"]["order"]["filled_avg_price"].asString());
			msg.position_qty = std::stod(noot["data"]["position_qty"].asString());

#ifdef OUTPUT_TO_FILE
			FILE* f = fopen(ALPACA_OUTPUT_FOLDER"trading.csv", "a");
#else
			FILE* f = NULL;
#endif
			if (f) {
				fprintf(f, "%s, %s, %g, %g, %s\n", msg.sym.c_str(), msg.action.c_str(), msg.filled, msg.execPrice, order.updated_at.c_str());
				fclose(f);
			}

			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "partial_fill") {
			msg.type = 3;
			jsonPrint(noot);
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "new";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);

			msg.filled = std::stod(noot["data"]["qty"].asString());
			msg.remaining = order.qty - order.filled_qty;
			msg.execPrice = std::stod(noot["data"]["order"]["filled_avg_price"].asString());
			msg.position_qty = std::stod(noot["data"]["position_qty"].asString());

#ifdef OUTPUT_TO_FILE
			FILE* f = fopen(ALPACA_OUTPUT_FOLDER"trading.csv", "a");
#else
			FILE* f = NULL;
#endif
			if (f) {
				fprintf(f, "%s, %s, %g, %g, %s\n", msg.sym.c_str(), msg.action.c_str(), msg.filled, msg.execPrice, order.updated_at.c_str());
				fclose(f);
			}

			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "rejected") {
			msg.type = 4;
			jsonPrint(noot);
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "rejected";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);

			//msg.rejectionReason = getXmlString(xml.c_str(), "RejectReason") == "REJECT_LOCATE" ? 1 : 0;
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "order_cancel_rejected") {
			msg.type = 5;
			jsonPrint(noot);
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = order.status;
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "order_cancel_rejected") {
			msg.type = 5;
			jsonPrint(noot);
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "new";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "expired") {
			msg.type = 6;
			jsonPrint(noot);
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "expired";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "replaced") {
			printf("replaced events:\n");
			jsonPrint(noot);
			msg.type = 1;
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = "canceled";
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(order.updated_at);
			msg.canceledQnt = order.qty - order.filled_qty;
			AlpacaPushMessage(msg);
			msg.orderid = order.replaced_by;
			msg.status = "new";
			if (order.filled_qty > 0.1) {  //replaced a partil filled order, its self partial filled
				msg.type = 9;
				msg.filled = order.filled_qty;
			}
			else msg.type = 0;
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "order_replace_rejected") {
			rprintf("order_replace_rejected events:\n");
			jsonPrint(noot);
			msg.type = 8;
			alpaca::Order order(noot["data"]["order"]);
			msg.orderid = order.id;
			msg.status = order.status;
			msg.action = order.action;
			msg.sym = order.symbol;
			msg.price = order.limit_price;
			msg.qnt = order.qty;
			msg.time = getEventTime(noot["data"]["timestamp"].asString());
			if (noot["data"]["reason"] == "TOO_LATE_TO_CANCEL") {
				msg.rejectionReason = 1;
			}
			AlpacaPushMessage(msg);
		}
		else if (noot["data"]["event"] == "pending_new") {
			printf("pending_new, pass the message\n");
			jsonPrint(noot);
		}
		else if (noot["data"]["event"] == "pending_cancel") {
			printf("pending_cancel, pass the message\n");
			jsonPrint(noot);
		}
		else if (noot["data"]["event"] == "pending_replace") {
			printf("pending_replace, pass the message\n");
			jsonPrint(noot);
		}
		else {
			rprintf("other events:\n");
			jsonPrint(noot);
		}
	}
	else if (noot["stream"] == "listening") {
		printf("Listening messages.\n");
	}
	else if (noot["stream"] == "account_updates") {
		rprintf("account_updates:\n");
		jsonPrint(noot);
	}
	else {
		rprintf("unknown message:\n");
		jsonPrint(noot);
	}
}

void AlpacaAccount::zeroActiveOrders(AlpacaOrder* o)
{
	if (!o) return;
	AlpacaPosition& p = getPosition(o->symbol);
	if (p.activeBuyOrder == o) p.activeBuyOrder = NULL;
	else if (p.activeSellOrder == o) p.activeSellOrder = NULL;
}

void AlpacaAccount::shareOrderInfo(AlpacaOrder* o)
{
	if (!o) return;
	updateShareOrder(o->orderId, o->symbol, o->action, o->status, o->totalQty, o->filledQty, o->lmtPrice, o->profit, 0.0, "Alpaca");
}

//remember that Messages update account may be lagging.
void AlpacaAccount::ProcessMessages()
{
	while (!messageQueue.empty()) {
		StreamMessage& msg = messageQueue.front();
		double lagging = 0;
		if (msg.time > 0) {
			double ct = GetTimeInSeconds();
			lagging = ct - msg.time;
			if (lagging > 0) {
				cprintf("The following message is lagging %g seconds\n", lagging);
			}
		}

		AlpacaOrder* p = NULL;
		for (auto order : orders) {
			if (order->orderId == msg.orderid) {
				p = order;
				break;
			}
		}

		if (msg.type == 0) { //new order placed, or from replaced order
			if (p) {
				if (p->status == "accepted" || p->status == "pending_new" || p->status == "accepted_for_bidding") p->status = "new";
				cprintf("Alpaca confirms a new order: %s, %s %s %g %g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), msg.sym.c_str(), p->lmtPrice, p->totalQty, p->remaining, p->status.c_str());
			}
			else {
				rprintf("Alpaca Update find a user input new order: %s, %s %s%g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), msg.sym.c_str(), msg.qnt, msg.price, msg.status.c_str());
				rprintf("I do not know whether this is a replace of old order or it is just a new order\n");
				alpaca::Order order = tradeApi->get_order(msg.orderid);
				updateOneOrder(msg.orderid, order.status, order.action, order.symbol, order.limit_price, order.qty, order.filled_qty, order.qty - order.filled_qty, order.limit_price);
				if (order.filled_qty > 0.1) addToPosition(order.symbol, order.action, order.filled_qty, order.limit_price);

/*
				p = new AlpacaOrder;
				p->orderId = msg.orderid;
				if (!tradeApi) {
					tradeApi = new alpaca::Tradeapi;
					tradeApi->init(endPoint, keyID, secretKey);
				}
				alpaca::Order order = tradeApi->get_order(p->orderId);
				p->symbol = order.symbol;
				p->action = order.action;
				p->lmtPrice = order.limit_price;
				p->status = order.status;
				p->totalQty = order.qty;
				p->filledQty = order.filled_qty;
				p->remaining = p->totalQty - p->filledQty;     
				p->avgFillPrice = p->lmtPrice;
				shareOrderInfo(p);
				rprintf("Alpaca User Manually input a new order: %s, %s %g %g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), p->lmtPrice, p->totalQty, p->remaining, p->status.c_str());
				rprintf("I do not know whether this is a replace of old order or it is just a new order\n");
				orders.push_back(p);
				*/
			}
		}
		else if (msg.type == 1) {  //order cancelled
			if (!p) rprintf("Cancelling a nonexisting order %s? Something must be wrong! Disconnected and not update?\n", msg.orderid.c_str());
			else {
				if (fabs(msg.qnt - p->totalQty) > 0.1) rprintf("In Cancelled message, the total quanty does not match %g, %g. Please check reason.\n", msg.qnt, p->totalQty);
				if (p->remaining > msg.canceledQnt + 0.01) {
					cprintf("cancelled order has some extra %g get filled\n", p->remaining - msg.canceledQnt);
					updateOneOrder(msg.orderid, "canceled", msg.action, msg.sym, msg.price, p->totalQty, msg.qnt - msg.canceledQnt, msg.canceledQnt, p->lmtPrice);
					addToPosition(msg.sym, msg.action, p->remaining - msg.canceledQnt, p->lmtPrice);
				}
				else if (p->remaining < msg.canceledQnt - 0.01) {
					rprintf("Alpaca canceled quantity %g > remaining %g, check reasons\n", msg.canceledQnt - (msg.qnt - p->totalQty), p->remaining);
				}
				if (p->status == "filled" || p->status == "expired" || p->status == "rejected") rprintf("Alpaca order cancelled from an impossible status %s\n", p->status.c_str());
				cprintf("Alpaca confirms order %s is cancelled\n", msg.orderid.c_str());
				p->status = "canceled";
				shareOrderInfo(p);
			}
			zeroActiveOrders(p);
		}
		else if (msg.type == 2) {  //order filled
			if (p) {
				if (p->totalQty < msg.qnt - 0.01) rprintf("Original order has larger original quantity. Check it.\n");
				if (p->status == "filled") {
					cprintf("Alpaca confirms a Filled order %s is really filled\n", p->orderId.c_str());
				}
				else {
					if (fabs(p->remaining - msg.filled) > 0.1) {
						rprintf("Remaining not equal to Filled qnt, This may happen when the filled message arrive faster than the partially filled message.\n");
						msg.filled = p->remaining;
					}
					updateOneOrder(msg.orderid, "filled", msg.action, msg.sym, msg.price, p->totalQty, p->totalQty, 0, msg.execPrice);
					addToPosition(msg.sym, msg.action, msg.filled, msg.execPrice);
					if (fabs(getPosition(msg.sym).quanty - msg.position_qty) > 0.1) {
						rprintf("Final position does not match %g-%g, something wrong? maybe another filled message is coming?\n", getPosition(msg.sym).quanty, msg.position_qty);
						if (lagging < 5) {
							cprintf("small lagging, use the message position as current position");
							getPosition(msg.sym).quanty = msg.position_qty;
						}
					}
				}
				shareOrderInfo(p);
				zeroActiveOrders(p);
				getPosition(msg.sym).actions.push_front(Action(4, msg.action, msg.execPrice, msg.qnt));
				//update the replacedto order to be rejected.
				std::string replacedto = p->replacedto;
				while (replacedto != "") {
					bool find = false;
					for (auto order : orders) {
						if (order->orderId == replacedto) {
							find = true;
							zeroActiveOrders(order);
							order->status = "rejected";
							cprintf("Change the replaced to order %s to be REJECTED.\n", order->orderId.c_str());
							//in the rare case, the replaced to order may be get partially filled, due to get order info may return falase information (the replaced from order info put into the replaced to order into).
							if (order->filledQty > 0) {
								AlpacaPosition& pos = getPosition(order->symbol);
								cprintf("Revert the position, update %s postion from %g to ", order->symbol.c_str(), pos.quanty);
								pos.quanty += order->action[0] == 'B' ? -order->filledQty : order->filledQty;
								cprintf("%g\n", pos.quanty);
								if (pos.shared) write_Td_position(pos.symbol, pos.quanty);
								order->filledQty = 0;
							}
							shareOrderInfo(order);
							replacedto = order->replacedto;
							break;
						}
					}
					if (!find) break;
				}
			}
			else {
				rprintf("Can not find this order %s? This should not happen! Something must be wrong 1023.\n", msg.orderid.c_str());
				updateOneOrder(msg.orderid, "filled", msg.action, msg.sym, msg.price, p->totalQty, p->totalQty, 0, msg.execPrice);
				addToPosition(msg.sym, msg.action, msg.filled, msg.execPrice);
			}
		}
		else if (msg.type == 3) {  //order partially filled
			if (p) {
				if (p->totalQty < msg.qnt) rprintf("Partially fill: Original order has larger original quantity.\n");
				if (p->status == "filled") {
					cprintf("Alpaca confirms a Filled order %s is partilly filled\n", p->orderId.c_str());
				}
				else {
					if (p->remaining > msg.filled + msg.remaining + 0.01) rprintf("Alpaca partially filled order, the remaining is too small. Something wrong? order remaining %g, this time filled %g, still remaining %g\n", p->remaining, msg.filled, msg.remaining);
					else if (p->remaining < msg.remaining + 0.01) cprintf("Alpaca confirms this known partial fill, already remaining %g, this time remaining %g\n", p->remaining, msg.remaining);
					else {
						cprintf("Alpaca find a partial fill\n");
						updateOneOrder(msg.orderid, msg.status, msg.action, msg.sym, msg.price, p->totalQty, p->totalQty - msg.remaining, msg.remaining, msg.execPrice);
						addToPosition(msg.sym, msg.action, msg.filled, msg.execPrice);
						if (fabs(getPosition(msg.sym).quanty - msg.position_qty) > 0.1) {
							rprintf("Final position in partial fill does not match %g-%g, something wrong? maybe another filled message is coming?\n", getPosition(msg.sym).quanty, msg.position_qty);
							if (lagging < 5) {
								cprintf("small lagging, use the message position as current position");
								getPosition(msg.sym).quanty = msg.position_qty;
							}
						}
					}
				}
				shareOrderInfo(p);
				getPosition(msg.sym).actions.push_front(Action(4, msg.action, msg.execPrice, msg.filled));
			}
			else {
				rprintf("An unknown partial filled order? This should not happen! Something must be wrong 1033.\n");
				updateOneOrder(msg.orderid, msg.status, msg.action, msg.sym, msg.price, msg.qnt, msg.filled, msg.remaining, msg.execPrice);
				addToPosition(msg.sym, msg.action, msg.filled, msg.execPrice);
			}
		}
		else if (msg.type == 4) {  //order rejected
			if (!p) rprintf("Rejecting a nonexisting order %s? Possible for replace rejection.\n", msg.orderid.c_str());
			else {
				if (p->status != "filled") {
					rprintf("Alpaca rejected order %s found, change its status from %s to REJECTED\n", msg.orderid.c_str(), p->status.c_str());
					p->status = "rejected";
					shareOrderInfo(p);
					if (msg.rejectionReason == 1 && p->action[0] == 's') {  //can not short
						rprintf("Can not short %s, change its canShort to false\n", msg.sym.c_str());
						getPosition(p->symbol).canShort = false;
					}
					else {
						//the replaced from order maybe filled.
						for (auto o : orders) {
							if (o->replacedto == msg.orderid) {
								if (o->status != "filled" && o->status != "rejected" && o->status != "canceled") {
									rprintf("Get the replaced from order %s status.\n", o->orderId.c_str());
									getOrderInfo(o);  //no more PENDING_REPLACE
								}
								break;
							}
						}
					}

				}
				else rprintf("Alpaca order %s is FILLED, How can it be rejected.\n", msg.orderid.c_str());
			}
			zeroActiveOrders(p);
		}
		else if (msg.type == 5) {  //order too late to canel
			if (!p) rprintf("Cancelling a nonexisting order %s? Something must be wrong! Disconnected and not update?\n", msg.orderid.c_str());
			else {
				p->status = msg.status;
				if (p->status == "replaced") p->status = "canceled";
				cprintf("Alpaca cancelling order %s is not successful, update its status %s\n", msg.orderid.c_str(), p->status.c_str());
				shareOrderInfo(p);
			}
		}
		else if (msg.type == 6) {  //order expired
			if (!p) rprintf("A nonexisting order %s expired? Something must be wrong! Disconnected and not update?\n", msg.orderid.c_str());
			else {
				cprintf("Alpaca order %s is expired\n", msg.orderid.c_str());
				p->status = "expired";
				shareOrderInfo(p);
			}
			zeroActiveOrders(p);
		}
		else if (msg.type == 8) {  //order replace rejected
			if (!p) rprintf("A nonexisting order %s replace rejected? Something must be wrong! Disconnected and not update?\n", msg.orderid.c_str());
			else {
				if (msg.rejectionReason == 1 && msg.status == "new") {
					cprintf("Too late to cancel, and its status is new, trying to cancel the order...\n");
					cancelOrder(p->orderId);
				}
				else {
					cprintf("Alpaca order %s replace is rejected, recover its status\n", msg.orderid.c_str());
					if (p->status != "canceled" || p->status != "filled" || p->status != "rejected")
						p->status = msg.status;
					if (p->status == "replaced") p->status = "canceled";
					if (msg.status != "new") rprintf("recovered status is %s, not new, please take care of it.\n", p->status.c_str());
					if (p->status != "new") zeroActiveOrders(p);
				}
				shareOrderInfo(p);
				std::string replacedto = p->replacedto;
				while (replacedto != "") {
					bool found = false;
					for (auto o : orders) {
						if (o->orderId == replacedto) {
							cprintf("Change the replaced to order %s to be rejected\n", replacedto.c_str());
							zeroActiveOrders(o);
							o->status = "rejected";
							replacedto = o->replacedto;
							shareOrderInfo(o);
							found = true;
							break;
						}
					}
					if (!found) break;
				}
			}
			
		}
		else if (msg.type == 9) { //replaced partial filled order
			if (p) {
				p->filledQty += msg.filled;
				p->remaining -= msg.filled;
				cprintf("Alpaca confirms a replace of partial filled order : %s, %s %s %g %g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), msg.sym.c_str(), p->lmtPrice, p->totalQty, p->remaining, p->status.c_str());
				shareOrderInfo(p);
			}
			else {
				rprintf("Alpaca Update find a user input new order: %s, %s %s%g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), msg.sym.c_str(), msg.qnt, msg.price, msg.status.c_str());
				p = new AlpacaOrder;
				p->orderId = msg.orderid;
				if (!tradeApi) {
					tradeApi = new alpaca::Tradeapi;
					tradeApi->init(endPoint, keyID, secretKey);
				}
				alpaca::Order order = tradeApi->get_order(p->orderId);
				p->symbol = order.symbol;
				p->action = order.action;
				p->lmtPrice = order.limit_price;
				p->status = order.status;
				p->totalQty = order.qty;
				p->filledQty = order.filled_qty;
				p->remaining = p->totalQty - p->filledQty;
				p->avgFillPrice = p->lmtPrice;
				rprintf("Alpaca User Manually input a new order: %s, %s %g %g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), p->lmtPrice, p->totalQty, p->remaining, p->status.c_str());
				rprintf("I do not know whether this is a replace of old order or it is just a new order\n");
				orders.push_back(p);
				shareOrderInfo(p);
			}
		}

		
		EnterCriticalSection(&critSec);
		messageQueue.pop_front();
		LeaveCriticalSection(&critSec);
	}
}

void AlpacaAccount::connect()
{
	if (!streamSession) streamSession = new alpaca::Stream;
	streamSession->init(AlpacaRealTimeStreamCallback);
	streamSession->connect(keyID, secretKey);

	std::vector<std::string> streams;
	streams.push_back("trade_updates");
	streams.push_back("account_updates");

	streamSession->subscribe(streams);
	bActive = true;
	pingpang = 0;
	streamSession->sendping();

	/*
	catch (tdma::StreamingException &e) {
		rprintf("Streaming Exception when connect: %s\n", e.what());
	}
	*/
}

void AlpacaAccount::disconnect()
{
	streamSession->disconnect();
	delete streamSession;
	streamSession = NULL;
	bActive = false;
}

inline std::string trim(std::string& str)
{
	str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
	str.erase(str.find_last_not_of('\r') + 1);         //surfixing spaces
	str.erase(str.find_last_not_of(' ') + 1);         //surfixing spaces
	return str;
}

bool AlpacaAccount::scanValue(std::string& line, const char* targetString, double& value1, double& value2) {
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

		cprintf("%s = %g, %g\n", targetString, value1, value2);
		return true;
	}
	return false;
}

bool AlpacaAccount::scanValue(std::string& line, const char* targetString, double& value) {
	if (line.find(targetString) != std::string::npos) {
		int equPos = line.find("=");
		if (equPos == std::string::npos) return true;
		std::string tmp = line.substr(equPos + 1, line.size() - equPos - 1);
		if (sscanf(tmp.c_str(), "%lf", &value) != 1) {
			for (int i = 0; i < 10; i++)  rprintf("Error Reading File: %s\n", line.c_str());
			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
		cprintf("%s = %g\n", targetString, value);
		return true;
	}
	return false;
}

bool AlpacaAccount::scanValue(std::string& line, const char* targetString, bool& value) {
	double tmp;
	if (scanValue(line, targetString, tmp)) {
		value = (tmp != 0);
		return true;
	}
	else return false;
}

bool AlpacaAccount::scanValue(std::string& line, const char* targetString, std::string& value) {
	if (line.find(targetString) != std::string::npos) {
		int equPos = line.find("=");
		if (equPos == std::string::npos) return true;
		value = line.substr(equPos + 1, line.size() - equPos - 1);
		trim(value);
		cprintf("%s = %s\n", targetString, value.c_str());
		return true;
	}
	return false;
}

bool AlpacaAccount::scanValue(std::string& line, const char* targetString, std::vector<double>& periods) {
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


void AlpacaAccount::UpdateControlFile()
{
	FILETIME t; DWORD fSize;
	HANDLE p = CreateFile(control_file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
	if ((wday == 0 || wday == 6 || (wday == 5 && (tm_struct->tm_hour > 16 || (tm_struct->tm_hour == 16 && tm_struct->tm_min >= cutmin))) || (wday == 1 && (tm_struct->tm_hour < 16 || (tm_struct->tm_hour == 16 && tm_struct->tm_min < cutmin))))) {
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

	for (auto pos : positions) {
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
				//pos->stdVariation = (volatility100 + volatility30) / 2;
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

	ExportControlFile();

}

void AlpacaAccount::ExportControlFile()
{

	//all else need update ControlFile.
	char line[200];
	std::string s = "ALL:\n";
	sprintf(line, "buySell = %d, %d\nalpacaAllTrading = %d\n", m_canBuy ? 1 : 0, m_canSell ? 1 : 0, m_alpacaAllTrading ? 1 : 0);
	s += line;
	for (auto pos : positions) {

		if (allStock[pos->stockIndex].primaryExchange == "")
			sprintf(line, "\n%s:\n", pos->symbol.c_str());
		else
			sprintf(line, "\n%s@%s:\n", pos->symbol.c_str(), allStock[pos->stockIndex].primaryExchange.c_str());
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
	f = fopen(control_file.c_str(), "w");
	if (!f) return;
	fprintf(f, "%s", s.c_str());
	fclose(f);
}

void AlpacaAccount::ProcessControlFile()
{
	static bool firsttime = true;
	static time_t now = time(NULL);
	time_t now_t = time(NULL);
	if (!firsttime && now_t - now < 2) return;
	firsttime = false;
	now = now_t;

	FILETIME t; DWORD fSize;
	HANDLE p = CreateFile(control_file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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
	unsigned int i;
	for (AlpacaPosition *p:positions) {
		AlpacaPosition& s = *p;
		s.init();
	}

	int stockIndex = -1;

	std::istringstream iss(pContent);  	delete[] pContent;
	AlpacaPosition pos;
	AlpacaPosition* pPos = &pos;

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
				pPos->startTime = hour1 * 60 + min1;
				pPos->endTime = hour2 * 60 + min2;
				cprintf("time=%d:%d, %d:%d\n", hour1, min1, hour2, min2);
			}
			else {
				continue;
			}
		}
		else if (line.find(":") != std::string::npos) {
			std::string sym = line.substr(0, line.find(":"));
			if (sym == "ALL") {
				stockIndex = -2;
				pPos = &pos;
				continue;
			}
			add_shared_symbol(sym);

			std::string exchange = "";
			if (sym.find("@") != std::string::npos) {
				exchange = sym.substr(sym.find("@") + 1, sym.size());
				sym = sym.substr(0, sym.find("@"));
			}
			for (i = 0; i < positions.size(); i++) {
				if (positions[i]->symbol == sym) {
					stockIndex = i;
					pPos = positions[i];
					addOneStock(sym);
					pPos->stockIndex = getStockIndex(sym);
					break;
				}
			}
			if (i == positions.size()) {
				addOneStock(sym);
				AlpacaPosition* p = new AlpacaPosition;
				p->symbol = sym;
				p->stockIndex = getStockIndex(sym);
				positions.push_back(p);
				stockIndex = i;
				pPos = p;
				if (exchange != "") allStock[pPos->stockIndex].primaryExchange = exchange;
			}
			cprintf("%s:\n", sym.c_str());
		}
		else if (scanValue(line, "bottomPrice", pPos->bottomPrice, pPos->targetBottomPrice)) continue;
		else if (scanValue(line, "topPrice", pPos->topPrice, pPos->targetTopPrice)) continue;
		else if (scanValue(line, "multiplier", pPos->multiplier, pPos->targetMultiplier)) continue;
		else if (scanValue(line, "offset", pPos->offset, pPos->targetOffset)) continue;
		else if (scanValue(line, "buySell", value1, value2)) {
			if (stockIndex == -2) {
				m_canBuy = (value1 != 0);
				m_canSell = (value2 != 0);
				cprintf("ALL: m_canBuy = %s, m_canSell = %s\n", m_canBuy ? "true" : "false", m_canSell ? "true" : "false");
			}
			else {
				pPos->canBuy = (value1 != 0);
				pPos->canSell = (value2 != 0);
				cprintf("canBuy = %s, canSell = %s\n", pPos->canBuy ? "true" : "false", pPos->canSell ? "true" : "false");
			}
			continue;
		}
		else if (scanValue(line, "fatTailBuyShortEnbled", value1, value2)) {
			pPos->fatTailBuyEnbled = (value1 != 0);
			pPos->fatTailShortEnbled = (value2 != 0);
			printf("fatTailBuyEnbled = %s, fatTailShortEnbled = %s\n", pPos->fatTailBuyEnbled ? "true" : "false", pPos->fatTailShortEnbled ? "true" : "false");
			continue;
		}
		else if (scanValue(line, "fatTailBuyShortDone", value1, value2)) {
			pPos->fatTailBuyDone = (value1 != 0);
			pPos->fatTailShortDone = (value2 != 0);
			printf("fatTailBuyDone = %s, fatTailShortDone = %s\n", pPos->fatTailBuyDone ? "true" : "false", pPos->fatTailShortDone ? "true" : "false");
			continue;
		}
		else if (scanValue(line, "periods", pPos->periods)) continue;
		else if (scanValue(line, "alpacaAllTrading", m_alpacaAllTrading)) continue;
		else if (scanValue(line, "previousClose", pPos->previousClose)) continue;
		else if (scanValue(line, "stdVariation", pPos->stdVariation)) continue;
		else if (scanValue(line, "lowBoundDiscount", pPos->lowBoundDiscount)) continue;
		else if (scanValue(line, "hedgeRatio", pPos->hedgeRatio)) continue;
		else if (scanValue(line, "takenProfit", pPos->takenProfit)) continue;
		else if (scanValue(line, "longTermHolding", pPos->longTermHolding)) continue;
		else if (scanValue(line, "buyBelow", pPos->buyBelow)) continue;
		else if (scanValue(line, "sellAbove", pPos->sellAbove)) continue;
		else if (scanValue(line, "buyAbove", pPos->buyAbove)) continue;
		else if (scanValue(line, "sellBelow", pPos->sellBelow)) continue;
		else if (scanValue(line, "canShort", pPos->canShort)) continue;
		else if (scanValue(line, "reverse", pPos->reverse)) continue;
		else if (scanValue(line, "shared", pPos->shared)) continue;
		else if (scanValue(line, "alpacaTrading", pPos->alpacaTrading)) continue;
		else if (scanValue(line, "maxAlpacaMarketValue", pPos->maxAlpacaMarketValue)) continue;
	}

	cprintf("*********Alpaca ***********\n");
	for (i = 0; i < positions.size(); i++) {
		AlpacaPosition& stock = * positions[i];
		if (stock.stockIndex == -1) continue;
		stock.oldBottomPrice = stock.bottomPrice;
		stock.oldTopPrice = stock.topPrice;
		stock.oldMultiplier = stock.multiplier;
		stock.oldOffset = stock.offset;
		if (stock.targetBottomPrice == 0) stock.targetBottomPrice = stock.bottomPrice;
		if (stock.targetTopPrice == 0) stock.targetTopPrice = stock.topPrice;
		if (stock.targetMultiplier < 0) stock.targetMultiplier = stock.multiplier;
		if (stock.targetOffset > 1000000) stock.targetOffset = stock.offset;

		//output the one with different
		if (fabs(stock.targetBottomPrice - stock.bottomPrice) > 0.001) {
			cprintf("%s Bottom price different: %g, %g\n", stock.symbol.c_str(), stock.bottomPrice, stock.targetBottomPrice);
		}
		if (fabs(stock.targetTopPrice - stock.topPrice) > 0.001) {
			cprintf("%s Top price different: %g, %g\n", stock.symbol.c_str(), stock.topPrice, stock.targetTopPrice);
		}
		if (fabs(stock.targetMultiplier - stock.multiplier) > 0.01) {
			cprintf("%s Multiplier different: %g, %g\n", stock.symbol.c_str(), stock.multiplier, stock.targetMultiplier);
		}
		if (fabs(stock.targetOffset - stock.offset) > 0.1) {
			cprintf("%s Offset different: %g, %g\n", stock.symbol.c_str(), stock.offset, stock.targetOffset);
		}

		/*
		if (fabs(stock.targetBottomPrice - stock.bottomPrice) / stock.bottomPrice >= 0.5) {
			rprintf("Wrong target BottomPrice!!!!!!!!!!!!!\n");
			stock.targetBottomPrice = stock.bottomPrice;
		}
		if (fabs(stock.targetTopPrice - stock.topPrice) / stock.topPrice >= 0.5) {
			rprintf("Wrong target TopPrice!!!!!!!!!!!!!\n");
			stock.targetTopPrice = stock.topPrice;
		}
		if (fabs(stock.targetMultiplier - stock.multiplier) / stock.multiplier >= 0.15) {
			rprintf("Wrong target multiplier!!!!!!!!!!!!!\n");
			stock.targetMultiplier = stock.multiplier;
		}
		*/
	}
	cprintf("*********Alpaca ***********\n");
	if (tradeApi) {
		cprintf("update shared postions\n");
		for (auto p : positions) {
			if (p->shared) write_Td_position(p->symbol, p->quanty);
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1000)); //wait for other process reading its file a little.
}

AlpacaPosition & AlpacaAccount::getPosition(std::string sym)
{
	unsigned int i;
	for (i = 0; i < positions.size(); i++) {
		AlpacaPosition& stock = *positions[i];
		if (stock.symbol == sym) return stock;
	}
	rprintf("Something wrong, can not find the stock!\n");
	return *(new AlpacaPosition);  
}

void AlpacaAccount::cancelAllOrders()
{
	extern std::vector<int> soundPlay;
	if (!tradeApi) return;
	cprintf("Alpaca Cancel All Orders\n");
	bool success;
	success = tradeApi->cancel_all_orders();
	if (!success) {
		rprintf("Alpaca Cancel All Orders Error\n");
		rprintf("cancelAllOrders ErrorMsg:%s\n", tradeApi->errorMsg.c_str());
		return;
	}
}

void AlpacaAccount::cancelOrder(std::string orderId)
{
	extern std::vector<int> soundPlay;
	if (!tradeApi) return;
	std::cout << "Alpaca Cancel Order ID: " << orderId << std::endl;
	bool success;
	success = tradeApi->cancel_order(orderId);
	if (!success) {
		cprintf("Alpaca Cancel Order Error\n");
		cprintf("cancelOrder ErrorMsg:%s\n", tradeApi->errorMsg.c_str());
		unsigned int i;
		for (i = 0; i < orders.size(); i++) {
			if (orders[i]->orderId == orderId) {
				getPosition(orders[i]->symbol).actions.push_front(Action(3, orders[i]->action, orders[i]->lmtPrice, orders[i]->totalQty));
				if (orders[i]->status != "filled" && orders[i]->status != "rejected" && orders[i]->status != "canceled" && orders[i]->status != "pending_cancel") {
					getOrderInfo(orders[i]);
				}
				break;
			}
		}
		return;
	}

	std::cout << "Alpaca Cancel: " << std::boolalpha << success << std::endl;
	unsigned int i;
	for (i = 0; i < orders.size(); i++) {
		if (orders[i]->orderId == orderId) {
			getPosition(orders[i]->symbol).actions.push_front(Action(3, orders[i]->action, orders[i]->lmtPrice, orders[i]->totalQty));
			if (success) {
				if (orders[i]->status != "pending_cancel") {
					orders[i]->status = "pending_cancel";
					orders[i]->pendingCancelTime = GetTimeInSeconds();
				}
				zeroActiveOrders(orders[i]);
				shareOrderInfo(orders[i]);
				soundPlay.push_back(3);
			}
			else {
				getOrderInfo(orders[i]);
			}
			break;
		}
	}
}

void AlpacaAccount::placeOrder(std::string sym, std::string action, double price, double qnt)
{
	if (!tradeApi) return;

	double curPosition = round(getPosition(sym).quanty);
	bool isBuy = (action[0] == 'b');
	bool toOpen = (isBuy && (curPosition >= 0.0)) || (!isBuy && (curPosition <= 0.0));

	if (isBuy) {
		if (curPosition < 0) {
			if (qnt > -curPosition) qnt = -curPosition;
		}
	}
	else {
		if (curPosition > 0) {
			if (qnt > curPosition) qnt = curPosition;
		}
	}

	size_t size = round(qnt);
	cprintf("Alpaca to placed a new order: %s %s %d-%g(%g), curPosition=%g\n", action.c_str(), sym.c_str(), size, round(qnt), price, curPosition);
	getPosition(sym).actions.push_front(Action(1, action, price, qnt));

	//if (sym.size() == 5 && (sym[4] == 'Y' || sym[4] == 'F')) order1.set_session(tdma::OrderSession::NORMAL);  //OTC stocks, some OTC stocks may not list here.
	//else order1.set_session(tdma::OrderSession::SEAMLESS);
	std::string oid;
	oid = tradeApi->submit_order(sym, qnt, isBuy?"buy":"sell", "limit", "day", price);
	if (oid == "") {
		std::string err = tradeApi->errorMsg;
		rprintf("placeOrder ErrorMsg:%s\n", tradeApi->errorMsg.c_str());
		if (err.find("insufficient qty available for order") != std::string::npos) {
			if (err.find("related_orders") != std::string::npos) {
				cprintf("get related order information.\n");
				Json::CharReaderBuilder readerBuilder;
				Json::CharReader* reader = readerBuilder.newCharReader();
				Json::Value noot;
				std::string errs;
				reader->parse(err.c_str(), err.c_str() + err.length(), &noot, &errs);
				for (auto id : noot["related_orders"]) {
					for (auto o : orders) {
						if (o->orderId == id.asString()) {
							o->totalQty = std::stod(noot["held_for_orders"].asString()) + o->filledQty;
							cprintf("update the totalQty of %s to %g\n", o->orderId.c_str(), o->totalQty);
						}
					}
				}
			}
			else 
				rprintf("There is insufficient qty available for order, check pending orders.\n");
			//if (action[0] == 'b') getPosition(sym).canBuy = false;
			//else getPosition(sym).canSell = false;
			return;
		}
		else if (err.find("cannot be sold short") != std::string::npos) {
			rprintf("Stock %s cannot be sold short, set the canShort to be false.\n", sym.c_str());
			getPosition(sym).canShort = false;
			return;
		}
		else if (err.find("You do not have enough available cash/buying power for this order") != std::string::npos) {
			rprintf("You do not have enough available cash/buying power for this order, Cancel all the buy orders.\n");
			m_canBuy = false;
			return;
		}
		else if (err.find("related_orders") != std::string::npos) {
			rprintf("get related order information.\n");
			Json::CharReaderBuilder readerBuilder;
			Json::CharReader* reader = readerBuilder.newCharReader();
			Json::Value noot;
			std::string errs;
			reader->parse(err.c_str(), err.c_str() + err.length(), &noot, &errs);
			for (auto id : noot["related_orders"]) {
				alpaca::Order order = tradeApi->get_order(id.asString());
				rprintf("related orders %s, %s %s %g(%g), filled %g, status %s\n", order.id.c_str(), order.action.c_str(), order.symbol.c_str(), order.qty, order.limit_price, order.filled_qty, order.status.c_str());
			}
			return;
		}
		else {
			return;
		}
	}

	cprintf("Alpaca Placed a new order: %s, %s %s %d-%g(%g), curPosition=%g\n", oid.c_str(), action.c_str(), sym.c_str(), size, round(qnt), price, curPosition);
	unsigned int i;
	for (i = 0; i < orders.size(); i++) {
		if (orders[i]->orderId == oid) {
			break;
		}
	}
	if (i >= orders.size()) {
		AlpacaOrder* p = new AlpacaOrder;
		p->orderId = oid;
		p->symbol = sym;
		p->action = action;
		p->status = "new";
		p->lmtPrice = price;
		p->totalQty = size;
		p->filledQty = 0;
		p->remaining = size;
		p->avgFillPrice = price;

		orders.push_back(p);
		shareOrderInfo(p);
		if (isBuy) getPosition(sym).activeBuyOrder = p;
		else getPosition(sym).activeSellOrder = p;
	}
}


void AlpacaAccount::replaceOrder(std::string orderId, std::string sym, std::string action, double price, double qnt)
{
	if (!tradeApi) return;

	double curPosition = round(getPosition(sym).quanty);
	bool isBuy = (action[0] == 'b');
	bool toOpen = (isBuy && (curPosition >= 0.0)) || (!isBuy && (curPosition <= 0.0));

	//the following should not needed, because there is processing in wangjob()
	if (action[0] == 'b') {
		if (curPosition < 0) {
			if (qnt > -curPosition) qnt = -curPosition;
		}
	}
	else {
		if (curPosition > 0) {
			if (qnt > curPosition) qnt = curPosition;
		}
	}

	getPosition(sym).actions.push_front(Action(2, action, price, qnt));

	size_t size = round(qnt);
	std::string oid;
	oid = tradeApi->replace_order(orderId, (int)round(qnt), price);
	if (oid == "") {
		std::string err = tradeApi->errorMsg;
		cprintf("replaceOrder ErrorMsg:%s\n", tradeApi->errorMsg.c_str());
		if (err.find("unable to replace order") != std::string::npos) cprintf("Can not replace the order %s.\n", orderId.c_str());
		else if (err.find("validation problem") != std::string::npos) rprintf("Replace order get 400 error, validation problem: %s.\n", orderId.c_str());
		else {
			rprintf("Alpaca Replace Order Error\n");
		}

		unsigned int i;
		for (i = 0; i < orders.size(); i++) {
			if (orders[i]->orderId == orderId) {
				if (orders[i]->status != "filled" && orders[i]->status != "rejected" && orders[i]->status != "canceled" && orders[i]->status != "pending_cancel") {
					getOrderInfo(orders[i]);  //no more PENDING_REPLACE
					//if (!getOrderInfo(orders[i])) orders[i]->status = "pending_replace";   //use pending replace, let call back function update it.
					if (orders[i]->status == "new") {
						cprintf("Cancel Order %s and place new order.\n", orderId.c_str());
						cancelOrder(orderId);
						placeOrder(sym, action, price, qnt);
					}
				}
				break;
			}
		}
		return;
	}
	cprintf("Alpaca Replaced order %s by order: %s, %s %s %d-%g(%g), curPosition=%g\n", orderId.c_str(), oid.c_str(), action.c_str(), sym.c_str(), size, qnt, price, curPosition);

	unsigned int i;
	for (i = 0; i < orders.size(); i++) {
		if (orders[i]->orderId == orderId) {
			if (orders[i]->status != "pending_cancel") {
				orders[i]->status = "pending_cancel";   //use pending cancel, even it may be cancelled, let call back function update it.
				orders[i]->pendingCancelTime = GetTimeInSeconds();
			}
			orders[i]->replacedto = oid;

			//create a new order
			AlpacaOrder* p = new AlpacaOrder;
			p->orderId = oid;
			p->symbol = sym;
			p->action = action;
			p->status = "new";
			p->lmtPrice = price;
			p->totalQty = size;
			p->remaining = size;
			p->filledQty = 0;
			p->avgFillPrice = price;

			orders.push_back(p);
			shareOrderInfo(p);
			if (action[0] == 'b') getPosition(sym).activeBuyOrder = p;
			else getPosition(sym).activeSellOrder = p;
			break;
		}
	}
}


void AlpacaAccount::printPositions() 
{
#ifndef NO_OUTPUT
	cprintf("############Start Alpaca Order List###########\n");
	for (unsigned int i = 0; i < orders.size(); i++) {
		AlpacaOrder* o = orders[i];
		cprintf("%s %s %s %s <%s> %g(%g) @ %g\n", o->account.c_str(), o->orderId.c_str(), o->symbol.c_str(), o->action.c_str(), o->status.c_str(), o->totalQty, o->remaining, o->avgFillPrice);
	}
	cprintf("############End Alpaca Order List###########\n");
#endif

	cprintf("-----Alpaca  -----\n");
	double totalProfit = 0;
	double totalCommission = 0;
	for (unsigned int index = 0; index < positions.size(); index++) {
		AlpacaPosition& s = *positions[index];
		double ibpos = 0;
		if (s.shared) read_IB_position(s.symbol, ibpos);
		cprintf("Stock %s(%g): Shares %g, cash %g; Total Profit %g (%g) (IB position %g)\n", s.symbol.c_str(), s.quanty, s.totalShares, s.totalCash, round(s.totalProfit, 0.01), round(s.totalCommission, 0.01), ibpos);

		//double earnedProfit = nCash + nShare * (s.askPrice + s.bidPrice) / 2;
		totalProfit += s.totalProfit;
		totalCommission += s.totalCommission;
	}
	cprintf("Total Profit is %g, and total Commission is %g\n", round(totalProfit, 0.01), round(totalCommission, 0.01));
	cprintf("Account Avaible Funds is %g, Excess Liquidity is %g\n", round(availableFunds), round(excessLiquidity));
}

//update order profit, commission
//update position total profit, and commission
//Input shares is the additional shares just executed.
//Input cash is the additional cash just executed.
void AlpacaAccount::updateOrderProfit(AlpacaOrder* o, double shares, double cash)
{
	unsigned int i;
	for (i = 0; i < positions.size(); i++) {
		if (positions[i]->symbol == o->symbol) break;
	}
	if (i >= positions.size()) return;

	AlpacaPosition& s = *positions[i];

	s.totalProfit -= o->profit;
	s.totalCommission -= o->commission;

	if (o->action[0] == 's') {
		s.totalShares -= shares;
		s.totalCash += cash;
		double delta = deltaCalculation(o->avgFillPrice, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
		delta += o->filledQty;
		double price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
		o->profit = o->filledQty * (o->avgFillPrice - price) * 0.5;
		o->profit -= o->commission;
	}
	else {
		s.totalShares += shares;
		s.totalCash -= cash;
		double delta = deltaCalculation(o->avgFillPrice, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
		delta -= o->filledQty;
		double price = priceFromDelta(delta, s.bottomPrice, s.topPrice, s.multiplier, s.offset, s.periods);
		o->profit = -o->filledQty * (o->avgFillPrice - price) * 0.5;
		o->profit -= o->commission;
	}

	s.totalProfit += o->profit;
	s.totalCommission += o->commission;
}

void AlpacaAccount::updateOnePosition(std::string sym, double qnt, double cost)
{
	unsigned int i;
	for (i = 0; i < positions.size(); i++) {
		if (positions[i]->symbol == sym) {
			cprintf("Update Position Quanty %s from %g to %g\n", sym.c_str(), positions[i]->quanty, qnt);
			if (positions[i]->quanty > 0 && positions[i]->quanty != qnt) {
				rprintf("Position different, there are errors! We did an update\n");
			}
			positions[i]->quanty = qnt;
			if (positions[i]->shared) write_Td_position(positions[i]->symbol, qnt);
			return;
		}
	}
	if (i >= positions.size()) {
		cprintf("New Position %s %g\n", sym.c_str(), qnt);
		AlpacaPosition* p = new AlpacaPosition;
		p->symbol = sym;
		p->quanty = qnt;
		p->stockIndex = getStockIndex(sym);
		if (p->shared) write_Td_position(sym, qnt);
		positions.push_back(p);
	}
}


//qnt can be negative
void AlpacaAccount::addToPosition(std::string sym, std::string action, double qnt, double cost)
{
	lastPositionChangeTime = time(NULL);
	if (action[0] == 'b') qnt = fabs(qnt);
	else qnt = -fabs(qnt);
	unsigned int i;
	for (i = 0; i < positions.size(); i++) {
		if (positions[i]->symbol == sym) {
			cprintf("update %s postion from %g to ", sym.c_str(), positions[i]->quanty);
			positions[i]->quanty += qnt;
			positions[i]->newOrderSettled = false;
			cprintf("%g\n",positions[i]->quanty);
			if (positions[i]->shared) write_Td_position(positions[i]->symbol, positions[i]->quanty);
			return;
		}
	} 
	if (i >= positions.size()) {
		AlpacaPosition* p = new AlpacaPosition;
		p->symbol = sym;
		p->quanty = qnt;
		p->newOrderSettled = false;
		p->stockIndex = getStockIndex(sym);
		if (p->shared) write_Td_position(p->symbol, p->quanty);
		positions.push_back(p);
		cprintf("A new position %s postion initialized to %g\n", sym.c_str(), p->quanty);
	}
}

//No delete in update order. Purpose: no conflict for multi threads.
void AlpacaAccount::updateOneOrder(std::string orderid, std::string status, std::string buySell, std::string sym, double price, double qnt, double filled, double remaining, double avgPrice)
{
	unsigned int i;
	extern std::vector<int> soundPlay;
	for (i = 0; i < orders.size(); i++) {
		AlpacaOrder* p = orders[i];
		if (p->orderId == orderid) {  //find the order
			//the order is filled or partially filled
			if (filled > p->filledQty) {
				if (status == "filled") { //filled order
					if (p->status == "canceled") rprintf("Wrong: Alpaca Canceling order get filled: %s, %s %s %g(%g)\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price);
					cprintf("Alpaca Order filled %s: %s %s %g of %g(%g)", orderid.c_str(), buySell.c_str(), sym.c_str(), filled - p->filledQty, qnt, price);
					soundPlay.push_back(1);
					double shares = filled - p->filledQty;
					double cash = avgPrice * filled - p->avgFillPrice * p->filledQty;
					p->totalQty = qnt;
					p->action = buySell;
					p->lmtPrice = price;
					p->filledQty = filled;
					p->status = status;
					p->remaining = remaining;
					p->avgFillPrice = avgPrice;
					if (filled > 0.01) p->partiallyFilled = true;
					p->fillTime = GetTimeInSeconds();
					updateOrderProfit(p, shares, cash);  //update the order, position's profit, commissions, and cash changes.
					cprintf("total profit %g\n", p->profit);
					shareOrderInfo(p);
					return;
				}
				else {  //partially filled order
					cprintf("Alpaca Order partially filled %s: %s %s %g of %g(%g)\n", orderid.c_str(), buySell.c_str(), sym.c_str(), filled - p->filledQty, qnt, price);
					cprintf("Alpaca Updated an Partially Filled Order: %s, %s %s%g(%g)from %s\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price, p->status.c_str());
					soundPlay.push_back(2);
					double shares = filled - p->filledQty;
					double cash = avgPrice * filled - p->avgFillPrice * p->filledQty;
					p->totalQty = qnt;
					p->action = buySell;
					p->lmtPrice = price;
					p->filledQty = filled;

					if (p->status == "pending_replace") p->status = "new";      //pending replace order is partially filled.
					else if (p->status == "partially_filled") p->status = "new";      
					else if (p->status != "canceled" && p->status != "pending_cancel" && p->status != "GUESS_FILLED") p->status = status;   //keep CANCELED or PENDING_CANCEL, these orders can get partially filled message later.
					cprintf(" to %s\n", p->status.c_str());
					p->remaining = remaining;
					p->avgFillPrice = avgPrice;
					if (filled > 0.01) p->partiallyFilled = true;
					updateOrderProfit(p, shares, cash);  //update the order, position's profit, commissions, and cash changes.
					cprintf("total profit %g\n", p->profit);
					shareOrderInfo(p);
					return;
				}
			}
			else if (filled < p->filledQty) {
				rprintf("!!!Found order update later than other update! This rarely happen. Check if there is anything wrong!!\n");
				return; //updating later than call back function, then no update
			}
			else if (p->status == "canceled" && status != "canceled") return;  //A just cancelled order, no need to update. 
			else if (p->status == "pending_cancel" && status != "canceled") return;  //A pending cancel order is still waiting, no need to update. 
			else if (p->status == "filled") return;  //A filled order, no need to update. 
			else {  //no filled or partially filled
				p->totalQty = qnt;
				p->action = buySell;
				p->lmtPrice = price;
				p->filledQty = filled;
				if (p->status != status) cprintf("Alpaca Updated an Order: %s, %s %s%g(%g)from %s to %s\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price, p->status.c_str(), status.c_str());
				//if (status == "canceled" && p->status != "canceled") soundPlay.push_back(3);
				p->status = status;
				p->remaining = remaining;
				p->avgFillPrice = avgPrice;
				if (filled > 0.01) p->partiallyFilled = true;
				shareOrderInfo(p);
				return;
			}
			return;
		}  //end find the order
	}
	//find a new order id
	if (i >= orders.size()) {
		if (getStockIndex(sym) == -1) return;
		if (fabs(filled) < 0.1 && (status == "canceled" || status == "rejected" || status == "expired")) return;
		if (status == "partially_filled") status = "new";
		cprintf("Alpaca Update find a new order: %s, %s %s%g(%g)status: %s\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price, status.c_str());
		AlpacaOrder* p = new AlpacaOrder;
		p->orderId = orderid;
		p->symbol = sym;
		p->action = buySell;
		p->lmtPrice = price;
		p->status = status;
		p->totalQty = qnt;
		p->filledQty = filled;
		p->remaining = remaining;
		p->avgFillPrice = avgPrice;
		if (filled > 0.01) p->partiallyFilled = true;
		if (filled > 0.01) {
			updateOrderProfit(p, filled, avgPrice * filled);  //update the order, position's profit, commissions, and cash changes.
			if (status == "filled") {
				cprintf("Alpaca New Order filled %s: %s %s %g of %g(%g)", orderid.c_str(), buySell.c_str(), sym.c_str(), filled, qnt, price);
				soundPlay.push_back(1);
			}
			else {
				cprintf("Alpaca New Order partilly filled %s: %s %s %g of %g(%g)", orderid.c_str(), buySell.c_str(), sym.c_str(), filled, qnt, price);
				soundPlay.push_back(2);
			}
			cprintf("total profit %g\n", p->profit);
		}
		shareOrderInfo(p);
		orders.push_back(p);
	}
	//cprintf("Order ID: %s, Status: %s\n", orderid.c_str(), status.c_str());
}

void AlpacaAccount::modifyLmtOrder(std::string orderId, double newSize, double newPrice)
{
	//				"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",
	for (unsigned int i = 0; i < orders.size(); i++) {
		AlpacaOrder * o = orders[i];
		if (o->orderId != orderId) continue;
		if (o->status != "new" && o->status != "pending_new" && o->status != "PENDING_ACTIVATION" && o->status != "accepted") return;
		if (o->orderType != "LMT") return;
		//the following is not needed, since the commission at tdameritrade is 0
		//if (o->partiallyFilled && fabs(o->lmtPrice - newPrice) / o->lmtPrice < 0.003 && fabs(newSize - o->remaining) > max(min(o->remaining / 3, 10), 2)) return;
		if (fabs(o->lmtPrice - newPrice) < 0.001 && fabs(newSize - o->remaining) < 0.1) return;  //same order 

		std::string sym = o->symbol;
		std::string action = o->action;
		cprintf("Alpaca Modifying order %s, %s %s %g(%g)status: %s to %s%g(%g)\n", o->orderId.c_str(), o->action.c_str(), sym.c_str(), o->totalQty, o->lmtPrice, o->status.c_str(), action.c_str(), newSize, newPrice);

		if (o->filledQty > 0 || o->partiallyFilled) {
			cprintf("Replace a partial filled order %s %g, cancel it and place a new order\n", o->orderId.c_str(), o->filledQty);
			cancelOrder(o->orderId);
			placeOrder(sym, action, newPrice, newSize);
		}
		else replaceOrder(orderId, sym, action, newPrice, newSize);

		//cancelOrder(o->orderId);
		//placeOrder(sym, action, newPrice, newSize);
		cprintf("Alpaca End Modified order\n");
		break;
	}
}



bool AlpacaAccount::getOrderInfo(AlpacaOrder * p)
{
	extern std::vector<int> soundPlay;
	bool print = true;
	if (!tradeApi) {
		tradeApi = new alpaca::Tradeapi;
		tradeApi->init(endPoint, keyID, secretKey);
	}
	alpaca::Order order = tradeApi->get_order(p->orderId);
	if (order.id == "") {
		rprintf("Alpaca get order %s information Error\n", p->orderId.c_str());
		for (int i = 0; i < 200; i++) {//10 minutes total
			cprintf("Sleep 3 seconds and try again...\n");
			std::this_thread::sleep_for(std::chrono::seconds(3));
			order = tradeApi->get_order(p->orderId);
			if (order.id != "") {
				cprintf("success!\n");
				break;
			}
		}
		if (order.id == "") {
			//delete tradeApi; tradeApi = NULL;
			return false;
		}
	}

	std::string status = order.status;
	if (status == "partially_filled") status = "new";

	if (status == "rejected" || status == "expired") {
		cprintf("Get Order %s Info: %s %s %g(%g)status: %s to %s\n", p->orderId.c_str(), p->action.c_str(), p->symbol.c_str(), p->totalQty, p->lmtPrice, p->status.c_str(), status.c_str());
		p->status = status;
		zeroActiveOrders(p);
		shareOrderInfo(p);
		return true;
	}
	else if (status == "canceled" || status == "filled" || status == "new" || status == "accepted" || status == "replaced") {
		if (status == "filled") {
			if (order.qty - order.filled_qty > 0.1)
				rprintf("GetOrderInfo, filled order still has remaining, quantity %g, filled %g\n", status.c_str(), order.qty, order.filled_qty);
		}

		if (p->totalQty > order.qty + 0.01) {
			rprintf("GetOrderInfo, the totalQty %g > quantity %g, correct totalQty to quantity, and remaining too\n", p->totalQty, order.qty);
			p->remaining -= p->totalQty - order.qty;
			p->totalQty = order.qty;
		}

		double fqty = p->remaining - (order.qty - order.filled_qty);  //consider remaining, instead of quanty
		if (fqty < -0.01) {
			cprintf("get order info, find filled quanty less than 0, remaining %g < quantity %g - filled %g\n", p->remaining, order.qty, order.filled_qty);
			cprintf("discard this information, this can be happenned if an order partially filled and has not been updated when request information.\n");
			shareOrderInfo(p);
			return false;
		}
		p->filledQty += fqty;
		p->remaining = order.qty - order.filled_qty;
		p->avgFillPrice = p->lmtPrice = order.limit_price;
		if (fqty > 0.01) {
			cprintf("Getorderinfo, remaining position %g get filled\n", fqty);
			/*
			//check if it is replaced from order get filled.
			for (auto o : orders) {
				if (o->replacedto == p->orderId) {
					if (o->status != "filled" && o->status != "rejected" && o->status != "canceled") {
						cprintf("It is replace from other order, which may be partially replaced. Get the replaced from order %s status.\n", o->orderId.c_str());
						getOrderInfo(o);
						if (o->filledQty > 0) {
							cprintf("filled from order is partially filled, change current order filled qty");
							p->filledQty -= o->filledQty;
							fqty -= o->filledQty;
						}
					}
					break;
				}
			}
			*/

			updateOrderProfit(p, fqty, fqty * p->lmtPrice);
			cprintf("Order total profit %g\n", p->profit);
			addToPosition(p->symbol, p->action, fqty, p->lmtPrice);
			if (status == "filled") soundPlay.push_back(1);
			else soundPlay.push_back(2);
		}
		p->status = status;
		cprintf("Get order info, order %s is %s\n", p->orderId.c_str(), status.c_str());
		if (status == "canceled") soundPlay.push_back(3);
		if (status != "new" && status != "accepted") zeroActiveOrders(p);
		shareOrderInfo(p);
		return true;
	}
	else {
		rprintf("Find new status %s\n", status.c_str());
		p->status = status;
		shareOrderInfo(p);
		return true;
	}
	return false;
}

void AlpacaAccount::exportActivities(int year, int mon, int day)
{
	char filename[100];
	sprintf_s(filename, "%s%04d-%02d-%02d.csv", ALPACA_ACTIVITY_FOLDER, year, mon, day);
	HANDLE p = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (p != INVALID_HANDLE_VALUE) {
		CloseHandle(p);
		return;
	}

	if (!tradeApi) {
		tradeApi = new alpaca::Tradeapi;
		tradeApi->init(endPoint, keyID, secretKey);
	}
	cprintf("Alpaca  getting account activities of %d-%d-%d...\n", year, mon, day);
	std::vector<alpaca::Activity> jActivities;
	jActivities = tradeApi->list_activities(year, mon, day);
	if (jActivities.size() == 0) return;

	cprintf("Export Activity of %d-%02d-%02d\n", year, mon, day);
	FILE* f = fopen(filename, "w");
	for (auto act : jActivities) {
		//updateOneOrder(order.id, order.status, order.action, order.symbol, order.limit_price, order.qty, order.filled_qty, order.qty - order.filled_qty, order.limit_price);
		fprintf(f, "%s,%s,%s,%g,%g\n", act.transaction_time.c_str(), act.symbol.c_str(), act.side.c_str(), act.qty, act.price);
	}
	fclose(f);

	sprintf_s(filename, "%s%04d.csv", ALPACA_ACTIVITY_FOLDER, year);

	p = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (p != INVALID_HANDLE_VALUE) {
		FILETIME t;
		SYSTEMTIME stUTC, stLocal;
		GetFileTime(p, NULL, NULL, &t);
		CloseHandle(p);
		FileTimeToSystemTime(&t, &stUTC);
		SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
		if (stLocal.wYear > year || (stLocal.wYear == year && stLocal.wMonth > mon) || (stLocal.wYear == year && stLocal.wMonth == mon && stLocal.wDay >= day)) return;
	}

	cprintf("Append Activity of %d-%02d-%02d to the yearly file\n", year, mon, day);
	f = fopen(filename, "a");
	for (auto act : jActivities) {
		//updateOneOrder(order.id, order.status, order.action, order.symbol, order.limit_price, order.qty, order.filled_qty, order.qty - order.filled_qty, order.limit_price);
		fprintf(f, "%s,%s,%s,%g,%g\n", act.transaction_time.c_str(), act.symbol.c_str(), act.side.c_str(), act.qty, act.price);
	}
	fclose(f);
}

//critical session when update account with orders and positions.
void AlpacaAccount::updateAccount()
{
	bool print = false;
	if (!tradeApi) {
		tradeApi = new alpaca::Tradeapi;
		tradeApi->init(endPoint, keyID, secretKey);
	}

	cprintf("Alpaca  clearing all positions, orders and account updating messages...\n");
	for (AlpacaOrder* p : orders) delete p;
	orders.clear();
	messageQueue.clear();

	cprintf("Alpaca  getting account positions and orders...\n");

	std::vector<alpaca::Position> jPositions;
	std::vector<alpaca::Order> jOrders;
	//when get the information, there should have no orders get executed.
	int trycount = 0;
	for (;;) {
		jPositions = tradeApi->list_positions();
		jOrders = tradeApi->list_orders("open", 500);
		/*
		catch (const std::exception& e) {
			rprintf("Alpaca Update Account Error %s\n", e.what());
			delete tradeApi; tradeApi = NULL;
			return;
		}
		*/
		if (!tradeApi->connected) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));  //wait for 1 second
			tradeApi->connected = true;
			continue;
		}
		if (jPositions.size() == 0 && trycount < 300) {
			rprintf("No positions? I will try to get positions again after a while\n");
			std::this_thread::sleep_for(std::chrono::milliseconds(3000));  //wait for 3 second
			trycount++;
			continue;
		}
		if (!messageQueue.empty()) messageQueue.clear();
		else break;
	}


	cprintf("Postions\n");

	for (auto position : jPositions) {
		if (position.asset_class == "us_equity") {
			updateOnePosition(position.symbol, position.qty, position.avg_entry_price);
		}
	}
	for (auto p : positions) {
		if (p->shared && p->quanty == 0.0) write_Td_position(p->symbol, 0.0);
	}

	if (print) std::cout << "Orders\n";

	for (auto order : jOrders) {
		//std::cout << order.dump(4);
		updateOneOrder(order.id, order.status, order.action, order.symbol, order.limit_price, order.qty, order.filled_qty, order.qty - order.filled_qty, order.limit_price);
	}

	alpaca::Account acc = tradeApi->get_account();
	availableFunds = acc.buying_power;
	excessLiquidity = acc.equity;
	for (auto pos : positions) {
		pos->activeBuyOrder = pos->activeSellOrder = NULL;
	}
	ProcessMessages();
	read_shared_tick();
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	int hour = tm_struct->tm_hour;
	int minute = tm_struct->tm_min;
	if ((hour == 9 && minute >= 30) || hour == 10 || hour == 11 || hour == 15 || hour == 14) //busiest trading hours
	{
		for (auto order : orders) {
			if (order->status == "new") {
				if (order->action[0] == 'b') {
					getPosition(order->symbol).activeBuyOrder = order;
				}
				else {
					getPosition(order->symbol).activeSellOrder = order;
				}
			}
		}
	}
}

void AlpacaAccount::updatePositions()
{
	bool print = false;
	if (!tradeApi) {
		tradeApi = new alpaca::Tradeapi;
		tradeApi->init(endPoint, keyID, secretKey);
	}
	std::vector<alpaca::Position> jPositions;

	cprintf("Alpaca  getting account positions only...\n");

	//when get the information, there should have no orders get executed.
	jPositions = tradeApi->list_positions();
	/*
	catch (const std::exception& e) {
		rprintf("Alpaca Update Positions Error %s\n", e.what());
		delete tradeApi; tradeApi = NULL;
		return;
	}
	*/
	cprintf("Postions\n");

	for (auto position : jPositions) {
		if (position.asset_class == "us_equity") {
			updateOnePosition(position.symbol, position.qty, position.avg_entry_price);
		}
	}
	cprintf("Ending positions update\n");
}


//does not work for the price quote. Do not use.
void AlpacaAccount::requestMarketData_nouse()
{
}

void AlpacaAccount::wangJob()
{
//	exportActivities(2021, 11, 26);
	static int count = 0;
	static time_t updatePositionsTime = time(NULL);
	static time_t now;
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	int hour = tm_struct->tm_hour;
	int minute = tm_struct->tm_min;
	int second = tm_struct->tm_sec;
	ProcessControlFile();
	UpdateControlFile();
	if (color != INDIVIDUALACCCOLOR) {
		if (hour < 12) {
			m_canBuy = false; m_canSell = true;
		}
		else {
			m_canBuy = true; m_canSell = false;
		}
	}

	if (pingpang >= 0) pingpang++;
	if (pingpang > 5 * 10) {  //10 seconds
		rprintf("Stream connection broken\n");
		disconnect();
		return;
	}

	read_shared_tick();
//	if (true) {  //out of trading hours: empty order list, and just wait...
	if (hour >= 20 || hour < 4 || (hour == 4 && minute < 1)) {  //out of trading hours: empty order list, and just wait...
		cprintf("Alpaca current time is %d:%d\n", hour, minute);
		if (!orders.empty() && hour < 18) cancelAllOrders();
		for (auto p : positions) p->activeBuyOrder = p->activeSellOrder = NULL;
		for (auto order : orders) {
			if (order->status != "filled"&& order->status != "canceled"&& order->status != "rejected"&& order->status != "replaced"&& order->status != "expired") {
				order->status = "expired";
				shareOrderInfo(order);
			}
		}
		orders.clear();
		if (hour >= 20) {
			clearSharedTickAndOrders();
			for (auto p : positions) p->totalProfit = p->totalCommission = p->totalCash = p->totalShares = 0.0;
			for (auto order : orders) {
				delete order;
			}
			messageQueue.clear();
			if (tm_struct->tm_wday != 0 && tm_struct->tm_wday != 6 && minute >= 1) 
				exportActivities(tm_struct->tm_year+1900, tm_struct->tm_mon+1, tm_struct->tm_mday);
			/*
			for (int i = 6; i < 12; i++)
				for (int j = 1; j <= 31; j++)
					exportActivities(2021, i, j);
					*/
		}
		ProcessMessages();
		if (tradeApi) delete tradeApi;
		tradeApi = NULL;
		for (unsigned int i = 0; i < positions.size(); i++) {
			//output the one with different
			AlpacaPosition& s = *positions[i];
			if (s.stockIndex == -1) continue;
			if (fabs(s.targetBottomPrice - s.oldBottomPrice) > 0.001) {
				cprintf("Alpaca %s Bottom price different: %g, %g\n", s.symbol.c_str(), s.oldBottomPrice, s.targetBottomPrice);
			}
			if (fabs(s.targetTopPrice - s.oldTopPrice) > 0.001) {
				cprintf("Alpaca %s Top price different: %g, %g\n",s.symbol.c_str(),s.oldTopPrice,s.targetTopPrice);
			}
			if (fabs(s.targetMultiplier -s.oldMultiplier) > 0.01) {
				cprintf("Alpaca %s Multiplier different: %g, %g\n",s.symbol.c_str(),s.oldMultiplier,s.targetMultiplier);
			}
			if (fabs(s.targetOffset -s.oldOffset) > 0.1) {
				cprintf("Alpaca %s Offset different: %g, %g\n",s.symbol.c_str(),s.oldOffset,s.targetOffset);
			}
		}
		pingpang = 0;
		streamSession->sendping();
		std::this_thread::sleep_for(std::chrono::seconds(30));
		if (pingpang == 0) {
			rprintf("Stream connection broken\n");
			disconnect();
		}
		return;
	}

	while (!tradeApi) {
		disconnect();
		cprintf("Sleeping %u seconds before updating account information\n", 10);
		std::this_thread::sleep_for(std::chrono::seconds(10));
		cprintf("Alpaca  try to reset the account information...\n");
		updateAccount();
		connect();
	}
	ProcessMessages();
	read_shared_tick();
	/*
	{
		alpaca::Order o = tradeApi->get_order("572cc3d6-ea8d-45b6-b23d-9d8b27464e1c");
		while (o.replaced_by != "")
			o = tradeApi->get_order(o.replaced_by);
	}
	*/
//	std::this_thread::sleep_for(std::chrono::seconds(30));
//	return;

	//update topprice, bottomprice, etc.
	for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
		AlpacaPosition & s = *positions[stockindex];
		if (s.stockIndex == -1) continue; //Not in the all stock list.
		double rate = 0;
		if (s.startTime < s.endTime) rate = (hour * 60 + minute - s.startTime + tm_struct->tm_sec / 10 * 10 / 60.0) / (s.endTime - s.startTime);
		if (rate > 1.0) rate = 1.0;
		if (rate < 0) rate = 0;
		s.topPrice = s.oldTopPrice + (s.targetTopPrice - s.oldTopPrice) * rate;
		s.bottomPrice = s.oldBottomPrice + (s.targetBottomPrice - s.oldBottomPrice) * rate;
		s.multiplier = s.oldMultiplier + (s.targetMultiplier - s.oldMultiplier) * rate;
		//if (s.multiplier < 0.01) s.multiplier = 0.01;
		if (s.topPrice <= s.bottomPrice) s.topPrice = s.bottomPrice + 0.02;
		s.offset = s.oldOffset + (s.targetOffset - s.oldOffset) * rate;
	}

	if (!(hour == 9 && minute >= 29)) {
		if (time(NULL) - updatePositionsTime > 3600) {  //every one hour
			cprintf("Every 1 hour, update positions...\n");
			updatePositions(); //every one hour update positions.
			updatePositionsTime = time(NULL);
			//CopyFile("C:\\TradingRobot\\trading.csv", "C:\\Google Drive\\stock-tax\\TradingRobot\\trading.csv", false);
		}
		else if (lastPositionChangeTime >= updatePositionsTime && lastPositionChangeTime - updatePositionsTime <= 3) {
			cprintf("Within 3 seconds, there is positions change, for security, we update positions again...\n");
			updatePositions(); //every one hour update positions.
			updatePositionsTime = time(NULL);
		}
	}

	// every 20 seconds: Update orders and positions, ProfitTakenAdjust, change the orders +/-1%, to see if it is ok.
	if (count == 0 || time(NULL) - now > 20) {
		count++;
		now = time(NULL);

		for (int i = orders.size() - 1; i >= 0; i--) {
			AlpacaOrder* o = orders[i];
										//		"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",			
			if (o->filledQty == 0 && (o->status == "canceled" || o->status == "expired" || o->status == "rejected" || o->status == "replaced")) {
				delete o;
				orders.erase(orders.begin() + i);
			}
			if (o->status == "pending_cancel") {
				cprintf("order %s in pending_cancel status, try to update it order sttus\n", o->orderId.c_str());
				getOrderInfo(o);
			}
		}

		//if (color == INDIVIDUALACCCOLOR) outputBuffer(TD_OUTPUTILENAME1);

		if (count % 6 == 0) {  //every 2 minutes
			printPositions();
			double hedgedAmount = 0.0;
			for (unsigned int index = 0; index < positions.size(); index++) {
				AlpacaPosition& s = *positions[index];
				if (s.stockIndex == -1) continue;
				hedgedAmount += s.quanty * (s.askPrice() + s.bidPrice()) * 0.5 * s.hedgeRatio;
			}
			cprintf("Alpaca Total hedging amount that still needed is %g (wan)\n", round(hedgedAmount * 0.0001, 0.01));
			printf("Check connection... send ping\n");
			pingpang = 0;
			streamSession->sendping();
		}

		bool modified = false;
		for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
			AlpacaPosition& s = *positions[stockindex];
			if (s.shared) write_Td_position(s.symbol, s.quanty);

			if (s.stockIndex == -1) continue; //Not in the all stock list.
			if (s.askPrice() < 1.0 || s.bidPrice() < 1.0) continue;
			if (hour >= 16 || hour < 9 || (hour == 9 && minute < 30)) {  //out of regular trading hours
				if (s.symbol.size() == 5 && (s.symbol[4] == 'Y' || s.symbol[4] == 'F')) continue;    // do not trade OTC stocks out of regular trading hours.
			}

			if (hour == 10 || hour == 11 || hour == 15 || hour == 14 || (hour == 9 && minute >= 28)) {  //busiest trading hours, do not do that.
				continue;
			}

			//any repeating patterns
			if (repeatPatterns(s.actions) && difftime(time(NULL), s.actions[0].t) < 30) continue;

			for (unsigned int i = 0; i < orders.size(); i++) {
				AlpacaOrder* o = orders[i];
				if (o->symbol != s.symbol) continue;
//				"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",
//				"instruction": "'BUY' or 'SELL' or 'BUY_TO_COVER' or 'SELL_SHORT' or 'BUY_TO_OPEN' or 'BUY_TO_CLOSE' or 'SELL_TO_OPEN' or 'SELL_TO_CLOSE' or 'EXCHANGE'",

				if (o->status == "new" && (o->remaining == o->totalQty || o->remaining * o->lmtPrice > 8000)) {
					if (o->action[0] == 'b' && o->lmtPrice >= s.bidPrice() - 0.001 && s.bidSize() == floor(o->remaining / 100)) {
						modifyLmtOrder(o->orderId, o->remaining, round(o->lmtPrice * 0.99, 0.01));
						s.newOrderSettled = false;
						modified = true;
					}
					else if (o->action[0] == 's' && o->lmtPrice <= s.askPrice() + 0.001 && s.askSize() == floor(o->remaining / 100)) {
						modifyLmtOrder(o->orderId, o->remaining, round(o->lmtPrice * 1.01, 0.01));
						s.newOrderSettled = false;
						modified = true;
					}
				}
			}
		}
		if (modified) {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		//wangExportControlFile();
	}

	/* China Stock real time information retrieval. I comment it since I do not trade it now.
	if (count % 300 == 1) { //adjust this number 300
		for (int stockindex = 0; stockindex < m_stocknum; stockindex++) {
			Stock & s = m_stock[stockindex];
			if (s.currency == "CNH") {
				getChinaStockPrice(s.symbol.c_str(), s.askPrice, s.bidPrice, s.askSize, s.bidSize, s.lastPrice, s.dayHigh, s.dayLow, s.closePrice, s.dayVolume);
				cprintf("%s Price is %g<%d>:%g<%d>\n", s.symbol.c_str(), s.askPrice, s.askSize, s.bidPrice, s.bidSize);
			}
		}
	}
	*/

	ProcessMessages();
	read_shared_tick();


	//no more guess filled. the following code should not be executed.
	for (auto order : orders) {
		if (order->status == "GUESS_FILLED" && order->fillTime + 1 < msgTimeSecs) {  // guess filled order is actually not filled.
			if (order->action[0] == 'b') {
//				if (order->lmtPrice < getPosition(order->symbol).bidPrice()-0.001) {
					rprintf("Guess filled order %s is actually not filled. \n", order->orderId.c_str());
					order->status = "new";
					order->fillTime = 0;
					addToPosition(order->symbol, "sell", order->remaining, order->lmtPrice);
					rprintf("Guess filled order %s reverted back to QUEUED\n", order->orderId.c_str());
					getPosition(order->symbol).activeBuyOrder = order;
//				}
			}
			else {
//				if (order->lmtPrice > getPosition(order->symbol).askPrice()+0.001) {
					rprintf("Guess filled order %s is actually not filled. \n", order->orderId.c_str());
					order->status = "new";
					order->fillTime = 0;
					addToPosition(order->symbol, "buy", order->remaining, order->lmtPrice);
					rprintf("Guess filled order %s reverted back to QUEUED\n", order->orderId.c_str());
					getPosition(order->symbol).activeSellOrder = order;
//				}
			}
		}
	}

	/*
	//check for fatTail 9:30 - 15:30
	if ((hour < 15 || (hour == 15 && minute < 30)) && (hour > 9 || (hour == 9 && minute > 30))) {
		for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
			AlpacaPosition& s = *positions[stockindex];
			if (s.stdVariation == 0.0) continue;
			if (fabs(s.targetBottomPrice - s.bottomPrice) > 0.005 || fabs(s.targetTopPrice - s.topPrice) > 0.005 || fabs(s.targetMultiplier - s.multiplier) > 0.001 || fabs(s.targetOffset - s.offset) > 0.005) continue;
			s.oldBottomPrice = s.bottomPrice; s.oldTopPrice = s.topPrice; s.oldMultiplier = s.multiplier; s.oldOffset = s.offset;
			double buyPrice = s.previousClose * (1 + s.stdVariation);
			double sellPrice = s.previousClose * (1 - s.stdVariation);
			double delta = round(s.multiplier * 100 / log(s.topPrice / s.bottomPrice) * log(1 + s.stdVariation) * 5);
			if (s.fatTailBuyEnbled && !s.fatTailBuyDone) {
				if (s.lastPrice() > buyPrice - 0.001 && s.askPrice() > buyPrice - 0.001) {
					s.targetOffset = s.offset + delta;
					s.startTime = hour * 60 + minute + 1;
					s.endTime = hour * 60 + minute + 1 + round(delta * buyPrice / 25000);
					if (s.endTime == s.startTime) s.endTime = s.startTime + 1;
					s.fatTailBuyDone = true;
					ExportControlFile();
					continue;
				}
			}
			if (s.fatTailShortEnbled && !s.fatTailShortDone) {
				if (s.lastPrice() < sellPrice + 0.001 && s.bidPrice() < sellPrice + 0.001) {
					s.targetOffset = s.offset - delta;
					s.startTime = hour * 60 + minute + 1;
					s.endTime = hour * 60 + minute + 1 + round(delta * sellPrice / 25000);
					if (s.endTime == s.startTime) s.endTime = s.startTime + 1;
					s.fatTailShortDone = true;
					ExportControlFile();
					continue;
				}
			}
		}
	}
	//after 3:30, close fatTail positions
	if (hour == 15 && minute > 30 && minute < 59) {
		for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
			AlpacaPosition& s = *positions[stockindex];
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
				ExportControlFile();
				continue;
			}
		}
	}
	*/



	for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
		AlpacaPosition& s = *positions[stockindex];
		if (s.stockIndex == -1) continue; //Not in the all stock list.
		if (s.askPrice() < 1.0 || s.bidPrice() < 1.0) continue;
		//if (s.previousClose == 0.0 || s.stdVariation == 0.0 || s.lowBoundDiscount > 0.5) continue;
		if (hour >= 16 || hour < 9 || (hour == 9 && minute < 30)) {  //out of regular trading hours
			if (s.symbol.size() == 5 && (s.symbol[4] == 'Y' || s.symbol[4] == 'F')) continue;    // do not trade OTC stocks out of regular trading hours.
		}
		if (hour == 10 || hour == 11 || hour == 15 || hour == 14 || (hour == 9 && minute >= 28)) //busiest trading hours
		{  
			if (s.newOrderSettled && time(NULL) - s.newOrderSettleTime < 2) //do nothing for 2 seconds   
				continue;
		}
		else {
			if (s.newOrderSettled && time(NULL) - s.newOrderSettleTime < 1) //do nothing for 1 seconds
				continue;
		}

		//any repeating patterns
		if (repeatPatterns(s.actions) && difftime(time(NULL), s.actions[0].t) < 5) continue;

		//get current buy order price and sell order price. Adjust bidPrice and askPrice.
		double currentBuyPrice = -1.0, currentBuyQty = 0;
		double currentSellPrice = -1.0, currentSellQty = 0;
		for (unsigned int i = 0; i < orders.size(); i++) {
			AlpacaOrder* o = orders[i];
			if (o->symbol != s.symbol) continue;
			if (o->status == "canceled") continue;
			if (o->status == "pending_cancel") continue;
			if (o->status == "filled") continue;
			if (o->status == "expired") continue;
			if (o->status == "rejected") continue;

			if (o->action[0] == 'b') {
				if (o->remaining > 0 && currentBuyPrice < 0) {
					currentBuyPrice = o->lmtPrice;
					currentBuyQty = o->remaining;
				}
			}
			else if (o->action[0] == 's') {
				if (o->remaining > 0 && currentSellPrice < 0) {
					currentSellPrice = o->lmtPrice;
					currentSellQty = o->remaining;
				}
			}
		}
		//do not count myself. Adjust bidPrice and askPrice.
		if (currentBuyPrice == s.bidPrice() && !s.reverse) {
			if (s.bidSize() * 100 <= currentBuyQty || s.bidSize() * 100 - currentBuyQty < 100) {
				s.bidPrice() -= 0.01;
			}
		}
		if (currentSellPrice == s.askPrice() && !s.reverse) {
			if (s.askSize() * 100 <= currentSellQty || s.askSize() * 100 - currentSellQty < 100) {
				s.askPrice() += 0.01;
			}
		}

		double bottomPrice = s.bottomPrice;		if (bottomPrice == 0) bottomPrice = 0.01;
		double topPrice = s.topPrice;
		double multiplier = s.multiplier;
		double offset = s.offset;
		double position = s.quanty;
		if (s.shared) {
			double ibqnt = 0;
			if (read_IB_position(s.symbol, ibqnt)) position += ibqnt;
			else continue;
		}
		double cPrice = (multiplier == 0) ? (s.askPrice() + s.bidPrice()) / 2 : priceFromDelta(position, bottomPrice, topPrice, multiplier, offset, s.periods);
		double spreadPercent; 
		if (hour == 9 && minute >= 28 && minute < 45) {  //9:28- 9:45
			spreadPercent = 0.4 / 200.0;
		}
		else if (hour == 9 && minute >= 45) {  //9:45 - 10:00
			spreadPercent = 0.3 / 200.0;
		}
		else if (hour == 10 && minute <= 30) { //10:00-10:30
			spreadPercent = 0.25 / 200.0;
		}
		else if (hour == 10 || hour == 11) {
			spreadPercent = 0.2 / 200.0;
		}
		else if (hour == 12 || hour == 13) {
			spreadPercent = 0.15 / 200.0;
		}
		else if (hour == 14 || hour == 15) {
			spreadPercent = 0.1 / 200.0;
		}
		else {
			spreadPercent = 0.15 / 200.0;
		}

		//spreadPercent = 0.05 / 200.0; //delete

		double profit = multiplier * 100 * spreadPercent * spreadPercent * cPrice;  //the profit for each round (one buy plus one sell)
		if (hour < 9 || hour >= 16 || (hour == 9 && minute < 28)) {// out of regular trading hours
			if (profit > 10) {
				profit = 10;
				spreadPercent = (multiplier == 0) ? 0.0001 : sqrt(profit / (multiplier * 100.0 * cPrice));  //the minimum spread to get the profit. 
				if (spreadPercent > 0.05) spreadPercent = 0.05;  //5% maximum
			}
			else if (profit < 4) {
				profit = 4;
				spreadPercent = (multiplier == 0) ? 0.0001 : sqrt(profit / (multiplier * 100.0 * cPrice));  //the minimum spread to get the profit. 
				if (spreadPercent > 0.05) spreadPercent = 0.05;  //5% maximum
			}
		}
		else if (hour == 9 && minute > 29 && minute <= 35) {// 5 minute starting trading hours
			profit = 10;
			spreadPercent = (multiplier == 0) ? 0.0001 : sqrt(profit / (multiplier * 100.0 * cPrice));  //the minimum spread to get the profit. 
			if (spreadPercent > 0.05) spreadPercent = 0.05;  //5% maximum
		}
		else {
			//s.takenProfit = 0.02; //delete
			if (profit > 10) {
				profit = 10;
				spreadPercent = (multiplier == 0) ? 0.0001 : sqrt(profit / (multiplier * 100.0 * cPrice));  //the minimum spread to get the profit. 
				if (spreadPercent > 0.05) spreadPercent = 0.05;  //5% maximum
			}
			else if (profit < s.takenProfit) {
				profit = s.takenProfit;
				spreadPercent = (multiplier == 0) ? 0.0001 : sqrt(profit / (multiplier * 100.0 * cPrice));  //the minimum spread to get the profit. 
				if (spreadPercent > 0.05) spreadPercent = 0.05;  //5% maximum
			}
		}

		double spread = spreadPercent * cPrice;
		double minDelta = ceil(profit / spread); double rate = minDelta / (profit / spread);   //increase profit so that delta to be an integer. 
		spread *= rate;
		if (s.currency == "CNH") {
			spreadPercent = 0.01;
			spread = round(spreadPercent * (s.askPrice() + s.bidPrice()) * 0.5 + 0.005, 0.01);
			minDelta = 30 / spread;
		}
		if (s.askPrice() > 0 && s.bidPrice() > 0 && (s.askPrice() - s.bidPrice() < (s.askPrice() + s.bidPrice()) * 0.5 * 0.25)) {  //the condition to submit oders.
			double buyPrice, buyDelta, sellPrice, sellDelta;

			if (s.reverse) {
				buyDelta = 0; buyPrice = 0.01;
				sellDelta = 0; sellPrice = 10000000;

				double posPrice = priceFromDelta(position, bottomPrice, topPrice, multiplier, offset, s.periods);
				double bidDelta = deltaCalculation(s.bidPrice(), bottomPrice, topPrice, multiplier, offset, s.periods);
				double askDelta = deltaCalculation(s.askPrice(), bottomPrice, topPrice, multiplier, offset, s.periods);
				if (bidDelta > position + 0.5 && (bidDelta - position) * (s.bidPrice() - posPrice) > s.takenProfit && fabs(posPrice - s.bidPrice()) > posPrice * 0.005) {
					buyPrice = s.bidPrice();
					buyDelta = bidDelta - position;
				}
				else if (askDelta < position - 0.5 && (position - askDelta) * (posPrice - s.askPrice()) > s.takenProfit && fabs(posPrice - s.askPrice()) > posPrice * 0.005) {
					sellPrice = s.askPrice();
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
				sellPrice = ceil((cPrice + spread) * 100) / 100;
				if (buyPrice > s.bidPrice() + 0.01) buyPrice = s.bidPrice() + 0.01; if (buyPrice > s.askPrice() - 0.011) buyPrice = s.askPrice() - 0.01;
				if (sellPrice < s.askPrice() - 0.01) sellPrice = s.askPrice() - 0.01; if (sellPrice < s.bidPrice() + 0.011) sellPrice = s.bidPrice() + 0.01;
				if (buyPrice > s.buyBelow) buyPrice = s.buyBelow;
				if (sellPrice < s.sellAbove) sellPrice = s.sellAbove;

				buyDelta = deltaCalculation(buyPrice, bottomPrice, topPrice, multiplier, offset, s.periods) - position; if (buyDelta < 0.5)  buyDelta = 0;
				sellDelta = position - deltaCalculation(sellPrice, bottomPrice, topPrice, multiplier, offset, s.periods); if (sellDelta < 0.5) sellDelta = 0;

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
					if (s.quanty > 0.01) {
						sellDelta = s.quanty;
						sellPrice = s.askPrice();
						buyDelta = 0; buyPrice = 0.01;
					}
					else {
						buyDelta = sellDelta = 0;
						buyPrice = 0.01; sellPrice = 10000000;
					}
					/*
					if (position - offset > 0.01) {
						sellDelta = position - offset;
						sellPrice = s.askPrice();
						buyDelta = 0; buyPrice = 0.01;
					}
					else if (position - offset < -0.01) {
						buyDelta = -(position - offset);
						buyPrice = s.bidPrice();
						sellDelta = 0; sellPrice = 10000000;
					}
					else {
						buyDelta = sellDelta = 0;
						buyPrice = 0.01; sellPrice = 10000000;
					}
					*/
				}
			}

			if (s.quanty < -0.01 && buyDelta > -s.quanty) buyDelta = -s.quanty;   //negative position
			else if (s.quanty > 0.01 && sellDelta > s.quanty) sellDelta = s.quanty;   //positive position
			else if (fabs(s.quanty) <= 0.01 && s.shared) {   //zero position
				sellDelta = 0;
			}
			else if (fabs(s.quanty) <= 0.01 && buyDelta > 0 && sellDelta > 0) {   //zero position
				//can not modified orders, keep it.
				for (unsigned int i = 0; i < orders.size(); i++) {
					AlpacaOrder* o = orders[i];
					if (o->symbol != s.symbol || !(o->status == "pending_replace" || o->status == "accepted" || o->status == "pending_new")) continue;
					if (o->action[0] == 'b') {
						sellDelta = 0;
						break;
					}
					else {
						buyDelta = 0;
						break;
					}
				}
				//if there is a buy order, or sell order keep it until sellPrice is very close to bidPrice.
				if (fabs(buyPrice - s.askPrice()) > fabs(sellPrice - s.bidPrice()) * 2+0.01) buyDelta = 0;
				else if (fabs(buyPrice - s.askPrice()) * 2 + 0.01 < fabs(sellPrice - s.bidPrice())) sellDelta = 0;
				else {
					for (unsigned int i = 0; i < orders.size(); i++) {
						AlpacaOrder* o = orders[i];
						if (o->symbol != s.symbol || o->status == "canceled" || o->status == "pending_cancel" || o->status == "filled" || o->status == "expired" || o->status == "rejected") continue;
						if (o->action[0] == 'b') {
							sellDelta = 0;
							break;
						}
						else {
							buyDelta = 0;
							break;
						}
					}
				}
			}

			if (!s.alpacaTrading || !m_alpacaAllTrading) {
				//no trading...
				buyDelta = 0;
				sellDelta = 0;
			} 
			else if (hour == 9 && minute*60+second >= 29*60+30 && minute <= 33) {  //from 9:29:30 to 9:34
				//no trading...
				buyDelta = 0;
				sellDelta = 0;
			}
			/*
			else if (hour == 9 && minute < 1) {  //from 9:00 to 9:01
				//no trading...
				buyDelta = 0;
				sellDelta = 0;
			}*/
			else {
				if (s.shared) {
					if ((s.quanty + 1.01) * s.askPrice() > s.maxAlpacaMarketValue *(106 - minute/5) / 100 || (position < -0.1 && s.quanty >-position + 0.1) || (position > 0.1 && s.quanty > position - 0.1)) {  //do not buy ; *3/3 is to stabalize it.
						buyDelta = 0;
					}
					if (buyDelta > round(s.maxAlpacaMarketValue*1.06 / s.askPrice() - s.quanty)) buyDelta = round(s.maxAlpacaMarketValue*1.06 / s.askPrice() - s.quanty);
					if (s.quanty < 0.1) {  //do not sell to negative.
						sellDelta = 0;
					}
				}
			}

			if (s.reverse && (hour < 9 || hour >= 16 || (hour == 9 && minute < 45) || (hour == 3 && minute == 59 && second > 30))) {
				buyDelta = 0;
				sellDelta = 0;
			}

			//check pending cancel orders
			if (sellDelta > 0.1 && s.quanty > 0.1) {
				int pending_cancel_sell_shares = 0;
				for (unsigned int i = 0; i < orders.size(); i++) {
					AlpacaOrder* o = orders[i];
					if (o->status == "pending_cancel" && o->action == "sell" && o->symbol == s.symbol)
						pending_cancel_sell_shares += o->totalQty - o->filledQty;
				}
				if (sellDelta > s.quanty - pending_cancel_sell_shares) 
					sellDelta = s.quanty - pending_cancel_sell_shares;
				if (sellDelta < 0.1) sellDelta = 0;
			}

			/*
			while (sellDelta < minDelta && sellPrice < topPrice) 
				sellPrice += 0.01;
				sellDelta = position - deltaCalculation(sellPrice, bottomPrice, topPrice, multiplier, offset); if (sellDelta < 1 && sellDelta > 0.2) sellDelta = 1;
			}

			while (buyDelta < minDelta && buyPrice > bottomPrice) {
				buyPrice -= 0.01;
				buyDelta = deltaCalculation(buyPrice, bottomPrice, topPrice, multiplier, offset) - position; if (buyDelta < 1 && buyDelta > 0.2)  buyDelta = 1;
			}
			*/
			if (buyDelta * buyPrice > 25000) buyDelta = round(25000 / buyPrice);  if (buyDelta < 0)  buyDelta = 0;
			if (sellDelta * sellPrice > 25000) sellDelta = round(25000 / sellPrice); if (sellDelta < 0) sellDelta = 0;

			bool toBuy = true;
			bool toSell = true;
			bool toModifyBuy = false; std::string modifyBuyOrderID = "";
			bool toModifySell = false; std::string modifySellOrderID = "";

			//determine which order to modify to buy or sell, or create new buy or sell order
			//And cancel all the other orders.
			for (unsigned int i = 0; i < orders.size(); i++) {
				AlpacaOrder* o = orders[i];
				if (o->symbol != s.symbol || o->status == "canceled" || o->status == "pending_cancel" || o->status == "filled" || o->status == "GUESS_FILLED" || o->status == "expired" || o->status == "rejected") continue;
				if (o->status != "new" && o->status != "accepted" && o->status != "pending_new" && o->status != "pending_cancel" && o->status != "pending_replace")
					rprintf("%s order %s\n", o->symbol.c_str(), o->status.c_str());  //print out special order status
				//				"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",
				//				"instruction": "'BUY' or 'SELL' or 'BUY_TO_COVER' or 'SELL_SHORT' ,

				if (o->action[0] == 'b') {  //Find the buy order
					if (buyDelta < 0.1 || toModifyBuy || !toBuy) {
						if (o->status == "new" || o->status == "accepted" || o->status == "pending_new") {
							cprintf("Extra Buying order to be cancelled\n");
							cancelOrder(o->orderId);
							ProcessMessages();
						}
						//toBuy = false; //not buy this time, wait for more guaranttee.
						continue;
					}
					double deltaDiff = fabs(buyDelta - o->remaining);
					if (toBuy && deltaDiff < 1 && round(o->lmtPrice * 100 - buyPrice * 100) == 0)   //same order, keep this order
					{
						if (o->status == "new") s.activeBuyOrder = o;
						toBuy = false;  //find the order to keep
					}
					else if (toModifyBuy == false && (o->status == "new" || o->status == "PENDING_ACTIVATION")) {  //change this order
						modifyBuyOrderID = o->orderId;
						toModifyBuy = true;
					}
					else if (o->status == "pending_replace" || o->status == "accepted" || o->status == "pending_new") {
						toBuy = false;
					}
					else {  //Others, just keep. For example, PENDING_CANCEL orders, etc.
						//toBuy = false;
					}
				}
				else if (o->action[0] == 's') {  //sell
					if (sellDelta < 0.1 || toModifySell || !toSell) {
						cprintf("Extra Selling order to be cancelled\n");
						cancelOrder(o->orderId);
						ProcessMessages();
						continue;
					}
					double deltaDiff = fabs(sellDelta - o->remaining);
					if (toSell && deltaDiff < 1 && round(o->lmtPrice * 100) == round(sellPrice * 100)) //same order, no new buy order
					{
						if (o->status == "new") s.activeSellOrder = o;
						toSell = false;  //find the order to keep
					}
					else if (toModifySell == false && (o->status == "new" || o->status == "PENDING_ACTIVATION")) {
						modifySellOrderID = o->orderId;
						toModifySell = true;
					}
					else if (o->status == "pending_replace" || o->status == "accepted" || o->status == "pending_new") {
						toSell = false;
					}
					else {  //Others, just keep. For example, PENDING_CANCEL orders, etc.
						//toSell = false;
					}
				}
			}

			if (buyDelta == 0) toBuy = false;
			if (sellDelta == 0) toSell = false;

			if (toBuy || toSell) {
				if (toBuy && buyDelta > 0.9 && buyPrice < s.buyBelow + 0.001 && buyPrice > s.buyAbove - 0.001 && s.canBuy && m_canBuy) {
					if (toModifyBuy) {
						modifyLmtOrder(modifyBuyOrderID, buyDelta, buyPrice);
					}
					else {
						placeOrder(s.symbol, "buy", buyPrice, buyDelta);
					}
					s.newOrderSettled = true;
					s.newOrderSettleTime = time(NULL);
				}

				if (position <= 0.01 && !s.canShort) {
					toSell = false;
				}
				if (toSell && sellDelta > 0.9 && sellPrice > s.sellAbove - 0.001 && sellPrice < s.sellBelow + 0.001 && s.canSell && m_canSell) {
					if (toModifySell) {
						modifyLmtOrder(modifySellOrderID, sellDelta, sellPrice);
					}
					else {
						placeOrder(s.symbol, "sell", sellPrice, sellDelta);
					}
					s.newOrderSettled = true;
					s.newOrderSettleTime = time(NULL);
				}

				if (s.activeBuyOrder) {
					cprintf("Current Active Buy Order is %s %s %s %g(%g) %s\n", s.activeBuyOrder->orderId.c_str(), s.activeBuyOrder->action.c_str(), s.activeBuyOrder->symbol.c_str(), s.activeBuyOrder->remaining, s.activeBuyOrder->lmtPrice, s.activeBuyOrder->status.c_str());
					/*
					if (s.activeBuyOrder->status == "pending_cancel" && GetTimeInSeconds() - s.activeBuyOrder->pendingCancelTime > 15) {  //more than 15 seconds
						cprintf("order %s in pending_cancel status more than 30 seconds, try to get it order sttus\n", s.activeBuyOrder->orderId.c_str());
						getOrderInfo(s.activeBuyOrder);
						if (s.activeBuyOrder->status == "pending_cancel") {  //still the same, update the pending cancel time for next 15 seconds
							s.activeBuyOrder->pendingCancelTime = GetTimeInSeconds();
						}
					}*/
				}
				else
					cprintf("No active buy order\n");

				if (s.activeSellOrder) {
					cprintf("Current Active Sell Order is %s %s %s %g(%g) %s\n", s.activeSellOrder->orderId.c_str(), s.activeSellOrder->action.c_str(), s.activeSellOrder->symbol.c_str(), s.activeSellOrder->remaining, s.activeSellOrder->lmtPrice, s.activeSellOrder->status.c_str());
					/*
					if (s.activeSellOrder->status == "pending_cancel" && GetTimeInSeconds() - s.activeSellOrder->pendingCancelTime > 15) {  //more than 15 seconds
						cprintf("order %s in pending_cancel status more than 30 seconds, try to get it order sttus\n", s.activeSellOrder->orderId.c_str());
						getOrderInfo(s.activeSellOrder);
						if (s.activeSellOrder->status == "pending_cancel") {  //still the same, update the pending cancel time for next 15 seconds
							s.activeSellOrder->pendingCancelTime = GetTimeInSeconds();
						}
					}
					*/
				}
				else
					cprintf("No active sell order\n");
				ProcessMessages();
				read_shared_tick();
			}
		}//end if
	}//end for all positions

	ProcessMessages();
	read_shared_tick();
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
}