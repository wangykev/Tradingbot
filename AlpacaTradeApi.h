#ifndef ALPACATRADEAPI
#define ALPACATRADEAPI

#include <string>
#include <iostream>
#include <sstream>
#include <list>
#include "curl/curl.h"
#include <json/json.h>

#include "Alpaca.h"
void jsonPrint(Json::Value v);

namespace alpaca {


	class Tradeapi {
	public:
		void init(std::string, std::string, std::string);
		~Tradeapi();
		/* Account */
		//Json::Value getAccount();
		Account get_account();

		/* Orders */
		std::string replace_order(std::string, int qty, double limit_price);

		std::string submit_order(std::string, int qty, std::string, std::string,
			std::string, double limit_price = 0, double stop_price = 0, std::string client_order_id = "");
		std::vector<Order> list_orders(std::string status, int limit = 50, std::string after = "",
			std::string until = "", std::string direction = "");
		std::vector<Activity> list_activities(int year, int mon, int day);
		Order get_order(std::string order_id);
		Order get_order_by_client_order_id(std::string client_order_id);
		bool cancel_order(std::string order_id);
		bool cancel_all_orders();

		/* Positions */
		std::vector<Position> list_positions();
		Position get_position(std::string);

		/* Asset */
		std::vector<Asset> list_assets(std::string status = "active", std::string asset_class = "us_equity");
		Asset get_asset(std::string);

		/* Bar */
		Json::Value/*std::vector<Bar>*/ get_barset(std::vector<std::string> symbols, std::string timeframe = "1D",
			int limit = 100, std::string start = "", std::string end = "",
			std::string after = "", std::string until = "");

		/* Clock */
		Clock get_clock();

		/* Calendar */
		Json::Value get_calendar(std::string start = "", std::string end = "");

		std::string errorMsg;
		bool connected;


	private:

		//WebAPI
		CURL* curl = NULL;
		Json::Value GET(std::string req, std::string params = "", std::string endpoint = "");
		Json::Value POST(std::string req, std::string params = "");
		Json::Value Delete(std::string req, std::string params = "");
		Json::Value Patch(std::string req, std::string path_params, std::string params);

		static std::size_t callback(
			const char* in,
			std::size_t size,
			std::size_t num,
			char* out);

		//Construct GET URL w/ parameters
		std::string build_params(std::vector<std::string> params);
		//Build JSON string
		std::string build_json(std::vector<std::string> params);

		//Authentication
		std::string EndPoint;
		std::string base_url;
		std::string KeyID;
		std::string SecretKey;
	};

	inline size_t Tradeapi::callback(
		const char* in,
		std::size_t size,
		std::size_t num,
		char* out)
	{
		std::string data(in, (std::size_t)size * num);
		*((std::stringstream*)out) << data;
		return size * num;
	}

}
#endif
