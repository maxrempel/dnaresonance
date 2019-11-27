#pragma once
#include <string>
using namespace std;

enum class FilterStatus: char
{
	Enforce = 'E', Ignore = 'I', Exclude = 'X',
};

