/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */
#include "StdAfx.h"

#include "Utils.h"
#include <iostream>
#include <platformspecific.h>

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

inline bool Utils::is_base64(std::uint8_t c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::vector<std::uint8_t> Utils::base64_decode(std::string const& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
   int in_ = 0;
    std::uint8_t char_array_4[4], char_array_3[3];
    std::vector<std::uint8_t> ret;

    while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
        char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
    }

    return ret;
}

std::string Utils::formatDoubleString(std::string const& str) {
    char buf[25];
    snprintf(buf, sizeof(buf), "%.2f", strtod(str.c_str(), NULL));
    return buf;
}


using std::string;

#pragma comment(lib,"ws2_32.lib")


HINSTANCE hInst;
WSADATA wsaData;
void mParseUrl(char *mUrl, string &serverName, string &filepath, string &filename);
SOCKET connectToServer(char *szServerName, WORD portNum);
int getHeaderLength(char *content);
char *readUrl2(char *szUrl, long &bytesReturnedOut, char **headerOut);

void getStockPriceFromYahoo(const char* symbol)
{
	char szUrl[300];
	sprintf(szUrl, "https://query1.finance.yahoo.com/v7/finance/download/%s?period1=1622745800&period2=1654281800&interval=1d&events=history&includeAdjustedClose=true", symbol);  
	long fileSize;
	char* memBuffer, * headerBuffer;

	memBuffer = headerBuffer = NULL;

	if (WSAStartup(0x101, &wsaData) != 0) return;


	memBuffer = readUrl2(szUrl, fileSize, &headerBuffer);
	//printf("returned from readUrl\n");
	//printf("data returned:\n%s", memBuffer);
	double open;
	double fiveBidPrice[5];
	int fiveBidSize[5];
	double fiveAskPrice[5];
	int fiveAskSize[5];
	double samount;
	if (fileSize != 0)
	{
		//printf("Got some data\n");
		printf("%s\n", memBuffer);
		delete(memBuffer);
		delete(headerBuffer);
	}

	WSACleanup();

}

void getChinaStockPrice(const char * symbol, double &askPrice, double &bidPrice, int &askSize, int & bidSize, double & lastPrice, double & dayHigh, double & dayLow, double & closePrice, int & dayVolume)
{
	const int bufLen = 1024;
	char szUrl[100];
	if (symbol[0] == '0') sprintf(szUrl, "http://hq.sinajs.cn/list=sz%s", symbol);   //"sz002459"
	else sprintf(szUrl, "http://hq.sinajs.cn/list=sh%s", symbol);
	long fileSize;
	char *memBuffer, *headerBuffer;

	memBuffer = headerBuffer = NULL;

	if (WSAStartup(0x101, &wsaData) != 0) return;


	memBuffer = readUrl2(szUrl, fileSize, &headerBuffer);
	//printf("returned from readUrl\n");
	//printf("data returned:\n%s", memBuffer);
	double open;
	double fiveBidPrice[5];
	int fiveBidSize[5];
	double fiveAskPrice[5];
	int fiveAskSize[5];
	double samount;
	if (fileSize != 0)
	{
		//printf("Got some data\n");
		char * s = strstr(memBuffer, symbol);
		if (s) {  //find the stock
			s = strstr(s, ",");
			if (s) {
				sscanf(s + 1, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,%d,%lf,", &open, &closePrice, &lastPrice, &dayHigh, &dayLow, &bidPrice, &askPrice, &dayVolume, &samount, &fiveBidSize[0], &fiveBidPrice[0], &fiveBidSize[1], &fiveBidPrice[1], &fiveBidSize[2], &fiveBidPrice[2], &fiveBidSize[3], &fiveBidPrice[3], &fiveBidSize[4], &fiveBidPrice[4],
					&fiveAskSize[0], &fiveAskPrice[0], &fiveAskSize[1], &fiveAskPrice[1], &fiveAskSize[2], &fiveAskPrice[2], &fiveAskSize[3], &fiveAskPrice[3], &fiveAskSize[4], &fiveAskPrice[4]);
				askSize = fiveAskSize[0]/100;
				bidSize = fiveBidSize[0]/100;
			}
		}
		delete(memBuffer);
		delete(headerBuffer);
	}

	WSACleanup();
}


