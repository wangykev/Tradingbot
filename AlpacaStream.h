#ifndef ALPACASTREAM_H
#define ALPACASTREAM_H

#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <json/json.h>
#include <list>
#include <cpprest/ws_client.h>

namespace alpaca {

	class Stream {
	public:
		Stream();
		Stream(Json::Value);
		~Stream();

		void init(const std::function<void(const web::websockets::client::websocket_incoming_message& msg)>& callback);
		void connect(std::string KeyID, std::string SecretKey);
		void sendping();
		void disconnect();
		void subscribe(std::vector<std::string> streams);
		Json::Value account_updates();
		void trade_updates();
		//Keep Json
		Json::Value json;

		std::list<Json::Value> logged;

		bool is_active() {
			return true;  //need work...
		}
	private:
		//int ret_data_count=0; 	// for seeking when storing data in ret_data buffer
		//concurrency::streams::container_buffer<std::vector<uint8_t>> ret_data;
		web::websockets::client::websocket_callback_client * pClient;

	};

}

#endif

