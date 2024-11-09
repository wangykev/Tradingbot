#include "alpaca.h"

namespace alpaca {
	Account::Account(Json::Value resp) {
		

		account_blocked = resp["account_blocked"].asBool();
		buying_power = std::stod(resp["buying_power"].asString());
		cash = std::stod(resp["cash"].asString());
		created_at = resp["created_at"].asString();
		currency = resp["currency"].asString();
		id = resp["id"].asString();
		portfolio_value = std::stod(resp["portfolio_value"].asString());
		status = resp["status"].asString();
		trade_suspended_by_user = resp["trade_suspended_by_user"].asBool();
		trading_blocked = resp["trading_blocked"].asBool();
		transfers_blocked = resp["transfers_blocked"].asBool();
		equity = std::stod(resp["equity"].asString());

		json = resp;
	}

	Asset::Asset(Json::Value data) {
		id = data["id"].asString();
		asset_class = data["asset_class"].asString();
		exchange = data["exchange"].asString();
		symbol = data["symbol"].asString();
		status = data["status"].asString();
		tradable = data["tradable"].asBool();

		json = data;
	}

	Bar::Bar(Json::Value data) {

		t = data["t"].asString();
		o = std::stod(data["o"].asString());
		h = std::stod(data["h"].asString());
		l = std::stod(data["l"].asString());
		c = std::stod(data["c"].asString());
		v = std::stod(data["v"].asString());

		json = data;
	}

	Calendar::Calendar(Json::Value data) {

		date = data["date"].asString();
		open = data["open"].asString();
		close = data["close"].asString();

		json = data;
	}


	Clock::Clock(Json::Value data) {

		timestamp = data["timestamp"].asString();
		is_open = data["is_open"].asBool();
		next_open = data["next_open"].asString();
		next_close = data["next_close"].asString();

		json = data;
	}

	Order::Order(Json::Value data) {
		if (data.isNull()) return;
		id = data["id"].asString();
		client_order_id = data["client_order_id"].asString();
		created_at = data["created_at"].asString();
		updated_at = data["updated_at"].asString();
		submitted_at = data["submitted_at"].asString();
		//filled_at = data["filled_at"].asString();
		//expired_at = data["expired_at"].asString();
		//canceled_at = data["canceled_at"].asString();
		asset_id = data["asset_id"].asString();
		symbol = data["symbol"].asString();
		//exchange = data["exchange"].asString();
		//asset_class = data["asset_class"].asString();
		qty = std::stod(data["qty"].asString());
		filled_qty = std::stod(data["filled_qty"].asString());
		replaced_by = data["replaced_by"].asString();
		type = data["type"].asString();
		action = data["side"].asString();
		//time_in_force = data["time_in_force"].asString();
		limit_price = data["limit_price"] ? std::stod(data["limit_price"].asString()) : 0.0;
		//stop_price = data["stop_price"] ? std::stod(data["stop_price"].asString()) : 0.0;
		status = data["status"].asString();

		if (status == "replaced") status = "canceled";

		//json = data;
	}
	/*
	
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

	
	*/
	Activity::Activity(Json::Value data) {
		if (data.isNull()) return;
		id = data["id"].asString();
		activity_type = data["activity_type"].asString();
		side = data["side"].asString();
		symbol = data["symbol"].asString();
		transaction_time = data["transaction_time"].asString();
		order_id = data["order_id"].asString();
		type = data["type"].asString();

		qty = std::stod(data["qty"].asString());
		cum_qty = std::stod(data["cum_qty"].asString());
		leaves_qty = std::stod(data["leaves_qty"].asString());
		price = std::stod(data["price"].asString());
	}

	Position::Position(std::string symbol) {

		this->symbol = symbol;
		qty = 0;
	}

	Position::Position(Json::Value data) {
		asset_id = data["asset_id"].asString();
		symbol = data["symbol"].asString();
		exchange = data["exchange"].asString();
		asset_class = data["asset_class"].asString();
		avg_entry_price = std::stod(data["avg_entry_price"].asString());
		qty = std::stod(data["qty"].asString());
		side = data["side"].asString();
		/*
		double market_value;
		double cost_basis;
		std::string unrealized_pl;
		double unrealized_plpc;
		double unrealized_intraday_pl;
		double unrealized_intraday_plpc;

		double current_price;
		double lastday_price;
		double change_today;

		json = data;
		*/
	}







}