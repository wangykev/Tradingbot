#include "AlpacaTradeApi.h"
#include "FolderDefinition.h"

std::string dtos(double d)
{
	char s[20];
	sprintf(s, "%g", d);
	return std::string(s);
}

void jsonPrint(Json::Value v)
{
	static int count = 0;
	count++;
	FILE* f = fopen(ALPACA_OUTPUT_FOLDER"messages.txt", "a");
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "    "; // If you want whitespace-less output
	const std::string output = Json::writeString(builder, v);
	fprintf(f, "%d\n%s\n", count, output.c_str());
	fclose(f);
	printf("json id: %d\n", count);
	f = fopen(ALPACA_OUTPUT_FOLDER"output.txt", "a");
	if (f) {
		fprintf(f, "json id: %d\n", count);
		fclose(f);
	}
}

namespace alpaca {

	void Tradeapi::init(std::string EndPoint, std::string KeyID, std::string SecretKey) {
		this->EndPoint = EndPoint;
		this->KeyID = KeyID;
		this->SecretKey = SecretKey;
		this->base_url = "https://" + EndPoint + "/v2";
		connected = true;

		//this->curl = curl_easy_init();
	}

	Tradeapi::~Tradeapi() {
		//curl_easy_cleanup(curl);
	}

	std::string Tradeapi::build_params(std::vector<std::string> params) {

		std::string ret = "?";
		for (int i = 0; i < params.size(); i++) {
			if (i > 0)
				ret += "&";
			ret += params[i];
		}
		return ret;
	}

	Json::Value Tradeapi::GET(std::string req, std::string params, std::string endpoint) {

		std::string request;
		Json::Value jsonData;

		if (endpoint.size() > 0)
			request = "https://" + endpoint + req + params;
		else
			request = base_url + req + params;

		std::cout << "request: " + request << std::endl;

		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, request.c_str());
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		struct curl_slist* chunk = NULL;
		std::string keyid = "APCA-API-KEY-ID: " + KeyID;
		std::string secret = "APCA-API-SECRET-KEY: " + SecretKey;


		chunk = curl_slist_append(chunk, keyid.c_str());
		chunk = curl_slist_append(chunk, secret.c_str());

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

