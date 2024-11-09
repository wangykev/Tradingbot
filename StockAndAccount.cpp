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
#include "curl_utils.hpp"
#include "time_utils.hpp"

double GetTimeInSeconds()
{
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	return 3600.0*tm_struct->tm_hour + 60.0*tm_struct->tm_min + tm_struct->tm_sec;
}

void WangAccount::read_shared_tick()
{
	bool print = false;
	if (reading_tick_index > 0 && g_pTickMsg[reading_tick_index - 1].set == 0) //reset when the first program erase everything, it will not happen. 
		reading_tick_index = 0;

	while (g_pTickMsg[reading_tick_index].set == 1) {
		int index = -1;
		WangPosition* pPosition = NULL;
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
				if (price > 0 && pPosition->activeSellOrder && pPosition->activeSellOrder->status == "QUEUED" && pPosition->activeSellOrder->lmtPrice < price + 0.001) {  //we guess the order has been filled.
					cprintf("Order %s %s %s is guess filled, because sell order price %g <= bidPrice %g\n", pPosition->activeSellOrder->orderId.c_str(), pPosition->activeSellOrder->action.c_str(), pPosition->activeSellOrder->symbol.c_str(), pPosition->activeSellOrder->lmtPrice, price);
					pPosition->activeSellOrder->status = "GUESS_FILLED";
					pPosition->activeSellOrder->fillTime = GetTimeInSeconds();
				}
				if (print) printf("bidPrice = %g\n", price);
				break;
			case 2:
				s.askPrice = price;
				if (price > 0 && pPosition->activeBuyOrder && pPosition->activeBuyOrder->status == "QUEUED" && pPosition->activeBuyOrder->lmtPrice > price - 0.001) {  //we guess the order has been filled.
					cprintf("Order %s %s %s is guess filled, because buy order price %g >= askPrice %g\n", pPosition->activeBuyOrder->orderId.c_str(), pPosition->activeBuyOrder->action.c_str(), pPosition->activeBuyOrder->symbol.c_str(), pPosition->activeBuyOrder->lmtPrice, price);
					pPosition->activeBuyOrder->status = "GUESS_FILLED";
					pPosition->activeBuyOrder->fillTime = GetTimeInSeconds();
				}
				if (print) printf("askPrice = %g\n", price);
				break;
			case 4:
				s.lastPrice = price;
				if (price > 0 && pPosition->activeSellOrder && pPosition->activeSellOrder->status == "QUEUED" && pPosition->activeSellOrder->lmtPrice < price + 0.001) {  //we guess the order has been filled.
					cprintf("Order %s %s %s is guess filled, because order price %g <= lastPrice %g\n", pPosition->activeSellOrder->orderId.c_str(), pPosition->activeSellOrder->action.c_str(), pPosition->activeSellOrder->symbol.c_str(), pPosition->activeSellOrder->lmtPrice, price);
					pPosition->activeSellOrder->status = "GUESS_FILLED";
					pPosition->activeSellOrder->fillTime = GetTimeInSeconds();
				}
				else if (price > 0 && pPosition->activeBuyOrder && pPosition->activeBuyOrder->status == "QUEUED" && pPosition->activeBuyOrder->lmtPrice > price - 0.001) {  //we guess the order has been filled.
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
				pos->activeBuyOrder->status = "QUEUED";
			}
		}
		if (pos->activeSellOrder && pos->activeSellOrder->status == "GUESS_FILLED") {
			if (!getOrderInfo(pos->activeSellOrder)) {
				pos->activeSellOrder->status = "QUEUED";
			}
		}
	}
}




int WangAccount::cprintf(const char* format, ...)
{
	//set color
	static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, color);
	//print time tag
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	printf("%02d:%02d:%02d ", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);

	int result;
	va_list args;

	va_start(args, format);
	result = vprintf(format, args);
	va_end(args);

	SetConsoleTextAttribute(hConsole, 7);  //white
	return result;
}


std::string getXmlString(const char * xml, const char* tag)
{
	char buf[50]; buf[0] = '\0';
	char startTag[30], endTag[30];
	int taglen = strlen(tag);
	startTag[0] = endTag[0] = '<'; 
	endTag[1] = '/';
	strncpy(startTag + 1, tag, taglen); startTag[1 + taglen] = '>'; startTag[2 + taglen] = '\0';
	strncpy(endTag + 2, tag, taglen); endTag[2 + taglen] = '>'; endTag[3 + taglen] = '\0';
	
	const char * start = strstr(xml, startTag);
	if (start) {
		start += taglen + 2;
		const char* next = strstr(start, endTag);
		if (next) {
			if (next - start < 50) {
				strncpy(buf, start, next - start);
				buf[next - start] = '\0';
				return std::string(buf);
			}
		}
	}
	printf("Can not find tag %s\n", tag);
	return std::string("");
}

double getXmlTime(const char * xml, const char* tag)
{
	std::string times = getXmlString(xml, tag);
	if (times.find("T") != std::string::npos) {
		int year, mon, day, hour, min;
		double sec;   //2021-06-04T08:33:28.804-05:00
		int ret = sscanf_s(times.c_str(), "%4d-%2d-%2dT%2d:%2d:%lf-", &year, &mon, &day, &hour, &min, &sec);
		if (ret == 6) return (hour+1) * 3600.0 + min * 60.0 + sec;
		else return 0;
	}
	else return 0;
}

double getXmlValue(const char* xml, const char* tag)
{
	char buf[50]; buf[0] = '\0';
	char startTag[30], endTag[30];
	int taglen = strlen(tag);
	startTag[0] = endTag[0] = '<';
	endTag[1] = '/';
	strncpy(startTag + 1, tag, taglen); startTag[1 + taglen] = '>'; startTag[2 + taglen] = '\0';
	strncpy(endTag + 2, tag, taglen); endTag[2 + taglen] = '>'; endTag[3 + taglen] = '\0';

	const char* start = strstr(xml, startTag);
	if (start) {
		start += taglen + 2;
		const char* next = strstr(start, endTag);
		if (next) {
			if (next - start < 50) {
				strncpy(buf, start, next - start);
				buf[next - start] = '\0';
				return atof(buf);
			}
		}
	}
	printf("Can not find tag %s\n", tag);
	return 0.0;
}

void PushMessage(StreamMessage& msg)
{
	extern WangAccount* tdAccount1;
	if (tdAccount1 && tdAccount1->account_id == msg.aid) {
		EnterCriticalSection(&tdAccount1->critSec);
		tdAccount1->messageQueue.push_back(msg);
		LeaveCriticalSection(&tdAccount1->critSec);
	}
}

