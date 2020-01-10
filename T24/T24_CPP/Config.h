#pragma once
#include <string>
#include <regex>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <ios>

#include "FilterStatus.h"

using namespace std;
namespace fs = filesystem;

enum class Process_Type {
	fpt, crd
};

struct Config
{
	unordered_map<string, string> config_map; // Map of values
	streamsize fpt_min_repeat_length, crd_min_repeat_length;
	unsigned int fpt_copy_number, crd_copy_number;
	signed int shift_coordinates = 0;

	bool please_create_fpt_file = true; // Need to generate the footprint output file?
	bool please_create_crd_file = true; // Need to generate the coordinates output file?
	
	bool please_create_masked_file = false; // Need to create the _srm output file?

	bool please_cull_crd = true;

	bool please_only_palindromes = false;
	int palindrome_arm;
	int palindrome_center;
	bool please_only_variable_centers = false;

	bool please_only_tandems = false;
	int tandem_min_unit;
	int tandem_unit_copies;

	FilterStatus palindrome_status;
	FilterStatus tandem_status;
	FilterStatus palindrome_arm_tandem_status;

	int palindrome_arm_tandem_min_unit;
	int palindrome_arm_tandem_unit_copies;

	int split_bunch_maxsize = -1;

	unordered_set<char> letters; // legitimate letters to be analyzed; case insensitive
	char masking_character = 'N';

	string folder_current_path;
	string file_config_name = "";
	regex regex_cfg;

	// Labels
	string label_fpt_min_repeat_length = "fpt_min_repeat_length",
		label_crd_min_repeat_length = "crd_min_repeat_length";
	string label_fpt_copy_number = "fpt_copy_number",
		label_crd_copy_number = "crd_copy_number";
	string label_shift_coordinates = "shift";
	string label_mask_repeats = "mask";
	string label_masking_character = "masking_character";
	string label_meaningful_letters = "meaningful_letters";
	string label_cull_crd = "cull_crd";
	string label_create_fpt_file = "create_fpt_file",
		label_create_crd_file = "create_crd_file";
	string label_only_palindromes = "palindromes"; // deprecated
	string label_palindrome_max_nonpalindromic_center = "palindrome_center";
	string label_palindrome_min_length = "palindrome_arm";
	string label_only_tandems = "tandems"; // deprecated
	string label_tandems_min_unit = "min_unit";
	string label_tandems_unit_copies = "unit_copies";
	string label_palindrome_status = "palindrome_status";
	string label_tandem_status = "tandem_status";
	string label_palindrome_arm_tandem_status = "palindrome_arms_tandems_status";
	string label_palindrome_arm_tandem_min_unit = "palindrome_arm_tandem_min_unit";
	string label_palindrome_arm_tandem_unit_copies = "palindrome_arm_tandem_unit_copies";
	string label_input_filename_prefix = "filename_prefix_to_replace";
	string label_split_bunch_maxsize = "split_bunch_maxsize";
	string label_palindrome_variable_centers = "variable_centers";

	string output_folder_name = "Output";

	streamsize absolute_origin = 0; // from the beginning of the chromosome

	Config(string config_regex = "T24.*\\.cfg") {
		regex_cfg = regex(config_regex, regex::icase);
	}

	void Parse(fs::path = fs::current_path());

	FilterStatus ReadFilterStatus(string s);

	bool is_whitespace(string s)
	{
		return s.find_first_not_of(" \t") == string::npos;
	}

	bool split_requested()
	{
		return split_bunch_maxsize > 0;
	}

	// Case insensitive.
	// Not a valid letter?
	bool is_N_letter(char c) {
		return letters.count(toupper(c)) == 0;
	}
};

