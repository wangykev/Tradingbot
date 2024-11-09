#include <windows.h>
#include <stdio.h>
#include "SharedMemory.h"
#include <boost/algorithm/string.hpp>

HANDLE  g_hSymList = NULL; //global handle to shared memory
HANDLE  g_hTickList = NULL; //global handle to shared memory
HANDLE  g_hActive = NULL; //global handle to shared memory
HANDLE g_hIBTdPositions = NULL;
HANDLE g_hShareOrders = NULL;
SymbolList* g_pSymboList = NULL;
TickMsg* g_pTickMsg = NULL;
bool * g_pActive = NULL;
SharePosition * g_pIBTdPositions;
ShareOrder* g_pShareOrders;
int writing_tick_index = 0;
int reading_tick_index = 0;

bool shared_initialize()
{
      //Shared Memory Stuff
     //Creates or opens shared memory, depending on whether already exists or not
	g_hActive = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security 
		PAGE_READWRITE,          // read/write access
		0,                       // max. object size 
		10,         // buffer size  
		"Shared Active Flag");    // name of mapping object

	if (NULL == g_hActive || INVALID_HANDLE_VALUE == g_hActive)
	{
		printf("Error occured while creating file mapping object : %d\n", GetLastError());
		return false;
	}

	g_pActive = (bool *)MapViewOfFile(g_hActive,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		10);  //10 flags

	if (NULL == g_pActive)
	{
		printf("Error occured while mapping view of the file : %d\n", GetLastError());
		return false;
	}
	
	g_hSymList = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security 
        PAGE_READWRITE,          // read/write access
        0,                       // max. object size 
        sizeof(SymbolList),         // buffer size  
        "Shared Stock List");    // name of mapping object

    if (NULL == g_hSymList || INVALID_HANDLE_VALUE == g_hSymList)
    {
        printf("Error occured while creating file mapping object : %d\n" , GetLastError() );
        return false;
    }

    g_pSymboList = (SymbolList * )MapViewOfFile(g_hSymList,   // handle to map object
        FILE_MAP_ALL_ACCESS, // read/write permission
        0,
        0,
        sizeof(SymbolList));

    if (NULL == g_pSymboList)
    {
        printf("Error occured while mapping view of the file : %d\n", GetLastError());
        return false;
    }

    g_hTickList = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security 
        PAGE_READWRITE,          // read/write access
        0,                       // max. object size 
        sizeof(TickMsg) * MAX_TICK_MSG,         // buffer size  
        "Shared TICK Messages");    // name of mapping object

    if (NULL == g_hTickList || INVALID_HANDLE_VALUE == g_hTickList)
    {
        printf("Error occured while creating file mapping object : %d\n", GetLastError());
        return false;
    }

    g_pTickMsg = (TickMsg*)MapViewOfFile(g_hTickList,   // handle to map object
        FILE_MAP_ALL_ACCESS, // read/write permission
        0,
        0,
        sizeof(TickMsg) * MAX_TICK_MSG);

    if (NULL == g_pTickMsg)
    {
        printf("Error occured while mapping view of the file : %d\n", GetLastError());
        return false;
    }

	g_hIBTdPositions = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security 
		PAGE_READWRITE,          // read/write access
		0,                       // max. object size 
		sizeof(SharePosition) * MAX_SYMBOL,         // buffer size  
		"Shared IB Td Positions");    // name of mapping object

	if (NULL == g_hIBTdPositions || INVALID_HANDLE_VALUE == g_hIBTdPositions)
	{
		printf("Error occured while creating file mapping object IB Td Positions: %d\n", GetLastError());
		return false;
	}

	g_pIBTdPositions = (SharePosition*)MapViewOfFile(g_hIBTdPositions,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		sizeof(SharePosition) * MAX_SYMBOL);

	if (NULL == g_pIBTdPositions)
	{
		printf("Error occured while mapping view of the file IB Positions : %d\n", GetLastError());
		return false;
	}
	
	g_hShareOrders = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security 
		PAGE_READWRITE,          // read/write access
		0,                       // max. object size 
		sizeof(ShareOrder) * MAX_ORDER,         // buffer size  
		"Shared Trading Orders");    // name of mapping object

	if (NULL == g_hShareOrders)
	{
		printf("Error occured while creating file mapping object Shared Orders : %d\n", GetLastError());
		return false;
	}
	g_pShareOrders = (ShareOrder*)MapViewOfFile(g_hShareOrders,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		sizeof(ShareOrder) * MAX_ORDER);

	if (NULL == g_pShareOrders)
	{
		printf("Error occured while mapping view of the file Shared Orders : %d\n", GetLastError());
		return false;
	}

	if (GetLastError() != ERROR_ALREADY_EXISTS) {
		printf("Zero all the shared memory...\n");
		if (g_pSymboList) memset(g_pSymboList, 0, sizeof(SymbolList));
		if (g_pTickMsg) memset(g_pTickMsg, 0, sizeof(TickMsg) * MAX_TICK_MSG);
		if (g_pIBTdPositions) memset(g_pIBTdPositions, 0, sizeof(SharePosition) * MAX_SYMBOL);
		if (g_pShareOrders) memset(g_pShareOrders, 0, sizeof(ShareOrder) * MAX_ORDER);
		writing_tick_index = 0;
		reading_tick_index = 0;
	}
	else {
		printf("Shared memory exists. Just use it\n");
		writing_tick_index = 0;
		while (g_pTickMsg[writing_tick_index].set == 1 && writing_tick_index < MAX_TICK_MSG - 1) writing_tick_index++;
	}


    return true;
}