//it is not real time, maybe lagging
void RealTimeStreamCallback(int cb_type, int ss_type, unsigned long long timestamp, const char* armsg)
{
	/*
	if (cb_type == (int)tdma::StreamingCallbackType::listening_start) {
	}
	else if (cb_type == (int)tdma::StreamingCallbackType::listening_stop) {  //listening_stop
	}
	else if (cb_type == (int)tdma::StreamingCallbackType::data) {  //data
	}
	else if (cb_type == (int)tdma::StreamingCallbackType::request_response) {  //listening_stop
	}
	else if (cb_type == (int)tdma::StreamingCallbackType::notify) {  //listening_stop
	}
	else if (cb_type == (int)tdma::StreamingCallbackType::timeout) {  //listening_stop
	}
	else if (cb_type == (int)tdma::StreamingCallbackType::error) {  //listening_stop
	}
	*/
	//				"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",

	if (cb_type == 2 && ss_type == 20) {  //
		json jGets = json::parse(std::string(armsg));
		for (auto event : jGets) {
			if (event.contains("2")) {
				StreamMessage msg;
				//printf("--- From Call Back -----\n");
				if (std::string(event["2"]) == "OrderEntryRequest" || std::string(event["2"]) == "OrderRoute") { //new order placed
					msg.type = 0;
					std::string xml(event["3"]);
					msg.orderid = getXmlString(xml.c_str(), "OrderKey");
					msg.status = "QUEUED";
					msg.action = getXmlString(xml.c_str(), "OrderInstructions");  for (auto& c : msg.action) c = toupper(c);
					msg.sym = getXmlString(xml.c_str(), "Symbol");
					msg.price = getXmlValue(xml.c_str(), "Limit");
					msg.qnt = getXmlValue(xml.c_str(), "OriginalQuantity");
					msg.time = getXmlTime(xml.c_str(), "ActivityTimestamp");
					msg.aid = getXmlString(xml.c_str(), "AccountKey");
					PushMessage(msg);
				}
				else if (std::string(event["2"]) == "OrderCancelReplaceRequest") { //new order placed
					printf("OrderCancelReplaceRequest, pass the message\n");
					//std::cout << event.dump(4) << std::endl;
				}
				else if (std::string(event["2"]) == "OrderCancelRequest") { //new order placed
					printf("OrderCancelRequest, pass the message\n");
					//std::cout << event.dump(4) << std::endl;
				}
				else if (std::string(event["2"]) == "UROUT") {  //order cancelled
					msg.type = 1;
					std::string xml(event["3"]);
					msg.orderid = getXmlString(xml.c_str(), "OrderKey");
					msg.status = "CANCELED";
					msg.action = getXmlString(xml.c_str(), "OrderInstructions");  for (auto& c : msg.action) c = toupper(c);
					msg.sym = getXmlString(xml.c_str(), "Symbol");
					msg.price = getXmlValue(xml.c_str(), "Limit");
					msg.qnt = getXmlValue(xml.c_str(), "OriginalQuantity");
					msg.canceledQnt = getXmlValue(xml.c_str(), "CancelledQuantity");

					msg.aid = getXmlString(xml.c_str(), "AccountKey");
					PushMessage(msg);
				}
				else if (std::string(event["2"]) == "OrderFill") {  //order filled
					msg.type = 2;
					std::string xml(event["3"]);
					msg.orderid = getXmlString(xml.c_str(), "OrderKey");
					msg.status = "FILLED";
					msg.action = getXmlString(xml.c_str(), "OrderInstructions");  for (auto& c : msg.action) c = toupper(c);
					msg.sym = getXmlString(xml.c_str(), "Symbol");
					msg.price = getXmlValue(xml.c_str(), "Limit");
					msg.qnt = getXmlValue(xml.c_str(), "OriginalQuantity");
					msg.filled = getXmlValue(xml.c_str(), "Quantity");
					msg.execPrice = getXmlValue(xml.c_str(), "ExecutionPrice");

					msg.time = getXmlTime(xml.c_str(), "ActivityTimestamp");
					msg.aid = getXmlString(xml.c_str(), "AccountKey");
					PushMessage(msg);
				}
				else if (std::string(event["2"]) == "OrderPartialFill") {  //order partially filled
					msg.type = 3;
					std::string xml(event["3"]);
					msg.orderid = getXmlString(xml.c_str(), "OrderKey");
					msg.status = "QUEUED";
					msg.action = getXmlString(xml.c_str(), "OrderInstructions");  for (auto& c : msg.action) c = toupper(c);
					msg.sym = getXmlString(xml.c_str(), "Symbol");
					msg.price = getXmlValue(xml.c_str(), "Limit");
					msg.qnt = getXmlValue(xml.c_str(), "OriginalQuantity");
					msg.filled = getXmlValue(xml.c_str(), "Quantity");
					msg.remaining = getXmlValue(xml.c_str(), "RemainingQuantity");
					msg.execPrice = getXmlValue(xml.c_str(), "ExecutionPrice");

					msg.time = getXmlTime(xml.c_str(), "ActivityTimestamp");
					msg.aid = getXmlString(xml.c_str(), "AccountKey");
					PushMessage(msg);
				}
				else if (std::string(event["2"]) == "OrderRejection") {  //REJECTED
					msg.type = 4;
					std::string xml(event["3"]);
					msg.orderid = getXmlString(xml.c_str(), "OrderKey");
					msg.status = "REJECTED";
					msg.action = getXmlString(xml.c_str(), "OrderInstructions");  for (auto& c : msg.action) c = toupper(c);
					msg.sym = getXmlString(xml.c_str(), "Symbol");
					msg.price = getXmlValue(xml.c_str(), "Limit");
					msg.qnt = getXmlValue(xml.c_str(), "OriginalQuantity");

					msg.time = getXmlTime(xml.c_str(), "ActivityTimestamp");
					msg.aid = getXmlString(xml.c_str(), "AccountKey");
					msg.rejectionReason = getXmlString(xml.c_str(), "RejectReason") == "REJECT_LOCATE" ? 1 : 0;
					PushMessage(msg);
				}
				else if (std::string(event["2"]) == "TooLateToCancel") { //too late to cancel. Found this once: replace an order, first order was "too late to cancel", second order is rejected.
					msg.type = 5;
					std::string xml(event["3"]);
					msg.orderid = getXmlString(xml.c_str(), "OrderKey");
					msg.status = "QUEUED";
					msg.action = getXmlString(xml.c_str(), "OrderInstructions");  for (auto& c : msg.action) c = toupper(c);
					msg.sym = getXmlString(xml.c_str(), "Symbol");
					msg.price = getXmlValue(xml.c_str(), "Limit");
					msg.qnt = getXmlValue(xml.c_str(), "OriginalQuantity");

					msg.time = getXmlTime(xml.c_str(), "ActivityTimestamp");
					msg.aid = getXmlString(xml.c_str(), "AccountKey");
					PushMessage(msg);
				}
				else if (std::string(event["2"]) == "OrderExpired") {  //REJECTED
					msg.type = 6;
					std::string xml(event["3"]);
					msg.orderid = getXmlString(xml.c_str(), "OrderKey");
					msg.status = "EXPIRED";
					msg.action = getXmlString(xml.c_str(), "OrderInstructions");  for (auto& c : msg.action) c = toupper(c);
					msg.sym = getXmlString(xml.c_str(), "Symbol");
					msg.price = getXmlValue(xml.c_str(), "Limit");
					msg.qnt = getXmlValue(xml.c_str(), "OriginalQuantity");

					msg.time = getXmlTime(xml.c_str(), "ActivityTimestamp");
					msg.aid = getXmlString(xml.c_str(), "AccountKey");
					PushMessage(msg);
				}
				else if (std::string(event["2"]) == "TransactionTrade") {  //Transaction trade
					printf("Transaction trade\n");
				}
				else {
					printf("----unknown message type1:%d, type2:%d------\n", cb_type, ss_type);
					std::cout << json::parse(std::string(armsg)).dump(4) << std::endl;
					printf("----end unknown message--\n");
				}
			}
		}
	}
	else if (cb_type == 4) {
		static time_t t = time(NULL);
		if (time(NULL) - t > 30) printf("no heartbeat for 30 seconds\n");
		t = time(NULL);
	}
	else printf("Unknown: %s\n", armsg);
}

bool WangAccount::updateReplacedToOrders(WangOrder * p, double partialFill)
{
	if (p->replacedto == "") return true;
	for (auto order : orders) {
		if (order->orderId == p->replacedto) {

			rprintf("Because the parent order %s partial fill %g, make child order %s qnty %g to be partially filled.\n", p->orderId.c_str(), partialFill, order->orderId.c_str(), order->totalQty);
			order->partiallyFilled = true;
			if (order->status == "FILLED") {
				//rprintf("Order %s is filled, the quanty has already been counted in the positions, please double check for errors\n", order->orderId.c_str());
				return true;
			}
			return updateReplacedToOrders(order, partialFill);
		}
	}
	return true;
}

void WangAccount::zeroActiveOrders(WangOrder * o)
{
	if (!o) return;
	WangPosition & p = getPosition(o->symbol);
	if (p.activeBuyOrder == o) p.activeBuyOrder = NULL;
	else if (p.activeSellOrder == o) p.activeSellOrder = NULL;
}

