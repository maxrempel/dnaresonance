#include <ctime>
#include <fstream>
#include <sstream>

#include "Util.h"

using namespace std;

// TODO: variable format of the timestamp string.
string Util::TimestampCurrent()
{
	time_t current_time;
	time(&current_time);

	tm timeinfo;
	localtime_s(&timeinfo, &current_time);

	char timebuffer[20];
	strftime(timebuffer, 20, "%Y/%m/%d-%H:%M:%S", &timeinfo); // YYYYMMDD-hhmmss

	return string(timebuffer);
}

// https://stackoverflow.com/questions/116038/how-do-i-read-an-entire-file-into-a-stdstring-in-c/116220#116220
string Util::SlurpFileIntoString(ifstream& is)
{
	return static_cast<stringstream const&>(stringstream() << is.rdbuf()).str();
}
