#include "Config.h"

void Config::Parse()
{
	// Intentionally left blank.
	// Initially use for development,
	// later read an actual config file.
	this->max_gap_length = 2;
	this->min_tandem_component_length = 3;
	this->min_repeat_count = 2;
	this->min_repeat_length = 7;
}