//remember that Messages update account may be lagging.
void WangAccount::ProcessMessages()
{
	while (!messageQueue.empty()) {
		StreamMessage& msg = messageQueue.front();
		if (account_id == msg.aid) {
			WangOrder* p = NULL;
			for (auto order : orders) {
				if (order->orderId == msg.orderid) {
					p = order;
					break;
				}
			}

			if (msg.type == 0) { //new order placed
				if (p) {
					p->action = msg.action;
					p->lmtPrice = msg.price;
					p->avgFillPrice = msg.price;
					if (p->status == "ACCEPTED") p->status = "QUEUED";
					cprintf("TD confirms a new order: %s, %s %s %g %g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), msg.sym.c_str(), p->lmtPrice, p->totalQty, p->remaining, p->status.c_str());
					if (p->totalQty != msg.qnt) cprintf("Order is replaced from a partial filled order, discard the original quantity information\n");
					//no status change
					//if (p->status != "PENDING_CANCEL" && p->status != "CANCELED"  && p->status != "REJECTED" && p->status != "FILLED" && p->status != "GUESS_FILLED") p->status = msg.status;   //may before this update, already being replaced.
					//nothing get executed, no position effect.
				}
				else {
					rprintf("TD Update find a user input new order: %s, %s %s%g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), msg.sym.c_str(), msg.qnt, msg.price, msg.status.c_str());
					p = new WangOrder;
					p->orderId = msg.orderid;
					p->symbol = msg.sym;
					p->action = msg.action;
					p->lmtPrice = msg.price;
					p->status = "QUEUED";
					p->remaining = p->totalQty = msg.qnt;     //may not correct!!!
					p->filledQty = 0; 
					p->avgFillPrice = msg.price;
					rprintf("TD User Manually input a new order: %s, %s %g %g(%g)status: %s\n", msg.orderid.c_str(), msg.action.c_str(), p->lmtPrice, p->totalQty, p->remaining, p->status.c_str());
					rprintf("I do not know whether this is a replace of old order or it is just a new order\n");
					orders.push_back(p);
				}
			}
			else if (msg.type == 1) {  //order cancelled
				if (!p) rprintf("Cancelling a nonexisting order %s? Something must be wrong! Disconnected and not update?\n", msg.orderid.c_str());
				else {
					if (p->remaining > msg.canceledQnt - (msg.qnt - p->totalQty) + 0.01) {
						cprintf("cancelled order has some extra %g get filled\n", p->remaining - msg.canceledQnt + (msg.qnt - p->totalQty));
						updateOneOrder(msg.orderid, "CANCELED", msg.action, msg.sym, msg.price, p->totalQty, msg.qnt - msg.canceledQnt, p->totalQty + msg.canceledQnt - msg.qnt, p->lmtPrice);
						addToPosition(msg.sym, msg.action, p->remaining - msg.canceledQnt + (msg.qnt - p->totalQty), p->lmtPrice);
					}
					else if (p->remaining < msg.canceledQnt - (msg.qnt - p->totalQty) - 0.01) {
						rprintf("TD canceled quantity %g > remaining %g, check reasons\n", msg.canceledQnt - (msg.qnt - p->totalQty), p->remaining);
					}
					if (p->status == "FILLED" || p->status == "EXPIRED" || p->status == "REJECTED") rprintf("TD order cancelled from an impossible status %s\n", p->status.c_str());
					cprintf("TD confirms order %s is cancelled\n", msg.orderid.c_str());
					p->status = "CANCELED";
				}
				zeroActiveOrders(p);
			}
			else if (msg.type == 2) {  //order filled
				if (p) {
					if (p->totalQty < msg.qnt) cprintf("Original order has larger original quantity.\n");
					if (p->status == "FILLED") {
						cprintf("TD confirms a Filled order %s is really filled\n", p->orderId.c_str());
					}
					else {
						if (fabs(p->remaining - msg.filled) > 0.1) rprintf("Remaining not equal to Filled qnt, This may happen when the filled message arrive faster than the partially filled message.\n");
						updateOneOrder(msg.orderid, "FILLED", msg.action, msg.sym, msg.price, p->totalQty, p->totalQty, 0, msg.execPrice);
						addToPosition(msg.sym, msg.action, msg.filled, msg.execPrice);
					}
					zeroActiveOrders(p);
					//update the replacedto order to be rejected.
					for (auto order : orders) {
						if (order->orderId == p->replacedto) {
							zeroActiveOrders(order);
							order->status = "REJECTED";
							cprintf("Change the replaced to order %s to be REJECTED.\n", order->orderId.c_str());
							break;
						}
					}
				}
				else {
					rprintf("Can not find this order %s? This should not happen! Something must be wrong 1023.\n", msg.orderid.c_str());
				}
			}
			else if (msg.type == 3) {  //order partially filled
				if (p) {
					if (p->totalQty < msg.qnt) cprintf("Partially fill: Original order has larger original quantity.\n");
					if (p->status == "FILLED") {
						cprintf("TD confirms a Filled order %s is partilly filled\n", p->orderId.c_str());
					}
					else {
						if (p->remaining > msg.filled + msg.remaining + 0.01) rprintf("TD partially filled order, the remaining is too small. Something wrong? order remaining %g, this time filled %g, still remaining %g\n", p->remaining, msg.filled, msg.remaining);
						else if (p->remaining < msg.remaining + 0.01) cprintf("TD confirms this known partial fill, already remaining %g, this time remaining %g\n", p->remaining, msg.remaining);
						else {
							cprintf("TD find a partial fill\n");
							if (updateReplacedToOrders(p, msg.filled)) {
								updateOneOrder(msg.orderid, msg.status, msg.action, msg.sym, msg.price, p->totalQty, p->totalQty - msg.remaining, msg.remaining, msg.execPrice);
								addToPosition(msg.sym, msg.action, msg.filled, msg.execPrice);
							}
							else {
								rprintf("This partial filled has been counted in a filled order, so there is no update. \n");
							}
							p->partiallyFilled = true;
						}
					}
				}
				else {
					rprintf("This should not happen! Something must be wrong 1033.\n");
				}
			}
			else if (msg.type == 4) {  //order rejected
				if (!p) rprintf("Rejecting a nonexisting order %s? Possible for replace rejection.\n", msg.orderid.c_str());
				else {
					if (p->status != "FILLED") {
						rprintf("TD changing order %s is not successful, change its status from %s to REJECTED\n", msg.orderid.c_str(), p->status.c_str());
						p->status = "REJECTED";
						if (msg.rejectionReason == 1 && p->action[0] == 'S') {  //can not short
							rprintf("Can not short %s, change its canShort to false\n", msg.sym.c_str());
							getPosition(p->symbol).canShort = false;
						}
						else {
							//the replaced from order maybe filled.
							for (auto o : orders) {
								if (o->replacedto == msg.orderid) {
									if (o->status != "FILLED" && o->status != "REJECTED" && o->status != "CANCELED") {
										rprintf("Get the replaced from order %s status.\n", o->orderId.c_str());
										getOrderInfo(o);  //no more PENDING_REPLACE
									}
									break;
								}
							}
						}

					}
					else rprintf("TD order %s is FILLED, How can it be rejected.\n", msg.orderid.c_str());
				}
				zeroActiveOrders(p);
			}
			else if (msg.type == 5) {  //order too late to canel
				if (!p) rprintf("Cancelling a nonexisting order %s? Something must be wrong! Disconnected and not update?\n", msg.orderid.c_str());
				else {
					 rprintf("TD cancelling order %s is not successful, keep its status %s\n", msg.orderid.c_str(), p->status.c_str());
				}
			}
			else if (msg.type == 6) {  //order expired
				if (!p) rprintf("A nonexisting order %s expired? Something must be wrong! Disconnected and not update?\n", msg.orderid.c_str());
				else {
					cprintf("TD order %s is expired\n", msg.orderid.c_str());
					p->status = "EXPIRED";
				}
				zeroActiveOrders(p);
			}
		}

		if (msg.time > 0) {
			double ct = GetTimeInSeconds();
			msgTimeSecs = msg.time;
			if (msgTimeSecs < ct) {
				cprintf("message is lagging %g seconds\n", ct - msgTimeSecs);
				/*
				if (msgTimeSecs < ct - 5) {
					rprintf("Lagging is bigger than 5 seconds. Reset the QOS...");
					try {
						streamSession->set_qos(tdma::QOSType::express);
					}
					catch (tdma::StreamingException &e) {
						rprintf("Streaming Exception when setting QOS: %s\n", e.what());
					}
					rprintf("done!\n");
				}*/
			}
		}
		
		EnterCriticalSection(&critSec);
		messageQueue.pop_front();
		LeaveCriticalSection(&critSec);
	}
}

void WangAccount::connect()
{
	tdma::AcctActivitySubscription aSub;
	try {
		streamSession->start(aSub);
		streamSession->set_qos(tdma::QOSType::express);
	}
	catch (tdma::StreamingException &e) {
		rprintf("Streaming Exception when connect: %s\n", e.what());
	}
}

WangStock allStock[100];
int allStockNum = 0;

//insert a stock sym to allstock list.
int addOneStock(std::string sym, std::string type)
{
	static std::mutex g_mutex;
	g_mutex.lock();

	int i;
	for (i = 0; i < allStockNum; i++) {
		if (allStock[i].symbol == sym && allStock[i].type == type) {
			g_mutex.unlock();
			return allStock[i].tikerID;
		}
	}
	if (allStockNum == 100) {
		std::cout << "Total monitored stock number is more than 100. Can not add more stocks!";
		g_mutex.unlock();
		return -1;
	}
	else {
		allStock[allStockNum].symbol = sym;
		allStock[allStockNum].type = type;
		allStock[allStockNum].tikerID = 12001 + allStockNum;
		allStockNum++;
		g_mutex.unlock();
		return 12001 + allStockNum - 1;
	}
}

void read_shared_symbol_to_allStock()
{
	static int readnumber = 0;
	while (readnumber < g_pSymboList->size) {
		char sym[10];
		strncpy(sym, g_pSymboList->sym[readnumber], 9);
		sym[9] = '\0';
		char* exchange = strstr(sym, "@"); 
		if (exchange) {
			exchange[0] = '\0';
			exchange++;
		}
		int i;
		for (i = 0; i < allStockNum; i++) {
			if (allStock[i].symbol == sym) break;
		}
		if (i == allStockNum) {
			addOneStock(sym);
			int index = getStockIndex(sym);
			if (exchange && index != -1) allStock[index].primaryExchange = exchange;
		}
		readnumber++;
	}
}


//combine position with all stock. 
//return -1 if it is not in allstock, which was get real time streaming quote from IB.
int getStockIndex(std::string sym, std::string type)
{
	int i;
	for (i = 0; i < allStockNum; i++) {
		if (allStock[i].symbol == sym && allStock[i].type == type) return i;
	}
	return -1;
}

inline std::string trim(std::string& str)
{
	str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
	str.erase(str.find_last_not_of('\r') + 1);         //surfixing spaces
	str.erase(str.find_last_not_of(' ') + 1);         //surfixing spaces
	return str;
}

bool WangAccount::scanValue(std::string& line, const char* targetString, double& value1, double& value2) {
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

bool WangAccount::scanValue(std::string& line, const char* targetString, double& value) {
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

bool WangAccount::scanValue(std::string& line, const char* targetString, bool& value) {
	double tmp;
	if (scanValue(line, targetString, tmp)) {
		value = (tmp != 0);
		return true;
	}
	else return false;
}

bool WangAccount::scanValue(std::string& line, const char* targetString, std::string& value) {
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

void WangAccount::UpdateControlFile()
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

	//all else need update ControlFile.
	char line[200];
	std::string s = "ALL:\n";
	sprintf(line, "buySell = %d, %d\n", m_canBuy ? 1 : 0, m_canSell ? 1 : 0);
	s += line;
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
				}
				else if (todayClose / previousClose - 1.0 > topBound) {
					fatChange = todayClose / previousClose / (1 + topBound) - 1.0;
				}
				pos->topPrice *= (1 + fatChange);
				pos->bottomPrice *= (1 + fatChange);
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
					}
					else if (previousClose / pos->previousClose - 1.0 > topBound) {
						fatChange = previousClose / pos->previousClose / (1 + topBound) - 1.0;
					}
					pos->topPrice *= (1 + fatChange);
					pos->bottomPrice *= (1 + fatChange);
					pos->previousClose = previousClose;
					//pos->stdVariation = (volatility100 + volatility30) / 2;
				}
			}

			/*  Not use topPrice and bottomPrice to cap.
			if (pos->topPrice < pos->previousClose) {
				pos->bottomPrice = pos->bottomPrice / pos->topPrice * pos->previousClose;
				pos->topPrice = pos->previousClose;
			}
			else if (pos->bottomPrice > pos->previousClose) {
				pos->topPrice = pos->topPrice / pos->bottomPrice * pos->previousClose;
				pos->bottomPrice = pos->previousClose;
			}*/
		}

		if (allStock[pos->stockIndex].primaryExchange == "")
			sprintf(line, "\n%s:\n", pos->symbol.c_str());
		else
			sprintf(line, "\n%s@%s:\n", pos->symbol.c_str(), allStock[pos->stockIndex].primaryExchange.c_str());
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
		sprintf(line, "hedgeRatio = %g\n", pos->hedgeRatio);
		s += line;
		sprintf(line, "takenProfit = %g\n", pos->takenProfit);
		s += line;
		sprintf(line, "maxAlpacaMarketValue = %g\n", pos->maxAlpacaMarketValue);
		s += line;
		sprintf(line, "buySell = %d, %d\n", pos->canBuy ? 1 : 0, pos->canSell ? 1 : 0);
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