void clearSharedTickAndOrders()
{
	//if (g_pSymboList) memset(g_pSymboList, 0, sizeof(SymbolList));
	if (g_pTickMsg) memset(g_pTickMsg, 0, sizeof(TickMsg) * MAX_TICK_MSG);
	//if (g_pIBTdPositions) memset(g_pIBTdPositions, 0, sizeof(SharePosition) * MAX_SYMBOL);
	if (g_pShareOrders) memset(g_pShareOrders, 0, sizeof(ShareOrder) * MAX_ORDER);
	writing_tick_index = 0;
	reading_tick_index = 0;
}

void shared_deinitialize()
{
	UnmapViewOfFile(g_pActive);

	CloseHandle(g_hActive);

	UnmapViewOfFile(g_pSymboList);

	CloseHandle(g_hSymList);

    UnmapViewOfFile(g_pTickMsg);

    CloseHandle(g_hTickList);
}

void add_shared_symbol(std::string& s)
{
    int i;
    for (i = 0; i < g_pSymboList->size; i++) {
        if (s == g_pSymboList->sym[i]) return;
    }
    if (i >= MAX_SYMBOL) return;
    strncpy_s(g_pSymboList->sym[i], s.c_str(), 9);
    g_pSymboList->size = i+1;
}

void add_shared_tick(std::string& sym, int type, double value)
{
    strncpy_s(g_pTickMsg[writing_tick_index].sym, sym.c_str(), 9);
    g_pTickMsg[writing_tick_index].type = type;
    g_pTickMsg[writing_tick_index].value = value;

    int index = writing_tick_index;
    writing_tick_index = (index + 1)% MAX_TICK_MSG;
    g_pTickMsg[writing_tick_index].set = 0;
    g_pTickMsg[index].set = 1;

	int i;
	for (i = 0; i < MAX_SYMBOL; i++) {
		if (sym == (g_pIBTdPositions[i].sym)) {
			switch (type) {
			case 1:
				g_pIBTdPositions[i].bidPrice = value;
				break;
			case 2:
				g_pIBTdPositions[i].askPrice = value;
				break;
			case 4:
				g_pIBTdPositions[i].lastPrice = value;
				break;
			case 6:
				g_pIBTdPositions[i].dayHigh = value;
				break;
			case 7:
				g_pIBTdPositions[i].dayLow = value;
				break;
			case 9:
				g_pIBTdPositions[i].closePrice = value;
				break;
			case 0:
				g_pIBTdPositions[i].bidSize = value;
				break;
			case 3:
				g_pIBTdPositions[i].askSize = value;
				break;
			case 5:
				g_pIBTdPositions[i].lastSize = value;
				break;
			case 8:
				g_pIBTdPositions[i].dayVolume = value;
				break;
			default:
				break;
			}
		}
	}
}

