/* Copyright (C) 2019 Interactive Brokers LLC. All rights reserved. This code is subject to the terms
 * and conditions of the IB API Non-Commercial License or the IB API Commercial License, as applicable. */

#include "StdAfx.h"
#include "FolderDefinition.h"

#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <thread>

#include "TestCppClient.h"
#include "StockAndAccount.h"
#include "SharedMemory.h"

#include "tdma_common.h"
#include "tdma_api_get.h"
#include "tdma_api_execute.h"
#include "AlpacaStockAndAccount.h"
#include "Utils.h"

WangAccount* tdAccount1 = NULL;
AlpacaAccount* alpacaAccount1 = NULL;

int IB_Active, TD_Active1;

/* IMPORTANT: always use your paper trading account. The code below will submit orders as part of the demonstration. */
/* IB will not be responsible for accidental executions on your live account. */
/* Any stock or option symbols displayed are for illustrative purposes only and are not intended to portray a recommendation. */
/* Before contacting our API support team please refer to the available documentation. */
void main_IB()
{
	const char* host = "";
	int port = 7496;
	const char* connectOptions = "";
	int clientId = 0;

	unsigned attempt = 0;

	for (;;) {
		IB_Active = 1;
		++attempt;
		printf("IB connection Attempt %u\n", attempt);

		TestCppClient client;

		// Run time error will occur (here) if TestCppClient.exe is compiled in debug mode but TwsSocketClient.dll is compiled in Release mode
		// TwsSocketClient.dll (in Release Mode) is copied by API installer into SysWOW64 folder within Windows directory 

		if (connectOptions) {
			client.setConnectOptions(connectOptions);
		}

		client.connect(host, port, clientId);
		std::this_thread::sleep_for(std::chrono::seconds(1));

		if (client.isConnected()) {
			attempt = 0;
			client.wangRequestMarketData();
			client.wangUpdateOrdersAndPositions();
		}

		while (client.isConnected()) {
			IB_Active = 1;
			client.wangJob();
			client.wangRequestMarketData();
		}
		printf("Sleeping %u seconds before next attempt\n", 10);
		std::this_thread::sleep_for(std::chrono::seconds(10));
		return;  //exit.
	}

}

