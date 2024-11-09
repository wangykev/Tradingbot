#include "AlpacaStream.h"
#include "FolderDefinition.h"

namespace alpaca {
	Stream::Stream() {
		pClient = NULL;
	}

	Stream::~Stream() {
		if (pClient) delete pClient;
	}

/*
	Stream::Stream(Json::Value data) {
		pClient = NULL;
		json = data;
	}

	Json::Value Stream::account_updates() {


	}
	*/
	void callBackFunction(web::websockets::client::websocket_incoming_message & msg)
	{
		//Parse server response, whether it is trade_update or account_update
		std::cout << msg.length() << std::endl;
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
		reader->parse(bufStr, bufStr + msg.length()+1, &noot, &errs);
		//std::cout << bufStr << std::endl;
		//std::cout << noot.toStyledString() << std::endl;

		//logged.push_back(noot);
		delete[] bufStr;
	}

	void Stream::trade_updates() {

		Json::Value stream;
		Json::Value data;
		Json::Value root;

		stream.append("trade_updates");
		data["streams"] = stream;
		root["action"] = "listen";
		root["data"] = data;
	}

	void Stream::init(const std::function<void(const web::websockets::client::websocket_incoming_message& msg)>& callback) {
		pClient = new web::websockets::client::websocket_callback_client;
		pClient->set_message_handler(callback);
	}

	void Stream::connect(std::string KeyID, std::string SecretKey) {

		web::uri_builder b;
		b.set_scheme(U("wss"));
		b.set_host(U(ALPACA_STREAMING_URL));
		b.set_path(U("stream"));

		pClient->connect(b.to_uri()).wait();
		//client.connect(()EndPoint).wait();

		Json::Value data;
		/* Authentication */
		data["key_id"] = KeyID;
		data["secret_key"] = SecretKey;

		Json::Value root;
		root["action"] = "authenticate";
		root["data"] = data;

		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		std::string params = Json::writeString(builder, root);

		web::websockets::client::websocket_outgoing_message out_msg;
		out_msg.set_utf8_message(params);
		pClient->send(out_msg).wait();
	}

	void Stream::disconnect() {
		pClient->close();
		delete pClient;
		pClient = NULL;
	}

	void Stream::sendping()
	{
		web::websockets::client::websocket_outgoing_message out_msg;
		out_msg.set_ping_message("Ping");
		pClient->send(out_msg).wait();
	}


	void Stream::subscribe(std::vector<std::string> selected_streams) {

		Json::Value streams;
		Json::Value data;
		Json::Value root;

		for (int i = 0; i < selected_streams.size(); i++)
			streams.append(selected_streams[i]);

		data["streams"] = streams;
		root["action"] = "listen";
		root["data"] = data;

		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		std::string params = Json::writeString(builder, root);

		web::websockets::client::websocket_outgoing_message out_msg;
		out_msg.set_utf8_message(params);
		pClient->send(out_msg).wait();
	}

}