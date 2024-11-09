#pragma once

#ifndef FOLDERDEFINITION_H
#define FOLDERDEFINITION_H

#ifdef DEBUG
#define PRAGRAM_FULL_PATH "C:\\TradingRobot\\Debug\\TradingRobot.exe"
#else
#define PRAGRAM_FULL_PATH "C:\\TradingRobot\\Release\\TradingRobot.exe"
#endif

//#define ACCOUNT1 "U5992390"      //IB first account
#define ACCOUNT2 "U4674504"
#define ACCOUNT1 "U4905334"     //IB second account, if no second account, leave it as is.


#define IB_CONTROLFILENAME			"C:\\Google Drive\\stock-tax\\StockControl\\StockControl.txt"

#define IB_CONTROLFILENAME_CURRENT	"C:\\Google Drive\\stock-tax\\StockControl\\StockControl_Current.txt"



#define TD_CONTROLFILENAME1			"C:\\Google Drive\\stock-tax\\StockControl\\TdStockControl.txt" 
#define TD_ACCOUNT1 "237744110"
#define TD_CRED_TOKEN_PATH1 "C:\\Google Drive\\stock-tax\\StockControl\\token\\token.txt"
#define TD_TOKEN_PASSWD1 "Yubor123"
#define TD_OUTPUTILENAME1			"C:\\Google Drive\\stock-tax\\StockControl\\TdScreenOutput.txt" 

#define TD_CONTROLFILENAMER1			"C:\\Google Drive\\stock-tax\\StockControl\\TdStockControlRothWang.txt" 
#define TD_ACCOUNTR1 "238227219"
#define TD_CRED_TOKEN_PATHR1 "C:\\Google Drive\\stock-tax\\StockControl\\token\\tokenrothwang.txt"
#define TD_TOKEN_PASSWDR1 "Yubor123"

#define TD_CONTROLFILENAMER2			"C:\\Google Drive\\stock-tax\\StockControl\\TdStockControlRothBo.txt" 
#define TD_ACCOUNTR2 "238226224"
#define TD_CRED_TOKEN_PATHR2 "C:\\Google Drive\\stock-tax\\StockControl\\token\\tokenrothbo.txt"
#define TD_TOKEN_PASSWDR2 "Yubor123"

#define TD_CONTROLFILENAMER3			"C:\\Google Drive\\stock-tax\\StockControl\\TdStockControlRothZhensheng.txt" 
#define TD_ACCOUNTR3 "238225510"
#define TD_CRED_TOKEN_PATHR3 "C:\\Google Drive\\stock-tax\\StockControl\\token\\tokenrothzhensheng.txt"
#define TD_TOKEN_PASSWDR3 "Yubor123"

#define TD_CONTROLFILENAMER4			"C:\\Google Drive\\stock-tax\\StockControl\\TdStockControlRothWei.txt" 
#define TD_ACCOUNTR4 "238225903"
#define TD_CRED_TOKEN_PATHR4 "C:\\Google Drive\\stock-tax\\StockControl\\token\\tokenrothwei.txt"
#define TD_TOKEN_PASSWDR4 "Yubor123"






#define AL_CONTROLFILENAME1			"C:\\Google Drive\\stock-tax\\StockControl\\AlpacaStockControl.txt" 
#ifdef PAPER_TRADING
#define ALPACA_STREAMING_URL "paper-api.alpaca.markets"  
#define ALPACA_KEY_ID "PK3F9QNNB9CC8MZ8OWBB"
#define ALPACA_SECURE_KEY "F8Xmbspx58rDsOkFYco4yO6Ocnvbk7GsXMbUi78P"
#else
#define ALPACA_STREAMING_URL "api.alpaca.markets" 
#define ALPACA_KEY_ID "AKSK790LY9BNF24W97ZN"
#define ALPACA_SECURE_KEY "xtq35u3vCjURohpsq9LDWZ3lB1Hn3oJ9yP1sdUqC"
#endif


#define ALPACA_OUTPUT_FOLDER "C:\\TradingRobot\\"
#define ALPACA_ACTIVITY_FOLDER "C:\\Google Drive\\stock-tax\\\AlpacaActivities\\"

#endif