void WangAccount::ProcessControlFile()
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
	m_alpacaTrading = true;
	unsigned int i;
	for (WangPosition *p:positions) {
		WangPosition& s = *p;
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
		s.canShort = false;   //canShort means that when long term account holding some position the short term account can short it. 
	}

	int stockIndex = -1;

	std::istringstream iss(pContent);  	delete[] pContent;
	WangPosition pos;
	WangPosition* pPos = &pos;

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
				WangPosition* p = new WangPosition;
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
		else if (scanValue(line, "alpacaTrading ", m_alpacaTrading)) continue;
		else if (scanValue(line, "hedgeRatio ", pPos->hedgeRatio)) continue;
		else if (scanValue(line, "takenProfit", pPos->takenProfit)) continue;
		else if (scanValue(line, "previousClose ", pPos->previousClose)) continue;
		else if (scanValue(line, "stdVariation ", pPos->stdVariation)) continue;
		else if (scanValue(line, "lowBoundDiscount ", pPos->lowBoundDiscount)) continue;
		else if (scanValue(line, "buyBelow", pPos->buyBelow)) continue;
		else if (scanValue(line, "sellAbove", pPos->sellAbove)) continue;
		else if (scanValue(line, "buyAbove", pPos->buyAbove)) continue;
		else if (scanValue(line, "sellBelow", pPos->sellBelow)) continue;
		else if (scanValue(line, "canShort", pPos->canShort)) continue;
		else if (scanValue(line, "shared", pPos->shared)) continue;
		else if (scanValue(line, "maxAlpacaMarketValue", pPos->maxAlpacaMarketValue)) continue;
	}

	cprintf("*********TD Ameritrade***********\n");
	for (i = 0; i < positions.size(); i++) {
		WangPosition& stock = * positions[i];
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
	cprintf("*********TD Ameritrade***********\n");
	if (isConnected()) {
		cprintf("update shared postions\n");
		for (auto p : positions) {
			if (p->shared) write_Td_position(p->symbol, p->quanty);
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1000)); //wait for other process reading its file a little.
}

WangPosition & WangAccount::getPosition(std::string sym)
{
	unsigned int i;
	for (i = 0; i < positions.size(); i++) {
		WangPosition& stock = *positions[i];
		if (stock.symbol == sym) return stock;
	}
	rprintf("Something wrong, can not find the stock!\n");
	return *(new WangPosition);  
}

void WangAccount::cancelOrder(std::string orderId)
{
	extern std::vector<int> soundPlay;
	if (!pCManager) return;
	std::cout << "TD Cancel Order ID: " << orderId << std::endl;
	bool success;
	try {
		success = tdma::Execute_CancelOrder(pCManager->credentials, account_id, orderId);
	}
	catch (const std::exception& e) {
		std::cout << "TD Cancel Order Error" << e.what() << std::endl;
		delete pCManager; pCManager = NULL;
		return;
	}

	std::cout << "TD Cancel: " << std::boolalpha << success << std::endl;
	unsigned int i;
	for (i = 0; i < orders.size(); i++) {
		if (orders[i]->orderId == orderId) {
			if (success) {
				orders[i]->status = "PENDING_CANCEL";
				zeroActiveOrders(orders[i]);
				soundPlay.push_back(3);
			}
			else {
				getOrderInfo(orders[i]);
			}
			break;
		}
	}
}

