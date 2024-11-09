#ifndef ALPACA_H
#define ALPACA_H

#include <json/json.h>

namespace alpaca {
	class Account {
	public:
		Account(Json::Value);
		bool account_blocked;
		bool trade_suspended_by_user;
		bool trading_blocked;
		bool transfers_blocked;

		double buying_power;
		double cash;
		double portfolio_value;
		double equity;

		std::string status;
		std::string created_at;
		std::string currency;
		std::string id;

		Json::Value json;
	};

	class Asset {
	public:
		Asset(Json::Value);

		std::string id;
		std::string asset_class;
		std::string exchange;
		std::string symbol;
		std::string status;
		bool tradable;

		//Keep Json
		Json::Value json;

	};

	class Bar {
	public:
		Bar(Json::Value);

		std::string symbol;
		std::string t;
		double o;
		double h;
		double l;
		double c;
		double v;

		//Keep Json
		Json::Value json;

	};

	class Calendar {
	public:
		Calendar(Json::Value);
		std::string date;
		std::string open;
		std::string close;

		//Keep Json
		Json::Value json;
	};

	class Clock {
	public:
		Clock(Json::Value);
		std::string timestamp;
		bool is_open;
		std::string next_open;
		std::string next_close;

		//Keep Json
		Json::Value json;
	};

	class Order {
	public:
		Order() {};
		Order(Json::Value);
		std::string id;
		std::string client_order_id;
		std::string created_at;
		std::string updated_at;
		std::string submitted_at;
		std::string filled_at;
		std::string expired_at;
		std::string canceled_at;
		std::string asset_id;
		std::string symbol;
		std::string exchange;
		std::string asset_class;
		std::string replaced_by;
		double qty;
		double filled_qty;
		std::string type;
		std::string action;
		std::string time_in_force;
		double limit_price;
		double stop_price;
		std::string status;

		//Keep Json
		Json::Value json;

	};


	/*
{
  "activity_type": "FILL",
  "cum_qty": "1",
  "id": "20190524113406977::8efc7b9a-8b2b-4000-9955-d36e7db0df74",
  "leaves_qty": "0",
  "price": "1.63",
  "qty": "1",
  "side": "buy",
  "symbol": "LPCN",
  "transaction_time": "2019-05-24T15:34:06.977Z",
  "order_id": "904837e3-3b76-47ec-b432-046db621571b",
  "type": "fill"
}
	*/
	class Activity {
	public:
		Activity() {};
		Activity(Json::Value);
		std::string id;
		std::string activity_type;
		std::string side;
		std::string symbol;
		std::string transaction_time;
		std::string order_id;
		std::string type;
		double cum_qty;
		double leaves_qty;
		double qty;
		double price;

		//Keep Json
		Json::Value json;

	};

	class Position {
	public:
		Position(std::string symbol);
		Position(Json::Value);

		std::string asset_id;
		std::string symbol;
		std::string exchange;
		std::string asset_class;
		double avg_entry_price;
		int qty;
		std::string side;
		double market_value;
		double cost_basis;
		std::string unrealized_pl;
		double unrealized_plpc;
		double unrealized_intraday_pl;
		double unrealized_intraday_plpc;

		double current_price;
		double lastday_price;
		double change_today;


		//Keep Json
		Json::Value json;

	};

}

#endif
