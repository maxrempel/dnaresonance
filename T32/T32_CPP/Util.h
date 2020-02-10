#pragma once
#include <string>

using namespace std;

struct Util
{
	static string TimestampCurrent();

	static string SlurpFileIntoString(ifstream&);
};