void WangAccount::placeOrder(std::string sym, std::string action, double price, double qnt)
{
	if (!pCManager) return;

	double curPosition = round(getPosition(sym).quanty);
	bool isBuy = (action[0] == 'B');
	bool toOpen = (isBuy && (curPosition >= 0.0)) || (!isBuy && (curPosition <= 0.0));

	if (action[0] == 'B') {
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
	cprintf("TD to placed a new order: %s %s %d-%g(%g), curPosition=%g\n", action.c_str(), sym.c_str(), size, round(qnt), price, curPosition);

	tdma::OrderTicket order1 = tdma::SimpleOrderBuilder::Equity::Build(sym, size, isBuy, toOpen, price);
	if (sym.size() == 5 && (sym[4] == 'Y' || sym[4] == 'F')) order1.set_session(tdma::OrderSession::NORMAL);  //OTC stocks, some OTC stocks may not list here.
	else order1.set_session(tdma::OrderSession::SEAMLESS);
	std::string oid;
	try {
		oid = Execute_SendOrder(pCManager->credentials, account_id, order1);
	}
	catch (const std::exception& e) {
		std::string err(e.what());
		if (err.find("not available to borrow") != std::string::npos) {
			rprintf("There is no available to borrow, set the canShort to be false.\n");
			getPosition(sym).canShort = false;
			return;
		}
		else if (err.find("You do not have enough available cash/buying power for this order") != std::string::npos) {
			rprintf("You do not have enough available cash/buying power for this order, set the stock canBuy to false.\n");
			getPosition(sym).canBuy = false;
			return;
		}
		else if (err.find("validation problem") != std::string::npos) rprintf("Place order get validation problem: %s.\n", e.what());
		else {
			rprintf("TD Place Order Error %s\n", e.what());
			rprintf("Reset the Credential Manager...\n");
			delete pCManager; pCManager = NULL;
			return;
		}
	}

	cprintf("TD Placed a new order: %s, %s %s %d-%g(%g), curPosition=%g\n", oid.c_str(), action.c_str(), sym.c_str(), size, round(qnt), price, curPosition);
	unsigned int i;
	for (i = 0; i < orders.size(); i++) {
		if (orders[i]->orderId == oid) {
			break;
		}
	}
	if (i >= orders.size()) {
		WangOrder* p = new WangOrder;
		p->orderId = oid;
		p->symbol = sym;
		p->action = action;
		p->status = "QUEUED";
		p->lmtPrice = price;
		p->totalQty = size;
		p->filledQty = 0;
		p->remaining = size;
		p->avgFillPrice = price;

		orders.push_back(p);
		if (action[0] == 'B') getPosition(sym).activeBuyOrder = p;
		else getPosition(sym).activeSellOrder = p;
	}
}


void WangAccount::replaceOrder(std::string orderId, std::string sym, std::string action, double price, double qnt)
{
	if (!pCManager) return;

	double curPosition = round(getPosition(sym).quanty);
	bool isBuy = (action[0] == 'B');
	bool toOpen = (isBuy && (curPosition >= 0.0)) || (!isBuy && (curPosition <= 0.0));

	//the following should not needed, because there is processing in wangjob()
	if (action[0] == 'B') {
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
	tdma::OrderTicket order1 = tdma::SimpleOrderBuilder::Equity::Build(sym, size, isBuy, toOpen, price);
	if (sym.size() == 5 && (sym[4] == 'Y' || sym[4] == 'F')) order1.set_session(tdma::OrderSession::NORMAL);  //OTC stocks, some OTC stocks may not list here.
	else order1.set_session(tdma::OrderSession::SEAMLESS);
	std::string oid;
	try {
		oid = Execute_ReplaceOrder(pCManager->credentials, account_id, orderId, order1);
	}
	catch (const std::exception& e) {
		std::string err(e.what());
		if (err.find("Order cannot be replaced") != std::string::npos) rprintf("Can not replace the order %s, cancelled, filled or rejected.\n", orderId.c_str());
		else if (err.find("validation problem") != std::string::npos) rprintf("Replace order get 400 error, validation problem: %s.\n", orderId.c_str());
		else {
			rprintf("TD Replace Order Error: %s\n", e.what());
			delete pCManager; pCManager = NULL;
			return;
		}

		unsigned int i;
		for (i = 0; i < orders.size(); i++) {
			if (orders[i]->orderId == orderId) {
				if (orders[i]->status != "FILLED" && orders[i]->status != "REJECTED" && orders[i]->status != "CANCELED" && orders[i]->status != "PENDING_CANCEL") {
					getOrderInfo(orders[i]);  //no more PENDING_REPLACE
					//if (!getOrderInfo(orders[i])) orders[i]->status = "PENDING_REPLACE";   //use pending replace, let call back function update it.
					if (orders[i]->status == "QUEUED") {
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
	cprintf("TD Replaced order %s by order: %s, %s %s %d-%g(%g), curPosition=%g\n", orderId.c_str(), oid.c_str(), action.c_str(), sym.c_str(), size, qnt, price, curPosition);

	unsigned int i;
	for (i = 0; i < orders.size(); i++) {
		if (orders[i]->orderId == orderId) {
			orders[i]->status = "PENDING_CANCEL";   //use pending cancel, even it may be cancelled, let call back function update it.
			orders[i]->replacedto = oid;

			//create a new order
			WangOrder* p = new WangOrder;
			p->orderId = oid;
			p->symbol = sym;
			p->action = action;
			p->status = "QUEUED";
			p->lmtPrice = price;
			p->totalQty = size;
			p->remaining = size;
			p->filledQty = 0;
			p->avgFillPrice = price;

			orders.push_back(p);
			if (action[0] == 'B') getPosition(sym).activeBuyOrder = p;
			else getPosition(sym).activeSellOrder = p;
			break;
		}
	}
}


double deltaCalculation(double price, double bottomPrice, double topPrice, double multiplier, double offset, const std::vector<double>& periods);
double priceFromDelta(double delta, double bottomPrice, double topPrice, double multiplier, double offset, const std::vector<double>& periods);
double round(double, double);

void WangAccount::printPositions() 
{
#ifndef NO_OUTPUT
	cprintf("############Start TD Order List###########\n");
	for (unsigned int i = 0; i < orders.size(); i++) {
		WangOrder* o = orders[i];
		cprintf("%s %s %s %s <%s> %g(%g) @ %g\n", o->account.c_str(), o->orderId.c_str(), o->symbol.c_str(), o->action.c_str(), o->status.c_str(), o->totalQty, o->remaining, o->avgFillPrice);
	}
	cprintf("############End TD Order List###########\n");
#endif

	cprintf("-----TD Ameritrade -----\n");
	double totalProfit = 0;
	double totalCommission = 0;
	for (unsigned int index = 0; index < positions.size(); index++) {
		WangPosition& s = *positions[index];
		double ibpos = 0;
		if (s.shared) read_IB_position(s.symbol, ibpos);
		cprintf("Stock %s(%g): Shares %g, cash %g; Total Profit %g (%g) (IB position %g)\n", s.symbol.c_str(), s.quanty, s.totalShares, s.totalCash, round(s.totalProfit, 0.01), round(s.totalCommission, 0.01), ibpos);

		//double earnedProfit = nCash + nShare * (s.askPrice + s.bidPrice) / 2;
		totalProfit += s.totalProfit;
		totalCommission += s.totalCommission;
	}
	cprintf("Total Profit is %g, and total Commission is %g\n", round(totalProfit, 0.01), round(totalCommission, 0.01));
	cprintf("Account Avaible Funds is %g, Excess Liquidity is %g, buyingPower is %g\n", round(availableFunds), round(excessLiquidity), round(buyingPower));
}

//update order profit, commission
//update position total profit, and commission
//Input shares is the additional shares just executed.
//Input cash is the additional cash just executed.
void WangAccount::updateOrderProfit(WangOrder* o, double shares, double cash)
{
	unsigned int i;
	for (i = 0; i < positions.size(); i++) {
		if (positions[i]->symbol == o->symbol) break;
	}
	if (i >= positions.size()) return;

	WangPosition& s = *positions[i];

	s.totalProfit -= o->profit;
	s.totalCommission -= o->commission;

	if (o->action[0] == 'S') {
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

void WangAccount::updateOnePosition(std::string sym, double qnt, double cost)
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
		WangPosition* p = new WangPosition;
		p->symbol = sym;
		p->quanty = qnt;
		p->stockIndex = getStockIndex(sym);
		if (p->shared) write_Td_position(sym, qnt);
		positions.push_back(p);
	}
}


//qnt can be negative
void WangAccount::addToPosition(std::string sym, std::string action, double qnt, double cost)
{
	lastPositionChangeTime = time(NULL);
	if (action[0] == 'B') qnt = fabs(qnt);
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
		WangPosition* p = new WangPosition;
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
void WangAccount::updateOneOrder(std::string orderid, std::string status, std::string buySell, std::string sym, double price, double qnt, double filled, double remaining, double avgPrice)
{
	unsigned int i;
	extern std::vector<int> soundPlay;
	for (i = 0; i < orders.size(); i++) {
		WangOrder* p = orders[i];
		if (p->orderId == orderid) {  //find the order
			//the order is filled or partially filled
			if (filled > p->filledQty) {
				if (status == "FILLED") { //filled order
					if (p->status == "CANCELED") rprintf("Wrong: TD Canceling order get filled: %s, %s %s %g(%g)\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price);
					cprintf("TD Order filled %s: %s %s %g of %g(%g)", orderid.c_str(), buySell.c_str(), sym.c_str(), filled - p->filledQty, qnt, price);
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
					return;
				}
				else {  //partially filled order
					cprintf("TD Order partially filled %s: %s %s %g of %g(%g)\n", orderid.c_str(), buySell.c_str(), sym.c_str(), filled - p->filledQty, qnt, price);
					cprintf("TD Updated an Partially Filled Order: %s, %s %s%g(%g)from %s\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price, p->status.c_str());
					soundPlay.push_back(2);
					double shares = filled - p->filledQty;
					double cash = avgPrice * filled - p->avgFillPrice * p->filledQty;
					p->totalQty = qnt;
					p->action = buySell;
					p->lmtPrice = price;
					p->filledQty = filled;

					if (p->status == "PENDING_REPLACE") p->status = "QUEUED";      //pending replace order is actually partially filled.
					else if (p->status != "CANCELED" && p->status != "PENDING_CANCEL" && p->status != "GUESS_FILLED") p->status = status;   //keep CANCELED or PENDING_CANCEL, these orders can get partially filled message later.
					cprintf(" to %s\n", p->status.c_str());
					p->remaining = remaining;
					p->avgFillPrice = avgPrice;
					if (filled > 0.01) p->partiallyFilled = true;
					updateOrderProfit(p, shares, cash);  //update the order, position's profit, commissions, and cash changes.
					cprintf("total profit %g\n", p->profit);
					return;
				}
			}
			else if (filled < p->filledQty) {
				rprintf("!!!Found order update later than other update! This rarely happen. Check if there is anything wrong!!\n");
				return; //updating later than call back function, then no update
			}
			else if (p->status == "CANCELED" && status != "CANCELED") return;  //A just cancelled order, no need to update. 
			else if (p->status == "PENDING_CANCEL" && status != "CANCELED") return;  //A pending cancel order is still waiting, no need to update. 
			else if (p->status == "FILLED") return;  //A filled order, no need to update. 
			else {  //no filled or partially filled
				p->totalQty = qnt;
				p->action = buySell;
				p->lmtPrice = price;
				p->filledQty = filled;
				if (p->status != status) cprintf("TD Updated an Order: %s, %s %s%g(%g)from %s to %s\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price, p->status.c_str(), status.c_str());
				//if (status == "CANCELED" && p->status != "CANCELED") soundPlay.push_back(3);
				p->status = status;
				p->remaining = remaining;
				p->avgFillPrice = avgPrice;
				if (filled > 0.01) p->partiallyFilled = true;
				return;
			}
			return;
		}  //end find the order
	}
	//find a new order id
	if (i >= orders.size()) {
		if (getStockIndex(sym) == -1) return;
		if (fabs(filled) < 0.1 && (status == "CANCELED" || status == "REJECTED" || status == "EXPIRED")) return;
		cprintf("TD Update find a new order: %s, %s %s%g(%g)status: %s\n", orderid.c_str(), buySell.c_str(), sym.c_str(), qnt, price, status.c_str());
		WangOrder* p = new WangOrder;
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
			if (status == "FILLED") {
				cprintf("TD New Order filled %s: %s %s %g of %g(%g)", orderid.c_str(), buySell.c_str(), sym.c_str(), filled, qnt, price);
				soundPlay.push_back(1);
			}
			else {
				cprintf("TD New Order partilly filled %s: %s %s %g of %g(%g)", orderid.c_str(), buySell.c_str(), sym.c_str(), filled, qnt, price);
				soundPlay.push_back(2);
			}
			cprintf("total profit %g\n", p->profit);
		}
		orders.push_back(p);
	}
	//cprintf("Order ID: %s, Status: %s\n", orderid.c_str(), status.c_str());
}

void WangAccount::modifyLmtOrder(std::string orderId, double newSize, double newPrice)
{
	//				"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",
	for (unsigned int i = 0; i < orders.size(); i++) {
		WangOrder * o = orders[i];
		if (o->orderId != orderId) continue;
		if (o->status != "QUEUED" && o->status != "WORKING" && o->status != "PENDING_ACTIVATION" && o->status != "ACCEPTED") return;
		if (o->orderType != "LMT") return;
		//the following is not needed, since the commission at tdameritrade is 0
		//if (o->partiallyFilled && fabs(o->lmtPrice - newPrice) / o->lmtPrice < 0.003 && fabs(newSize - o->remaining) > max(min(o->remaining / 3, 10), 2)) return;
		if (fabs(o->lmtPrice - newPrice) < 0.001 && fabs(newSize - o->remaining) < 0.1) return;  //same order 

		std::string sym = o->symbol;
		std::string action = o->action;
		cprintf("TD Modifying order %s, %s %s %g(%g)status: %s to %s%g(%g)\n", o->orderId.c_str(), o->action.c_str(), sym.c_str(), o->totalQty, o->lmtPrice, o->status.c_str(), action.c_str(), newSize, newPrice);
		if (o->filledQty > 0 || o->partiallyFilled) {
			cprintf("Replace a partial filled order %s %g, cancel it and place a new order\n", o->orderId.c_str(), o->filledQty);
			cancelOrder(o->orderId);
			placeOrder(sym, action, newPrice, newSize);
		}
		else replaceOrder(orderId, sym, action, newPrice, newSize);
		//cancelOrder(o->orderId);
		//placeOrder(sym, action, newPrice, newSize);
		cprintf("TD End Modified order\n");
		break;
	}
}



bool WangAccount::getOrderInfo(WangOrder * p)
{
	extern std::vector<int> soundPlay;
	bool print = true;
	if (!pCManager) {
		pCManager = new tdma::CredentialsManager(creds_path, password);
	}
	tdma::OrderGetter o(pCManager->credentials, account_id, p->orderId);
	json order;
	try {
		order = o.get();
	}
	catch (const std::exception& e) {
		rprintf("TD get order %s information Error %s\n", p->orderId.c_str(), e.what());
		delete pCManager; pCManager = NULL;
		return false;
	}
	if (std::to_string((__int64)order["accountId"]) == account_id && order["orderLegCollection"][0]["instrument"]["assetType"] == "EQUITY" && order["orderType"] == "LIMIT") {
		std::string status = order["status"];
		if (status == "REJECTED" || status == "EXPIRED") {
			cprintf("Get Order %s Info: %s %s %g(%g)status: %s to %s\n", p->orderId.c_str(), p->action.c_str(), p->symbol.c_str(), p->totalQty, p->lmtPrice, p->status.c_str(), status.c_str());
			p->status = status;
			zeroActiveOrders(p);
			return true;
		}
		else if (status == "CANCELED" || status == "FILLED" || status == "QUEUED" || status == "ACCEPTED") {
			if (status == "FILLED") {
				if ((double)order["orderLegCollection"][0]["quantity"] - (double)order["filledQuantity"] > 0.01) 
					rprintf("GetOrderInfo, filled order still has remaining, quantity %g, filled %g\n", status.c_str(), (double)order["orderLegCollection"][0]["quantity"], (double)order["filledQuantity"]);
			}

			if (p->totalQty > (double)order["orderLegCollection"][0]["quantity"] + 0.01) {
				rprintf("GetOrderInfo, the totalQty %g > quantity %g, correct totalQty to quantity, and remaining too\n", p->totalQty, (double)order["orderLegCollection"][0]["quantity"]);
				p->remaining -= p->totalQty - (double)order["orderLegCollection"][0]["quantity"];
				p->totalQty = (double)order["orderLegCollection"][0]["quantity"];
			}

			double fqty = p->remaining - ((double)order["orderLegCollection"][0]["quantity"] - (double)order["filledQuantity"]);  //consider remaining, instead of quanty
			if (fqty < -0.01) {
				cprintf("get order info, find filled quanty less than 0, remaining %g < quantity %g - filled %g\n", p->remaining, (double)order["orderLegCollection"][0]["quantity"], (double)order["filledQuantity"]);
				cprintf("discard this information, this can be happenned if an order partially filled and has not been updated when request information.\n");
				return false;
			}
			p->filledQty += fqty;
			p->remaining = (double)order["remainingQuantity"];
			p->avgFillPrice = p->lmtPrice = (double)order["price"];
			if (fqty > 0.01) {
				cprintf("Getorderinfo, remaining position %g get filled\n", fqty);
				updateOrderProfit(p, fqty, fqty*p->lmtPrice);
				cprintf("Order total profit %g\n", p->profit);
				addToPosition(p->symbol, p->action, fqty, p->lmtPrice);
				if (status == "FILLED") soundPlay.push_back(1);
				else soundPlay.push_back(2);
			}
			p->status = status;
			cprintf("Get order info, order %s is %s\n", p->orderId.c_str(), status.c_str());
			if (status == "CANCELED") soundPlay.push_back(3);
			if (status != "QUEUED" && status != "ACCEPTED") zeroActiveOrders(p);
			return true;
		}
		else {
			rprintf("Find new status %s\n", status.c_str());
			p->status = status;
			return true;
		}
	}
	return false;
}



//critical session when update account with orders and positions.
void WangAccount::updateAccount()
{
	bool print = false;
	if (!pCManager) {
		pCManager = new tdma::CredentialsManager(creds_path, password);
	}
	tdma::AccountInfoGetter o(pCManager->credentials, account_id, false, false);
	o.return_orders(true);
	o.return_positions(true);
	//cout << o.get().dump(4);

	cprintf("Td Ameritrade clearing all positions, orders and account updating messages...\n");
	for (WangOrder* p : orders) delete p;
	orders.clear();
	messageQueue.clear();

	cprintf("Td Ameritrade getting account positions and orders...\n");
	json jGets;

	//when get the information, there should have no orders get executed.
	for (;;) {
		try {
			jGets = o.get();
		}
		catch (const std::exception& e) {
			rprintf("TD Update Account Error %s\n", e.what());
			delete pCManager; pCManager = NULL;
			return;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));  //wait for 1 second
		if (!messageQueue.empty()) messageQueue.clear();
		else break;
	}


	cprintf("Postions\n");

	auto jPositions = jGets["securitiesAccount"]["positions"];
	for (auto position : jPositions) {
		if (position["instrument"]["assetType"] == "EQUITY") {
			updateOnePosition(position["instrument"]["symbol"], (double)position["longQuantity"] - (double)position["shortQuantity"], (double)position["averagePrice"]);
		}
	}
	for (auto p : positions) {
		if (p->shared && p->quanty == 0.0) write_Td_position(p->symbol, 0.0);
	}

	if (print) std::cout << "Orders\n";
	auto jOrders = jGets["securitiesAccount"]["orderStrategies"];

	for (auto order : jOrders) {
		//std::cout << order.dump(4);
		if (std::to_string((__int64)order["accountId"]) == account_id && order["orderLegCollection"][0]["instrument"]["assetType"] == "EQUITY" && order["orderType"] == "LIMIT") {
			updateOneOrder(std::to_string((__int64)order["orderId"]), order["status"], order["orderLegCollection"][0]["instruction"], order["orderLegCollection"][0]["instrument"]["symbol"], (double)order["price"], (double)order["orderLegCollection"][0]["quantity"], (double)order["filledQuantity"], (double)order["remainingQuantity"], (double)order["price"]);
		}
		//if (std::to_string((__int64)order["orderId"]) == "4509030215") {
		//	std::cout << order.dump(4) << std::endl;
		//}
	}

	availableFunds = jGets["securitiesAccount"]["currentBalances"]["sma"];
	excessLiquidity = jGets["securitiesAccount"]["currentBalances"]["liquidationValue"];
	//std::cout << jGets.dump(4);
	buyingPower = jGets["securitiesAccount"]["currentBalances"]["buyingPower"];
	
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
			if (order->status == "QUEUED") {
				if (order->action[0] == 'B') {
					getPosition(order->symbol).activeBuyOrder = order;
				}
				else {
					getPosition(order->symbol).activeSellOrder = order;
				}
			}
		}
	}
}

void WangAccount::updatePositions()
{
	bool print = false;
	if (!pCManager) {
		pCManager = new tdma::CredentialsManager(creds_path, password);
	}
	tdma::AccountInfoGetter o(pCManager->credentials, account_id, false, false);
	o.return_positions(true);

	cprintf("Td Ameritrade getting account positions only...\n");
	json jGets;

	//when get the information, there should have no orders get executed.
	try {
		jGets = o.get();
	}
	catch (const std::exception& e) {
		rprintf("TD Update Positions Error %s\n", e.what());
		delete pCManager; pCManager = NULL;
		return;
	}
	cprintf("Postions\n");

	auto jPositions = jGets["securitiesAccount"]["positions"];
	for (auto position : jPositions) {
		if (position["instrument"]["assetType"] == "EQUITY") {
			updateOnePosition(position["instrument"]["symbol"], (double)position["longQuantity"] - (double)position["shortQuantity"], (double)position["averagePrice"]);
		}
	}
	for (auto p : positions) {
		if (p->shared && p->quanty == 0.0) write_Td_position(p->symbol, 0.0);
	}
	cprintf("Ending positions update\n");
}


//does not work for the price quote. Do not use.
void WangAccount::requestMarketData_nouse()
{
	using ft = tdma::QuotesSubscription::FieldType;
	std::set<std::string> symbols = {};
	std::set<ft> fields = {ft::bid_price, ft::bid_size, ft::ask_price, ft::ask_size, ft::last_price, ft::last_size};

	int i;
	for (i = 0; i < allStockNum; i++) {
		if (allStock[i].dataRequested) continue;

		symbols.insert(allStock[i].symbol);

		allStock[i].dataRequested = true;
	}
	tdma::QuotesSubscription q1(symbols, fields);

//	tdma::RawSubscription q19("NASDAQ_BOOK", "SUBS", { {"keys","RCL,BILI"}, {"fields", "0,1,2,3,4"} });


	streamSession->add_subscription(q1);
}

void WangAccount::wangJob()
{
	static int count = 0;
	static time_t updatePositionsTime = time(NULL);
	static time_t now;
	time_t now_t = time(NULL);
	struct tm* tm_struct = localtime(&now_t);
	int hour = tm_struct->tm_hour;
	int minute = tm_struct->tm_min;
	ProcessControlFile();
	if (color != INDIVIDUALACCCOLOR) {
		if (excessLiquidity < 27000) {
			if (hour < 12) {
				m_canBuy = false; m_canSell = true;
				//m_canBuy = true; m_canSell = false;
			}
			else {
				m_canBuy = true; m_canSell = false;
			}
		}
		else {
			m_canBuy = true; m_canSell = true;
		}
	}

	read_shared_tick();
	if (hour >= 20 || hour < 6 || (hour == 6 && minute < 59)) {  //out of trading hours: empty order list, and just wait...
		cprintf("TD current time is %d:%d\n", hour, minute);
		for (auto p : positions) p->activeBuyOrder = p->activeSellOrder = NULL;
		if (!orders.empty()) {
			for (int i = orders.size() - 1; i >= 0; i--) {
				if (orders[i]->status == "QUEUED" || orders[i]->status == "WORKING" || orders[i]->status == "PENDING_REPLACE" || orders[i]->status == "ACCEPTED")
					cancelOrder(orders[i]->orderId);
				delete orders[i];
				orders.erase(orders.begin() + i);
			}
			std::this_thread::sleep_for(std::chrono::seconds(30));
		}
		messageQueue.clear();
		if (pCManager) delete pCManager;
		pCManager = NULL;
		for (unsigned int i = 0; i < positions.size(); i++) {
			//output the one with different
			WangPosition& s = *positions[i];
			if (s.stockIndex == -1) continue;
			if (fabs(s.targetBottomPrice - s.oldBottomPrice) > 0.001) {
				cprintf("TD %s Bottom price different: %g, %g\n", s.symbol.c_str(), s.oldBottomPrice, s.targetBottomPrice);
			}
			if (fabs(s.targetTopPrice - s.oldTopPrice) > 0.001) {
				cprintf("TD %s Top price different: %g, %g\n",s.symbol.c_str(),s.oldTopPrice,s.targetTopPrice);
			}
			if (fabs(s.targetMultiplier -s.oldMultiplier) > 0.01) {
				cprintf("TD %s Multiplier different: %g, %g\n",s.symbol.c_str(),s.oldMultiplier,s.targetMultiplier);
			}
			if (fabs(s.targetOffset -s.oldOffset) > 0.1) {
				cprintf("TD %s Offset different: %g, %g\n",s.symbol.c_str(),s.oldOffset,s.targetOffset);
			}
		}
		std::this_thread::sleep_for(std::chrono::seconds(30));
		return;
	}

	while (!pCManager) {
		printf("Sleeping %u seconds before updating account information\n", 10);
		std::this_thread::sleep_for(std::chrono::seconds(10));
		printf("Td Ameritrade try to reset the account information...\n");
		updateAccount();
	}
	ProcessMessages();
	read_shared_tick();

	//update topprice, bottomprice, etc.
	for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
		WangPosition & s = *positions[stockindex];
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
		if (count % 45 == 0) {  //every 15 minutes
			if (pCManager) delete pCManager;
			pCManager = new tdma::CredentialsManager(creds_path, password);
		}

		for (int i = orders.size() - 1; i >= 0; i--) {
			WangOrder* o = orders[i];
										//		"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",			
			if (o->filledQty == 0 && (o->status == "CANCELED" || o->status == "EXPIRED" || o->status == "REJECTED" || o->status == "REPLACED")) {
				delete o;
				orders.erase(orders.begin() + i);
			}
		}

		if (color == INDIVIDUALACCCOLOR) outputBuffer(TD_OUTPUTILENAME1);

		if (count % 6 == 0) {  //every 2 minutes
			printPositions();
			double hedgedAmount = 0.0;
			for (unsigned int index = 0; index < positions.size(); index++) {
				WangPosition& s = *positions[index];
				if (s.stockIndex == -1) continue;
				hedgedAmount += s.quanty * (s.askPrice() + s.bidPrice()) * 0.5 * s.hedgeRatio;
			}
			cprintf("TD Total hedging amount that still needed is %g (wan)\n", round(hedgedAmount * 0.0001, 0.01));
		}

		bool modified = false;
		for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
			WangPosition& s = *positions[stockindex];
			if (s.shared) write_Td_position(s.symbol, s.quanty);

			if (s.stockIndex == -1) continue; //Not in the all stock list.
			if (s.askPrice() < 1.0 || s.bidPrice() < 1.0) continue;
			if (hour >= 16 || hour < 9 || (hour == 9 && minute < 30)) {  //out of regular trading hours
				if (s.symbol.size() == 5 && (s.symbol[4] == 'Y' || s.symbol[4] == 'F')) continue;    // do not trade OTC stocks out of regular trading hours.
			}

			if (hour == 10 || hour == 11 || hour == 15 || hour == 14 || (hour == 9 && minute >= 28)) {  //busiest trading hours, do not do that.
				continue;
			}

			for (unsigned int i = 0; i < orders.size(); i++) {
				WangOrder* o = orders[i];
				if (o->symbol != s.symbol) continue;
//				"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",
//				"instruction": "'BUY' or 'SELL' or 'BUY_TO_COVER' or 'SELL_SHORT' or 'BUY_TO_OPEN' or 'BUY_TO_CLOSE' or 'SELL_TO_OPEN' or 'SELL_TO_CLOSE' or 'EXCHANGE'",

				if (o->status == "QUEUED" && (o->remaining == o->totalQty || o->remaining * o->lmtPrice > 8000)) {
					if (o->action[0] == 'B' && o->lmtPrice >= s.bidPrice() - 0.001 && s.bidSize() == floor(o->remaining / 100)) {
						modifyLmtOrder(o->orderId, o->remaining, round(o->lmtPrice * 0.99, 0.01));
						s.newOrderSettled = false;
						modified = true;
					}
					else if (o->action[0] == 'S' && o->lmtPrice <= s.askPrice() + 0.001 && s.askSize() == floor(o->remaining / 100)) {
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
			if (order->action[0] == 'B') {
//				if (order->lmtPrice < getPosition(order->symbol).bidPrice()-0.001) {
					rprintf("Guess filled order %s is actually not filled. \n", order->orderId.c_str());
					order->status = "QUEUED";
					order->fillTime = 0;
					addToPosition(order->symbol, "SELL", order->remaining, order->lmtPrice);
					rprintf("Guess filled order %s reverted back to QUEUED\n", order->orderId.c_str());
					getPosition(order->symbol).activeBuyOrder = order;
//				}
			}
			else {
//				if (order->lmtPrice > getPosition(order->symbol).askPrice()+0.001) {
					rprintf("Guess filled order %s is actually not filled. \n", order->orderId.c_str());
					order->status = "QUEUED";
					order->fillTime = 0;
					addToPosition(order->symbol, "BUY", order->remaining, order->lmtPrice);
					rprintf("Guess filled order %s reverted back to QUEUED\n", order->orderId.c_str());
					getPosition(order->symbol).activeSellOrder = order;
//				}
			}
		}
	}


	for (unsigned int stockindex = 0; stockindex < positions.size(); stockindex++) {
		WangPosition& s = *positions[stockindex];
		if (s.stockIndex == -1) continue; //Not in the all stock list.
		if (s.askPrice() < 1.0 || s.bidPrice() < 1.0) continue;
		if (hour >= 16 || hour < 9 || (hour == 9 && minute < 30)) {  //out of regular trading hours
			if (s.symbol.size() == 5 && (s.symbol[4] == 'Y' || s.symbol[4] == 'F')) continue;    // do not trade OTC stocks out of regular trading hours.
		}
		if (hour == 10 || hour == 11 || hour == 15 || hour == 14 || (hour == 9 && minute >= 28)) //busiest trading hours
		{  
			if (s.newOrderSettled && time(NULL) - s.newOrderSettleTime < 10) //do nothing for 10 seconds
				continue;
		}
		else {
			if (s.newOrderSettled && time(NULL) - s.newOrderSettleTime < 2) //do nothing for 2 seconds
				continue;
		}


		//get current buy order price and sell order price. Adjust bidPrice and askPrice.
		double currentBuyPrice = -1.0, currentBuyQty = 0;
		double currentSellPrice = -1.0, currentSellQty = 0;
		for (unsigned int i = 0; i < orders.size(); i++) {
			WangOrder* o = orders[i];
			if (o->symbol != s.symbol) continue;
			if (o->status == "CANCELED") continue;
			if (o->status == "PENDING_CANCEL") continue;
			if (o->status == "FILLED") continue;
			if (o->status == "EXPIRED") continue;
			if (o->status == "REJECTED") continue;

			if (o->action[0] == 'B') {
				if (o->remaining > 0 && currentBuyPrice < 0) {
					currentBuyPrice = o->lmtPrice;
					currentBuyQty = o->remaining;
				}
			}
			else if (o->action[0] == 'S') {
				if (o->remaining > 0 && currentSellPrice < 0) {
					currentSellPrice = o->lmtPrice;
					currentSellQty = o->remaining;
				}
			}
		}
		//do not count myself. Adjust bidPrice and askPrice.
		if (currentBuyPrice == s.bidPrice()) {
			if (s.bidSize() * 100 <= currentBuyQty || s.bidSize() * 100 - currentBuyQty < 100) {
				s.bidPrice() -= 0.01;
			}
		}
		if (currentSellPrice == s.askPrice()) {
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
			spreadPercent = 0.6 / 200.0;
		}
		else if (hour == 9 && minute >= 45) {  //9:45 - 10:00
			spreadPercent = 0.5 / 200.0;
		}
		else if (hour == 10 && minute <= 30) { //10:00-10:30
			spreadPercent = 0.4 / 200.0;
		}
		else if (hour == 10 || hour == 11) {
			spreadPercent = 0.3 / 200.0;
		}
		else if (hour == 12 || hour == 13) {
			spreadPercent = 0.3 / 200.0;
		}
		else if (hour == 14 || hour == 15) {
			spreadPercent = 0.4 / 200.0;
		}
		else {
			spreadPercent = 0.25 / 200.0;
		}

		double profit = multiplier * 100 * spreadPercent * spreadPercent * cPrice;  //the profit for each round (one buy plus one sell)
		if (hour < 9 || hour >= 16 || (hour == 9 && minute < 28)) {// out of regular trading hours
			if (profit < 4) {
				profit = 4;
				spreadPercent = (multiplier == 0) ? 0.0001 : sqrt(profit / (multiplier * 100.0 * cPrice));  //the minimum spread to get the profit. 
				if (spreadPercent > 0.05) spreadPercent = 0.05;  //5% maximum
			}
		}
		else {
			if (profit < s.takenProfit) {
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
			double buyPrice = floor((cPrice - spread) * 100) / 100;
			double sellPrice = ceil((cPrice + spread) * 100) / 100;
			if (buyPrice > s.bidPrice() + 0.01) buyPrice = s.bidPrice() + 0.01; if (buyPrice > s.askPrice() - 0.011) buyPrice = s.askPrice() - 0.01;
			if (sellPrice < s.askPrice() - 0.01) sellPrice = s.askPrice() - 0.01; if (sellPrice < s.bidPrice() + 0.011) sellPrice = s.bidPrice() + 0.01;

			double buyDelta = deltaCalculation(buyPrice, bottomPrice, topPrice, multiplier, offset, s.periods) - position; if (buyDelta < 0.5)  buyDelta = 0;
			double sellDelta = position - deltaCalculation(sellPrice, bottomPrice, topPrice, multiplier, offset, s.periods); if (sellDelta < 0.5) sellDelta = 0;

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
			}

			if (s.quanty < -0.01 && buyDelta > -s.quanty) buyDelta = -s.quanty;   //negative position
			else if (s.quanty > 0.01 && sellDelta > s.quanty) sellDelta = s.quanty;   //positive position
			else if (fabs(s.quanty) <= 0.01 && s.shared) {   //zero position
				sellDelta = 0;
			}
			else if (fabs(s.quanty) <= 0.01 && buyDelta > 0 && sellDelta > 0) {   //zero position
				//if there is a buy order, or sell order keep it until sellPrice is very close to bidPrice.
				if (fabs(buyPrice - s.askPrice()) > fabs(sellPrice - s.bidPrice()) * 2+0.01) buyDelta = 0;
				else if (fabs(buyPrice - s.askPrice()) * 2 + 0.01 < fabs(sellPrice - s.bidPrice())) sellDelta = 0;
				else {
					for (unsigned int i = 0; i < orders.size(); i++) {
						WangOrder* o = orders[i];
						if (o->symbol != s.symbol || o->status == "CANCELED" || o->status == "PENDING_CANCEL" || o->status == "FILLED" || o->status == "EXPIRED" || o->status == "REJECTED") continue;
						if (o->action[0] == 'B') {
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
/*
			if (s.shared && s.quanty * round(s.askPrice()*3.0)/3.0  >= 35000) {  //do not buy ; *3/3 is to stabalize it.
				buyDelta = 0;
			}
			if (s.shared && s.quanty < 0.1) {  //do not sell to negative.
				sellDelta = 0;
			}
*/
			if (s.shared) { 
				buyDelta = 0;
			}
			if (s.shared && s.quanty < 0.1) {  //do not sell to negative.
				sellDelta = 0;
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
			if (buyDelta * buyPrice > buyingPower) buyDelta = round(buyingPower / buyPrice);  if (buyDelta < 0)  buyDelta = 0;
			if (buyDelta * buyPrice > 25000) buyDelta = round(25000 / buyPrice);  if (buyDelta < 0)  buyDelta = 0;
			if (sellDelta * sellPrice > 25000) sellDelta = round(25000 / sellPrice); if (sellDelta < 0) sellDelta = 0;

			bool toBuy = true;
			bool toSell = true;
			bool toModifyBuy = false; std::string modifyBuyOrderID = "";
			bool toModifySell = false; std::string modifySellOrderID = "";

			//determine which order to modify to buy or sell, or create new buy or sell order
			//And cancel all the other orders.
			for (unsigned int i = 0; i < orders.size(); i++) {
				WangOrder* o = orders[i];
				if (o->symbol != s.symbol || o->status == "CANCELED" || o->status == "PENDING_CANCEL" || o->status == "FILLED" || o->status == "GUESS_FILLED" || o->status == "EXPIRED" || o->status == "REJECTED" || o->status == "AWAITING_UR_OUT") continue;
				if (o->status != "QUEUED" && o->status != "ACCEPTED" && o->status != "WORKING" && o->status != "PENDING_CANCEL" && o->status != "PENDING_REPLACE")
					rprintf("%s order %s\n", o->symbol.c_str(), o->status.c_str());  //print out special order status
				//				"status": "'AWAITING_PARENT_ORDER' or 'AWAITING_CONDITION' or 'AWAITING_MANUAL_REVIEW' or 'ACCEPTED' or 'AWAITING_UR_OUT' or 'PENDING_ACTIVATION' or 'QUEUED' or 'WORKING' or 'REJECTED' or 'PENDING_CANCEL' or 'CANCELED' or 'PENDING_REPLACE' or 'REPLACED' or 'FILLED' or 'EXPIRED'",
				//				"instruction": "'BUY' or 'SELL' or 'BUY_TO_COVER' or 'SELL_SHORT' ,

				if (o->action[0] == 'B') {  //Find the buy order
					if (buyDelta < 0.1 || toModifyBuy || !toBuy) {
						if (o->status == "QUEUED" || o->status == "ACCEPTED" || o->status == "WORKING") {
							cprintf("Extra Buying order to be cancelled\n");
							cancelOrder(o->orderId);
						}
						//toBuy = false; //not buy this time, wait for more guaranttee.
						continue;
					}
					double deltaDiff = fabs(buyDelta - o->remaining);
					if (toBuy && deltaDiff < 1 && round(o->lmtPrice * 100 - buyPrice * 100) == 0)   //same order, keep this order
					{
						if (o->status == "QUEUED") s.activeBuyOrder = o;
						toBuy = false;  //find the order to keep
					}
					else if (toModifyBuy == false && (o->status == "QUEUED" || o->status == "ACCEPTED" || o->status == "WORKING" || o->status == "PENDING_ACTIVATION")) {  //change this order
						modifyBuyOrderID = o->orderId;
						toModifyBuy = true;
					}
					else if (o->status == "PENDING_REPLACE") {
						toBuy = false;
					}
					else {  //Others, just keep. For example, PENDING_CANCEL orders, etc.
						//toBuy = false;
					}
				}
				else if (o->action[0] == 'S') {  //sell
					if (sellDelta < 0.1 || toModifySell || !toSell) {
						cprintf("Extra Selling order to be cancelled\n");
						cancelOrder(o->orderId);
						continue;
					}
					double deltaDiff = fabs(sellDelta - o->remaining);
					if (toSell && deltaDiff < 1 && round(o->lmtPrice * 100) == round(sellPrice * 100)) //same order, no new buy order
					{
						if (o->status == "QUEUED") s.activeSellOrder = o;
						toSell = false;  //find the order to keep
					}
					else if (toModifySell == false && (o->status == "QUEUED" || o->status == "ACCEPTED" || o->status == "WORKING" || o->status == "PENDING_ACTIVATION")) {
						modifySellOrderID = o->orderId;
						toModifySell = true;
					}
					else if (o->status == "PENDING_REPLACE") {
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
						placeOrder(s.symbol, "BUY", buyPrice, buyDelta);
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
						placeOrder(s.symbol, "SELL", sellPrice, sellDelta);
					}
					s.newOrderSettled = true;
					s.newOrderSettleTime = time(NULL);
				}

				if (s.activeBuyOrder) {
					cprintf("Current Active Buy Order is %s %s %s %g(%g) %s\n", s.activeBuyOrder->orderId.c_str(), s.activeBuyOrder->action.c_str(), s.activeBuyOrder->symbol.c_str(), s.activeBuyOrder->remaining, s.activeBuyOrder->lmtPrice, s.activeBuyOrder->status.c_str());
				}
				else
					cprintf("No active buy order\n");

				if (s.activeSellOrder) {
					cprintf("Current Active Sell Order is %s %s %s %g(%g) %s\n", s.activeSellOrder->orderId.c_str(), s.activeSellOrder->action.c_str(), s.activeSellOrder->symbol.c_str(), s.activeSellOrder->remaining, s.activeSellOrder->lmtPrice, s.activeSellOrder->status.c_str());
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