void mParseUrl(char *mUrl, string &serverName, string &filepath, string &filename)
{
	string::size_type n;
	string url = mUrl;

	if (url.substr(0, 7) == "http://")
		url.erase(0, 7);

	if (url.substr(0, 8) == "https://")
		url.erase(0, 8);

	n = url.find('/');
	if (n != string::npos)
	{
		serverName = url.substr(0, n);
		filepath = url.substr(n);
		n = filepath.rfind('/');
		filename = filepath.substr(n + 1);
	}

	else
	{
		serverName = url;
		filepath = "/";
		filename = "";
	}
}

SOCKET connectToServer(char *szServerName, WORD portNum)
{
	struct hostent *hp;
	unsigned int addr;
	struct sockaddr_in server;
	SOCKET conn;

	conn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (conn == INVALID_SOCKET)
		return NULL;

	if (inet_addr(szServerName) == INADDR_NONE)
	{
		hp = gethostbyname(szServerName);
	}
	else
	{
		addr = inet_addr(szServerName);
		hp = gethostbyaddr((char*)&addr, sizeof(addr), AF_INET);
	}

	if (hp == NULL)
	{
		closesocket(conn);
		return NULL;
	}

	server.sin_addr.s_addr = *((unsigned long*)hp->h_addr);
	server.sin_family = AF_INET;
	server.sin_port = htons(portNum);
	if (connect(conn, (struct sockaddr*)&server, sizeof(server)))
	{
		closesocket(conn);
		return NULL;
	}
	return conn;
}

int getHeaderLength(char *content)
{
	const char *srchStr1 = "\r\n\r\n", *srchStr2 = "\n\r\n\r";
	char *findPos;
	int ofset = -1;

	findPos = strstr(content, srchStr1);
	if (findPos != NULL)
	{
		ofset = findPos - content;
		ofset += strlen(srchStr1);
	}

	else
	{
		findPos = strstr(content, srchStr2);
		if (findPos != NULL)
		{
			ofset = findPos - content;
			ofset += strlen(srchStr2);
		}
	}
	return ofset;
}

char *readUrl2(char *szUrl, long &bytesReturnedOut, char **headerOut)
{
	const int bufSize = 512;
	char readBuffer[bufSize], sendBuffer[bufSize], tmpBuffer[bufSize];
	char *tmpResult = NULL, *result;
	SOCKET conn;
	string server, filepath, filename;
	long totalBytesRead, thisReadSize, headerLen;

	mParseUrl(szUrl, server, filepath, filename);

	///////////// step 1, connect //////////////////////
	conn = connectToServer((char*)server.c_str(), 80);

	///////////// step 2, send GET request /////////////
	sprintf(tmpBuffer, "GET %s HTTP/1.0", filepath.c_str());
	strcpy(sendBuffer, tmpBuffer);
	strcat(sendBuffer, "\r\n");
	sprintf(tmpBuffer, "Host: %s", server.c_str());
	strcat(sendBuffer, tmpBuffer);
	strcat(sendBuffer, "\r\n");
	strcat(sendBuffer, "\r\n");
	send(conn, sendBuffer, strlen(sendBuffer), 0);

	//    SetWindowText(edit3Hwnd, sendBuffer);
	//printf("Buffer being sent:\n%s", sendBuffer);

	///////////// step 3 - get received bytes ////////////////
	// Receive until the peer closes the connection
	totalBytesRead = 0;
	while (1)
	{
		memset(readBuffer, 0, bufSize);
		thisReadSize = recv(conn, readBuffer, bufSize, 0);

		if (thisReadSize <= 0)
			break;

		tmpResult = (char*)realloc(tmpResult, thisReadSize + totalBytesRead);

		memcpy(tmpResult + totalBytesRead, readBuffer, thisReadSize);
		totalBytesRead += thisReadSize;
	}

	headerLen = getHeaderLength(tmpResult);
	long contenLen = totalBytesRead - headerLen;
	result = new char[contenLen + 1];
	memcpy(result, tmpResult + headerLen, contenLen);
	result[contenLen] = 0x0;
	char *myTmp;

	myTmp = new char[headerLen + 1];
	strncpy(myTmp, tmpResult, headerLen);
	myTmp[headerLen] = NULL;
	delete(tmpResult);
	*headerOut = myTmp;

	bytesReturnedOut = contenLen;
	closesocket(conn);
	return(result);
}