#pragma once
struct Config
{
	unsigned int min_repeat_count, min_repeat_length;
	unsigned int max_gap_length, min_tandem_component_length;

	void Parse();
};

