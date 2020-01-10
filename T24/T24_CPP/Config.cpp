#include "Config.h"
#include "FilterStatus.h"
#include "Error.h"

namespace fs = filesystem;

void Config::Parse(fs::path path)
{
	folder_current_path = path.string();

	// First file matching regex_cfg
	for (const auto& entry : fs::directory_iterator(folder_current_path)) {
		auto filename = entry.path().filename();
		if (regex_match(filename.string(), regex_cfg)) {
			file_config_name = filename.string();
			break;
		}
	}

	if (file_config_name == "") {
		string msg = "No configuration file foudn in folder " + folder_current_path;
		Error().Fatal(msg);
	}

	string config_match[] = {
		label_input_filename_prefix,
		label_meaningful_letters,
		label_fpt_min_repeat_length,
		label_crd_min_repeat_length,
		label_shift_coordinates,
		label_mask_repeats,
		label_masking_character,
		label_fpt_copy_number,
		label_crd_copy_number,
		label_cull_crd,
		label_create_fpt_file,
		label_create_crd_file,
		label_only_palindromes,
		label_palindrome_max_nonpalindromic_center,
		label_palindrome_min_length,
		label_only_tandems,
		label_tandems_min_unit,
		label_tandems_unit_copies,
		label_palindrome_status,
		label_tandem_status,
		label_palindrome_arm_tandem_status,
		label_palindrome_arm_tandem_min_unit,
		label_palindrome_arm_tandem_unit_copies,
		label_split_bunch_maxsize,
		label_palindrome_variable_centers
	};
	auto config_match_total = sizeof(config_match) / sizeof(config_match[0]);
	int config_match_count = 0;

	// Set up config.
	ifstream config_file;
	config_file.open(file_config_name);
	string line;
	while (std::getline(config_file, line)) {
		cout << "[got] " << line << endl;
		if (line.substr(0, 1) == ">") {
			continue; // skip a comment line
		}
		if (is_whitespace(line)) {
			continue;
		}
		auto pos = line.find('=');
		if (pos == string::npos) {
			throw new exception((const char* const)("Cannot parse config file: " + line).c_str());
		}
		auto first = line.substr(0, pos);
		auto second = line.substr(pos + 1, line.length() - pos);

		auto last = config_match + config_match_total;
		auto f = find(config_match, last, first);
		if (f == last) {
			continue; // first not defined as a config key
		}
		// process first as a config key
		if (config_map.count(first) > 0) {
			continue; // already set
		}
		config_map[first] = second;
		regex yes("\\bYES\\b", regex::icase);
		// Process numerics.
		if (first == label_fpt_min_repeat_length) {
			fpt_min_repeat_length = stoi(second); //  can throw
		}
		else if (first == label_crd_min_repeat_length) {
			crd_min_repeat_length = stoi(second); //  can throw
		}
		else if (first == label_fpt_copy_number) {
			fpt_copy_number = stoi(second); // can throw
		}
		else if (first == label_crd_copy_number) {
			crd_copy_number = stoi(second); // can throw
		}
		else if (first == label_shift_coordinates) {
			shift_coordinates = stoi(second); // can throw
		}
		else if (first == label_palindrome_max_nonpalindromic_center) {
			palindrome_center = stoi(second); // can throw
		}
		else if (first == label_palindrome_min_length) {
			palindrome_arm = stoi(second); // can throw
		}
		else if (first == label_tandems_min_unit) {
			tandem_min_unit = stoi(second); // can throw
		}
		else if (first == label_tandems_unit_copies) {
			tandem_unit_copies = stoi(second); // can throw
		}
		else if (first == label_palindrome_arm_tandem_min_unit) {
			palindrome_arm_tandem_min_unit = stoi(second); // can throw
		}
		else if (first == label_palindrome_arm_tandem_unit_copies) {
			palindrome_arm_tandem_unit_copies = stoi(second); // can throw
		}
		else if (first == label_split_bunch_maxsize) {
			split_bunch_maxsize = stoi(second); // can throw
		}
		// Process bools.
		else if (first == label_cull_crd) {
			please_cull_crd = regex_match(second, yes);
		}
		else if (first == label_create_fpt_file) {
			please_create_fpt_file = regex_match(second, yes);
		}
		else if (first == label_create_crd_file) {
			please_create_crd_file = regex_match(second, yes);
		}
		else if (first == label_mask_repeats) {
			please_create_masked_file = regex_match(second, yes);
		}
		else if (first == label_only_palindromes) {
			please_only_palindromes = regex_match(second, yes);
		}
		else if (first == label_only_tandems) {
			please_only_tandems = regex_match(second, yes);
		}
		else if (first==label_palindrome_variable_centers) {
			please_only_variable_centers = regex_match(second, yes);
		}
		// Process filters.
		else if (first == label_palindrome_status) {
			palindrome_status = ReadFilterStatus(second);
		}
		else if (first == label_tandem_status) {
			tandem_status = ReadFilterStatus(second);
		}
		else if (first == label_palindrome_arm_tandem_status) {
			palindrome_arm_tandem_status = ReadFilterStatus(second);
		}
		// Other special cases.
		else if (first == label_masking_character) {
			masking_character = second.at(0);
		}
		// Iterate.
		++config_match_count;
	}

	for (auto const& c : config_map[label_meaningful_letters]) {
		letters.insert(toupper(c));
	}

} // Parse()

FilterStatus Config::ReadFilterStatus(string s)
{
	// Enforce
	regex enforce("^ENFORCE", regex::icase);
	regex include("^INCLUDE", regex::icase);
	regex yes("^YES", regex::icase);
	// Ignore
	regex ignore("^IGNORE", regex::icase);
	// Exclude
	regex exclude("^EXCLUDE", regex::icase);
	regex no("^NO", regex::icase);

	if (regex_match(s, ignore)) {
		return FilterStatus::Ignore;
	}
	if (regex_match(s, exclude) || regex_match(s, no)) {
		return FilterStatus::Exclude;
	}
	if (regex_match(s, enforce) || regex_match(s, yes) || regex_match(s, include)) {
		return FilterStatus::Enforce;
	}

	return FilterStatus::Ignore;
} // ReadFilterStatus()