void update_stock_range(std::string& sym, double top, double bottom, double multiplier, double offset, double previousClose, double stdVariation, double lowBoundDiscount)
{
	int i;
	for (i = 0; i < MAX_SYMBOL; i++) {
		if (sym == (g_pIBTdPositions[i].sym)) {
			g_pIBTdPositions[i].topPrice = top;
			g_pIBTdPositions[i].bottomPrice = bottom;
			g_pIBTdPositions[i].multiplier = multiplier;
			g_pIBTdPositions[i].offset = offset;
			g_pIBTdPositions[i].previousClose = previousClose;
			g_pIBTdPositions[i].stdVariation = stdVariation;
			g_pIBTdPositions[i].lowBoundDiscount = lowBoundDiscount;
		}
	}

}
void write_IB_position(std::string &sym, double position)
{
	int i;
	for (i = 0; i < MAX_SYMBOL; i++) {
		if (sym == (g_pIBTdPositions[i].sym)) {
			g_pIBTdPositions[i].IBset = false;
			g_pIBTdPositions[i].IBPosition = position;
			g_pIBTdPositions[i].IBset = true;
			return;
		}
		else if (g_pIBTdPositions[i].sym[0] == '\0') {
			strncpy_s(g_pIBTdPositions[i].sym, sym.c_str(), 9);
			g_pIBTdPositions[i].IBset = false;
			g_pIBTdPositions[i].IBPosition = position;
			g_pIBTdPositions[i].IBset = true;
			return;
		}
	}
}
void write_Td_position(std::string &sym, double position)
{
	int i;
	for (i = 0; i < MAX_SYMBOL; i++) {
		if (sym == (g_pIBTdPositions[i].sym)) {
			g_pIBTdPositions[i].Tdset = false;
			g_pIBTdPositions[i].TdPosition = position;
			g_pIBTdPositions[i].Tdset = true;
			return;
		}
		else if (g_pIBTdPositions[i].sym[0] == '\0') {
			strncpy_s(g_pIBTdPositions[i].sym, sym.c_str(), 9);
			g_pIBTdPositions[i].Tdset = false;
			g_pIBTdPositions[i].TdPosition = position;
			g_pIBTdPositions[i].Tdset = true;
			return;
		}
	}

}
bool read_IB_position(std::string &sym, double &position)
{
	int i;
	for (i = 0; i < MAX_SYMBOL; i++) {
		if (sym == (g_pIBTdPositions[i].sym)) {
			for (int j = 0; j < 5; j++) {  //read 5 times;
				if (g_pIBTdPositions[i].IBset) {
					position = g_pIBTdPositions[i].IBPosition;
					return true;
				}
				else Sleep(0);
			}
			return false;
		}
		else if (g_pIBTdPositions[i].sym[0] == '\0') {
			return false;
		}
	}
	return false;
}
bool read_Td_position(std::string &sym, double &position)
{
	int i;
	for (i = 0; i < MAX_SYMBOL; i++) {
		if (sym == (g_pIBTdPositions[i].sym)) {
			for (int j = 0; j < 5; j++) {  //read 5 times;
				if (g_pIBTdPositions[i].Tdset) {
					position = g_pIBTdPositions[i].TdPosition;
					return true;
				}
				else Sleep(0);
			}
			return false;
		}
		else if (g_pIBTdPositions[i].sym[0] == '\0') {
			return false;
		}
	}
	return false;
}


const char* ReadConsoleBuffer(char** buffer, size_t* bufsize)
{
	CONSOLE_SCREEN_BUFFER_INFO buffer_info = { 0 };
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &buffer_info) != FALSE)
	{
		DWORD data_size = (DWORD)((buffer_info.dwSize.X * (buffer_info.dwCursorPosition.Y + 1)) -
			(buffer_info.dwSize.X - (buffer_info.dwCursorPosition.X + 1)));
		printf("data_size = %d\n", data_size);

		if (data_size > 1)
		{
			char* data = (char*)malloc(data_size + 1);
			_ASSERTE(data != 0);

			DWORD num_char_read;
			COORD first_char_read = { 0,0 };
			if (ReadConsoleOutputCharacterA(GetStdHandle(STD_OUTPUT_HANDLE), data, data_size, first_char_read, &num_char_read) != FALSE)
			{
				if (num_char_read < data_size) data_size = num_char_read;
				data[data_size] = '\0';


				char* pbeg = &data[0];
				char* pend = &data[data_size] + 1;
				char* pcur, *pmem = 0;

				int line_count = buffer_info.dwCursorPosition.Y;
				int line_size = buffer_info.dwSize.X;
				printf("line_count = %d, line_size = %d, data_size = %d\n", line_count, line_size, data_size);

				for (int i = line_count - 1; i >= 0; i--) {
					pbeg = &data[i * line_size];
					pmem = pbeg + line_size;
					pcur = pmem - 1;
					if (*pcur == ' ' && pcur[-1] == ' ') {
						while (pcur >= pbeg && *pcur == ' ') { pcur--; }
						*(pcur + 1) = '\n';
						if (pmem > pcur + 2) {
							memmove(pcur + 2, pmem, pend - pmem);
							pend -= (pmem - (pcur + 2));
						}
					}
				}

				*bufsize = pend - data;
				//printf("bufsize = %d\n", *bufsize);

				*buffer = (char*)malloc(*bufsize); //= new char[*bufsize];
				_ASSERTE(*buffer != 0);

				memcpy(*buffer, data, *bufsize);
				free(data);

				pcur = *buffer;
				return pcur;
			}

			if (data)
				free(data); // delete[] data;
		}
	}

	*buffer = 0;
	return 0;
}