		//Response information
		long httpCode(0);
		std::stringstream httpData;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &httpData);

		curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_easy_cleanup(curl);


		if (httpCode == 200) {
			//std::cout << "200" << std::endl;
			//Json::Value jsonData;
			Json::CharReaderBuilder jsonReader;
			std::string errs;

			if (Json::parseFromStream(jsonReader, httpData, &jsonData, &errs)) {
				//std::cout << "\nJSON data received: " << std::endl;
				//std::cout << jsonData.toStyledString() << std::endl;	
			}
		}
		else if (httpCode == 0) {
			printf("connection failed, get nothing\n");
			connected = false;
		}
		else {
			printf("httpCode = %d\n", httpCode);
			errorMsg = httpData.str();
		}
		return jsonData;
	}

	Json::Value Tradeapi::POST(std::string req, std::string params) {

		std::string request;
		Json::Value jsonData;

		request = base_url + req;

		std::cout << "request: " << request << std::endl;
		std::cout << "params: " << params << std::endl;

		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, request.c_str());
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		struct curl_slist* chunk = NULL;
		std::string keyid = "APCA-API-KEY-ID: " + KeyID;
		std::string secret = "APCA-API-SECRET-KEY: " + SecretKey;


		chunk = curl_slist_append(chunk, keyid.c_str());
		chunk = curl_slist_append(chunk, secret.c_str());

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());

		//Response information
		long httpCode(0);
		std::stringstream httpData;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &httpData);

		curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_easy_cleanup(curl);

		if (httpCode == 200) {
			//std::cout << "200" << std::endl;
			//Json::Value jsonData;
			Json::CharReaderBuilder jsonReader;
			std::string errs;

			if (Json::parseFromStream(jsonReader, httpData, &jsonData, &errs)) {
				//std::cout << "\nJSON data received: " << std::endl;
				//std::cout << jsonData.toStyledString() << std::endl;	
			}
		}
		else if (httpCode == 0) {
			printf("connection failed, get nothing\n");
			connected = false;
		}
		else {
			printf("httpCode = %d\n", httpCode);
			errorMsg = httpData.str();
		}
		return jsonData;
	}

	Json::Value Tradeapi::Delete(std::string req, std::string params) {

		std::string request;
		Json::Value jsonData;

		request = base_url + req + params;

		std::cout << "request: " + request << std::endl;

		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, request.c_str());
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		struct curl_slist* chunk = NULL;
		std::string keyid = "APCA-API-KEY-ID: " + KeyID;
		std::string secret = "APCA-API-SECRET-KEY: " + SecretKey;


		chunk = curl_slist_append(chunk, keyid.c_str());
		chunk = curl_slist_append(chunk, secret.c_str());

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

		//Response information
		long httpCode(0);
		std::stringstream httpData;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &httpData);

		curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_easy_cleanup(curl);

		if (httpCode == 200) {
			//std::cout << "200" << std::endl;
			//Json::Value jsonData;
			Json::CharReaderBuilder jsonReader;
			std::string errs;

			if (Json::parseFromStream(jsonReader, httpData, &jsonData, &errs)) {
				std::cout << "\nJSON data received: " << std::endl;
				std::cout << jsonData.toStyledString() << std::endl;
			}
		}
		else if (httpCode == 0) {
			printf("connection failed, get nothing\n");
			connected = false;
		}
		else {
			printf("httpCode = %d\n", httpCode);
			errorMsg = httpData.str();
		}
		return jsonData;
	}

	Json::Value Tradeapi::Patch(std::string req, std::string path_params, std::string params) {

		std::string request;
		Json::Value jsonData;

		request = base_url + req + path_params;

		std::cout << "request: " + request << std::endl;

		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, request.c_str());
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		struct curl_slist* chunk = NULL;
		std::string keyid = "APCA-API-KEY-ID: " + KeyID;
		std::string secret = "APCA-API-SECRET-KEY: " + SecretKey;


		chunk = curl_slist_append(chunk, keyid.c_str());
		chunk = curl_slist_append(chunk, secret.c_str());

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());

		//Response information
		long httpCode(0);
		std::stringstream httpData;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &httpData);

		curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_easy_cleanup(curl);

		if (httpCode == 200) {
			//std::cout << "200" << std::endl;
			//Json::Value jsonData;
			Json::CharReaderBuilder jsonReader;
			std::string errs;

			if (Json::parseFromStream(jsonReader, httpData, &jsonData, &errs)) {
				//std::cout << "\nJSON data received: " << std::endl;
				//std::cout << jsonData.toStyledString() << std::endl;	
			}
		}
		else if (httpCode == 0) {
			printf("connection failed, get nothing\n");
			connected = false;
		}
		else {
			printf("httpCode = %d\n", httpCode);
			errorMsg = httpData.str();
		}
		return jsonData;
	}


	//Json::Value Tradeapi::getAccount() {
	Account Tradeapi::get_account() {

		Json::Value resp;
		resp = GET("/account");
		//return resp;
		return Account(resp);
	}

	std::string Tradeapi::submit_order(std::string symbol, int qty, std::string side, std::string type, std::string time_in_force, double limit_price, double stop_price, std::string client_order_id) {

		std::cout << "submit_order" << std::endl;
		Json::Value data;
		Json::Value resp;

		data["symbol"] = symbol;
		data["qty"] = dtos(qty);
		data["side"] = side;
		data["type"] = type;
		data["time_in_force"] = time_in_force;
		data["extended_hours"] = true;
		if (type == "limit") {
			data["limit_price"] = dtos(limit_price);
		}
		if (type == "stop" || type == "stop_limit") {
			data["stop_price"] = dtos(stop_price);
		}

		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		std::string params = Json::writeString(builder, data);

		resp = POST("/orders", params);
		//jsonPrint(resp);
		if (resp.isNull())
			return "";
		return resp["id"].asString();
	}

	std::string Tradeapi::replace_order(std::string orderid, int qty, double limit_price) {

		std::cout << "replace_order" << std::endl;
		Json::Value data;
		Json::Value resp;

		data["qty"] = dtos(qty);
		data["limit_price"] = dtos(limit_price);
		data["type"] = "limit";
		data["time_in_force"] = "day";
		data["extended_hours"] = true;

		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		std::string params = Json::writeString(builder, data);

		resp = Patch("/orders/", orderid, params);
		//jsonPrint(resp);
		if (resp.isNull())
			return "";
		return resp["id"].asString();
	}

	std::vector<Order> Tradeapi::list_orders(std::string status = "open", int limit, std::string after,
		std::string until, std::string direction) {
		//todo: Move out into function
		//std::string params = "?";
		//params += "status=open";
		std::vector<std::string> params;
		params.push_back("status=" + status);
		params.push_back("limit=" + std::to_string(limit));

		Json::Value resp;
		resp = GET("/orders", build_params(params));
		//jsonPrint(resp);
		std::vector<Order> noot(std::begin(resp), std::end(resp));
		return noot;
	}

	std::vector<Activity> Tradeapi::list_activities(int year, int mon, int day) {
		//todo: Move out into function
		//std::string params = "?";
		//params += "status=open";
		char date[15];
		sprintf_s(date, "%4d-%02d-%02d", year, mon, day);
		std::string pam = "date=";
		pam += date;
		std::vector<std::string> params;
		params.push_back(pam);
		params.push_back("activity_types=FILL");
		params.push_back("direction=asc");

		Json::Value resp;
		resp = GET("/account/activities", build_params(params));
		//jsonPrint(resp);
		std::vector<Activity> noot(std::begin(resp), std::end(resp));
		return noot;
	}

	Order Tradeapi::get_order(std::string order_id) {
		Json::Value resp = GET("/orders/" + order_id);
		//Json::Value resp;	
		//jsonPrint(resp);
		return Order(resp);
	}

	Order Tradeapi::get_order_by_client_order_id(std::string client_order_id) {
		std::cout << "get_order_by_client_order\n";
		std::vector<std::string> params;
		params.push_back("client_order_id=" + client_order_id);

		Json::Value resp = GET("/orders:by_client_order_id", build_params(params));
		//jsonPrint(resp);
		return Order(resp);
	}

	bool Tradeapi::cancel_order(std::string order_id) {
		Json::Value resp = Delete("/orders/" + order_id);
		//jsonPrint(resp);
		return !resp.isNull();
	}

	bool Tradeapi::cancel_all_orders() {
		Json::Value resp = Delete("/orders");
		//jsonPrint(resp);
		return !resp.isNull();
	}

	std::vector<Position> Tradeapi::list_positions() {
		Json::Value resp = GET("/positions");

		//jsonPrint(resp);
		std::vector<Position> noot(std::begin(resp), std::end(resp));
		return noot;
	}

	Position Tradeapi::get_position(std::string symbol) {
		Json::Value resp = GET("/positions/" + symbol);
		//jsonPrint(resp);
		if (resp.isNull())
			return Position(symbol); //no open positions
		return Position(resp);
	}

	std::vector<Asset> Tradeapi::list_assets(std::string status, std::string asset_class) {

		std::vector<std::string> params;
		params.push_back("status=" + status);
		params.push_back("asset_class=" + asset_class);
		Json::Value resp = GET("/assets", build_params(params));

		std::vector<Asset> noot(std::begin(resp), std::end(resp));
		return noot;
	}

	Asset Tradeapi::get_asset(std::string symbol) {
		Json::Value resp = GET("/assets/" + symbol);
		//jsonPrint(resp);
		return Asset(resp);
	}

	Json::Value/*std::vector<Bar>*/ Tradeapi::get_barset(std::vector<std::string> symbols, std::string timeframe, int limit,
		std::string start, std::string end, std::string after, std::string until) {

		std::vector<std::string> params;
		std::string symbols_list = "symbols=";
		for (int i = 0; i < symbols.size(); i++) {
			symbols_list += symbols[i];
			if (i < symbols.size() - 1)
				symbols_list += ",";
		}

		params.push_back(symbols_list);
		params.push_back("limit=" + std::to_string(limit));
		params.push_back("start=" + start);
		params.push_back("end=" + end);
		params.push_back("until=" + until);


		Json::Value resp = GET("/bars/" + timeframe, build_params(params), "data.alpaca.markets/v1");
		//std::cout << "BUNNY" << std::endl;
		//std::vector<Bar> noot(std::begin(resp[symbols[0]]),std::end(resp[symbols[0]]));
		//return noot;
		//

		return resp;
	}

	Clock Tradeapi::get_clock() {
		Json::Value resp = GET("/clock");
		jsonPrint(resp);
		return Clock(resp);
	}

	Json::Value Tradeapi::get_calendar(std::string start, std::string end) {

		std::vector<std::string> params;
		params.push_back("start=" + start);
		params.push_back("end=" + end);
		Json::Value resp = GET("/calendar", build_params(params));

		jsonPrint(resp);
		return resp;
		//return Calendar(resp);
	}


}