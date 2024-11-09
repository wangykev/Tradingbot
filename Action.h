#pragma once

#include <time.h>
#include <string>
#include <deque>

class Action
{
public:
	Action(int ty, std::string a, double p, double q):price(p), qty(q), type(ty), action(a) {
		t = time(&t);
	}
	time_t t;
	double price;
	double qty;
	int type;   //1, new order; 2, replace order; 3, cancel order; 4, fill order
	std::string action;  //buy, sell
	bool operator==(Action& a) {
//		if (fabs(difftime(a.t, t)) < 3 && a.type == type && fabs(a.price - price) < 0.0001 && fabs(a.qty - qty) < 0.0001 && a.action == action) return true;
		if (a.type == type && fabs(a.price - price) < 0.0001 && fabs(a.qty - qty) < 0.0001 && a.action == action) return true;
		else return false;
	}
};

typedef  std::deque<class Action> Actions;

bool repeatPatterns(Actions &action);