void outputBuffer(const char* filename)
{
	char* buffer;
	size_t size;
	ReadConsoleBuffer(&buffer, &size);
	//printf("buffersize = %d\n", size);

	char* start = buffer;
	char* find = strstr(start, "Output console buffer success!");;
	while (find) {
		start = find + 31;
		find = strstr(start, "Output console buffer success!");
	}

	if (start)
	{
		FILE* f;
		fopen_s(&f, filename, "a");
		if (f) {
			fprintf(f, "%s", start);
			free(buffer);
			fclose(f);
			printf("Output console buffer success!\n");
			return;
		}
	}
	printf("Failed output console buffer file\n");
}


//first order used for maximum order index by qty. 
void updateShareOrder(std::string& id, std::string& sym, std::string action, std::string status, double qty, double filled_qty, double price, double profit, double commission, std::string broker)
{
	boost::to_upper(action);
	boost::to_upper(status);
	int maxIndex = round(g_pShareOrders[0].qty);
	if (maxIndex <= 0) { maxIndex = 1;  g_pShareOrders[0].qty = 1.0; }
	int i; 
	if (profit < 0.001 && filled_qty < 0.1 && (status == "CANCELLED" || status == "CANCELED" || status == "EXPIRED" || status == "REJECTED" || status == "REPLACED")) {
		for (i = 1; i <= maxIndex; i++) {
			if (g_pShareOrders[i].sym == sym && g_pShareOrders[i].id == id) {
				g_pShareOrders[i].sym[0] = '\0';
				g_pShareOrders[i].id[0] = '\0';
				return;
			}
		}
	}
	else {
		for (i = 1; i <= maxIndex; i++) {
			if (g_pShareOrders[i].sym == sym && g_pShareOrders[i].id == id) {
				strncpy_s(g_pShareOrders[i].action, action.c_str(), 9);
				strncpy_s(g_pShareOrders[i].status, status.c_str(), 19);
				g_pShareOrders[i].qty = qty;
				g_pShareOrders[i].filled_qty = filled_qty;
				g_pShareOrders[i].price = price;
				g_pShareOrders[i].profit = profit;
				g_pShareOrders[i].commission = commission;
				strncpy_s(g_pShareOrders[i].broker, broker.c_str(), 19);

				g_pShareOrders[i].time = time(NULL);
				return;
			}
		}

		//not found, insert to empty slot
		for (i = 1; i <= maxIndex; i++) {
			if (g_pShareOrders[i].sym[0] == '\0' && g_pShareOrders[i].id[0] == '\0') {
				strncpy_s(g_pShareOrders[i].sym, sym.c_str(), 9);
				strncpy_s(g_pShareOrders[i].id, id.c_str(), 49);
					
				strncpy_s(g_pShareOrders[i].action, action.c_str(), 9);
				strncpy_s(g_pShareOrders[i].status, status.c_str(), 19);
				g_pShareOrders[i].qty = qty;
				g_pShareOrders[i].filled_qty = filled_qty;
				g_pShareOrders[i].price = price;
				g_pShareOrders[i].profit = profit;
				g_pShareOrders[i].commission = commission;
				strncpy_s(g_pShareOrders[i].broker, broker.c_str(), 19);
				g_pShareOrders[i].time = time(NULL);

				return;
			}
		}

		//no empty slot
		i = maxIndex + 1;
		if (i >= MAX_ORDER) {
			printf("Order number reached Maximum, no slot in shared memory\n");
			return;
		}
		strncpy_s(g_pShareOrders[i].sym, sym.c_str(), 9);
		strncpy_s(g_pShareOrders[i].id, id.c_str(), 49);

		strncpy_s(g_pShareOrders[i].action, action.c_str(), 9);
		strncpy_s(g_pShareOrders[i].status, status.c_str(), 19);
		g_pShareOrders[i].qty = qty;
		g_pShareOrders[i].filled_qty = filled_qty;
		g_pShareOrders[i].price = price;
		g_pShareOrders[i].profit = profit;
		g_pShareOrders[i].commission = commission;
		strncpy_s(g_pShareOrders[i].broker, broker.c_str(), 19);
		g_pShareOrders[i].time = time(NULL);

		g_pShareOrders[0].qty = i;
		return;
	}
}