std::vector<int> soundPlay;
void playSoundThread()
{
	std::this_thread::sleep_for(std::chrono::seconds(10));  //wait for 20 seconds to erase the initialization update sound.
	soundPlay.clear();
	for (;;) {
		if (soundPlay.size() > 4) {
			while (soundPlay.size() > 4)
				soundPlay.erase(soundPlay.begin());
		}
		else if (soundPlay.size() > 0) {
			int s = soundPlay[0];
			if (s == 1) {
				PlaySound(TEXT("orderfilled.wav"), NULL, SND_FILENAME);
			}
			else if (s == 2) {
				PlaySound(TEXT("orderpartiallyfilled.wav"), NULL, SND_FILENAME);
			}
			else if (s == 3) {
				PlaySound(TEXT("ordercancelled.wav"), NULL, SND_FILENAME);
			}
			soundPlay.erase(soundPlay.begin());
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
}

int main_TD(WangAccount* tdAccount, int* Active)
{
	if (!tdAccount) return 0;
	tdAccount->ProcessControlFile();

	unsigned attempt = 0;
	for (;;) {
		*Active = 1;

		++attempt;
		printf("TD connection Attempt %u\n", attempt);
		tdAccount->connect();

		if (tdAccount->isConnected()) {
			attempt = 0;
			tdAccount->updateAccount();
		}

		while (tdAccount->isConnected()) {
			*Active = 1;
			tdAccount->wangJob();
		}
		printf("Sleeping %u seconds before next attempt\n", 10);
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
	return 1;
}

int main_AL(AlpacaAccount* tdAccount, int* Active)
{
	if (!tdAccount) return 0;
	tdAccount->ProcessControlFile();

	unsigned attempt = 0;
	for (;;) {
		*Active = 1;

		++attempt;
		printf("TD connection Attempt %u\n", attempt);
		tdAccount->connect();

		if (tdAccount->isConnected()) {
			attempt = 0;
			tdAccount->updateAccount();
		}

		while (tdAccount->isConnected()) {
			*Active = 1;
			tdAccount->wangJob();
		}
		printf("Sleeping %u seconds before next attempt\n", 10);
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
	return 1;
}


//start this program with different arguments.
int main(int argc, char** argv)
{
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), {106, 9999});
	bool isIB = false;
	bool isTD = false;
	tdAccount1 = NULL;
	std::thread* t1, * tIB;
	std::thread soundThread(playSoundThread);
	shared_initialize();
	int acc = 0;

	if (argc < 2) { 
		isIB = true;
		printf("Start of Interactive Brokers C++ Socket Client\n");
		tIB = new std::thread(main_IB);
		acc = 0;
		std::this_thread::sleep_for(std::chrono::milliseconds(300)); //wait for IB thread a little.
	}
	else if (argv[1][0] == '0') {
		printf("Start TD account of Wang...\n");
		tdAccount1 = new WangAccount(TD_ACCOUNT1, TD_CRED_TOKEN_PATH1, TD_TOKEN_PASSWD1, TD_CONTROLFILENAME1, 0);    //Individual - Wang
		isTD = true;
		acc = 1;
	}
	else if (argv[1][0] == 'a') {
		printf("Start Alpaca account of Wang...\n");
		alpacaAccount1 = new AlpacaAccount(ALPACA_STREAMING_URL, ALPACA_KEY_ID, ALPACA_SECURE_KEY, AL_CONTROLFILENAME1, 0);    //Individual - Wang
		isTD = true;
		acc = 1;
	}
	else if (argv[1][0] == '1') {
		printf("Start TD account of Roth-Wang...\n");
		tdAccount1 = new WangAccount(TD_ACCOUNTR1, TD_CRED_TOKEN_PATHR1, TD_TOKEN_PASSWDR1, TD_CONTROLFILENAMER1, 1);     //Roth-Wang
		isTD = true;
		acc = 2;
	}
	else if (argv[1][0] == '2') {
		printf("Start TD account of Roth-Bo...\n");
		tdAccount1 = new WangAccount(TD_ACCOUNTR2, TD_CRED_TOKEN_PATHR2, TD_TOKEN_PASSWDR2, TD_CONTROLFILENAMER2, 2);		//Roth-Bo

		isTD = true;
		acc = 3;
	}
	else if (argv[1][0] == '3') {
		printf("Start TD account of Roth-Zhensheng...\n");
		tdAccount1 = new WangAccount(TD_ACCOUNTR3, TD_CRED_TOKEN_PATHR3, TD_TOKEN_PASSWDR3, TD_CONTROLFILENAMER3, 3);		//Roth-Zhensheng

		isTD = true;
		acc = 4;
	}
	else if (argv[1][0] == '4') {
		printf("Start TD account of Roth-Wei...\n");
		tdAccount1 = new WangAccount(TD_ACCOUNTR4, TD_CRED_TOKEN_PATHR4, TD_TOKEN_PASSWDR4, TD_CONTROLFILENAMER4, 4);		//Roth-Wei

		isTD = true;
		acc = 5;
	}
	g_pActive[acc] = true;

	if (tdAccount1) {
		t1 = new std::thread(main_TD, tdAccount1, &TD_Active1);
		std::this_thread::sleep_for(std::chrono::milliseconds(300)); //wait a little.
	}
	if (alpacaAccount1) {
		t1 = new std::thread(main_AL, alpacaAccount1, &TD_Active1);
		std::this_thread::sleep_for(std::chrono::milliseconds(300)); //wait a little.
	}


	int i = 0;
	int j = 0; 
	int nSec = 30;  //every 30 seconds.
	for (;;) {
		//check other program is dead or not. every 30 seconds.
		if (i == 0) {  
			if (acc == 0) {
				g_pActive[1] = g_pActive[2] = g_pActive[3] = g_pActive[4] = g_pActive[5] = false;
			}
			else if (acc == 1) {
				g_pActive[0] = false;
			}
		}
		else if (i == nSec - 1) {
			if (acc == 0) {
				if (!g_pActive[1]) {
					rprintf("Start the program for Individual Wang\n");
					//ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, "0", NULL, SW_SHOWDEFAULT);  //TD Ameritrade
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, "a", NULL, SW_SHOWDEFAULT);    //Alpaca
				}
				/*
				if (!g_pActive[2]) {
					rprintf("Start the program for IRA Wang\n");
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, "1", NULL, SW_SHOWDEFAULT);
				}
				if (!g_pActive[3]) {
					rprintf("Start the program for IRA Bo Yu\n");
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, "2", NULL, SW_SHOWDEFAULT);
				}
				if (!g_pActive[4]) {
					rprintf("Start the program for IRA Zhensheng Yu\n");
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, "3", NULL, SW_SHOWDEFAULT);
				}
				if (!g_pActive[5]) {
					rprintf("Start the program for IRA Wei Dai\n");
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, "4", NULL, SW_SHOWDEFAULT);
				}
				*/
			}
			else if (acc == 1) {
				if (!g_pActive[0]) {
					rprintf("Start the IB program\n");
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, NULL, NULL, SW_SHOWDEFAULT);
				}
			}
		}

		g_pActive[acc] = true;
		i = (i + 1) % nSec;

		//check if the thread is dead or not.  Every 3 minutes
		if (j == 0) {
			IB_Active = TD_Active1 = 0;
		}
		else if (j == 180 - 1) {
			if ((isIB && IB_Active == 0) || (tdAccount1 && TD_Active1 == 0)) {
				printf("Dead Program\n");
				printf("Lanching new process...\n");
				if (argc < 2)
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, NULL, NULL, SW_SHOWDEFAULT);
				else
					ShellExecute(NULL, "open", PRAGRAM_FULL_PATH, argv[1], NULL, SW_SHOWDEFAULT);
				exit(1);
			}
		}
		j = (j + 1) % 180;

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return 1;
}


