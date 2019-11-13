#include <iostream>
#include <filesystem>
#include <regex>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <cstring>
#include <sstream>
#include <map>

#include "Error.h"
#include "Config.h"

using namespace std;
namespace fs = filesystem;

/*
* Auxiliary.
*/
string summary_file_name(Config config) {
	string output_summary_filename = "summary.tab"; // single file for the whole folder
	output_summary_filename = (fs::path(config.output_folder_name) /= output_summary_filename)
		.string();
	return output_summary_filename;
}

// True if and only if seq is an exact palindrome,
// with the constraints from config.
bool is_palindrome(string seq, Config config) {
	auto stalk_max_length = config.palindrome_center;
	auto arm_min_length = config.palindrome_arm;
	auto seq_length = seq.length();

	if (stalk_max_length < 0) {
		string msg = "Config: palindrome stalk length is " + to_string(stalk_max_length) + ", understood as ZERO (no stalk allowed).";
		Error().Warn(msg);
		stalk_max_length = 0;
	}
	if (arm_min_length <= 0) {
		string msg = "Config: palindrome minimum arm length is " + to_string(arm_min_length)
			+ ", interpreted as zero and is therefore ignored. Effectively, the palindrome constraint is NOT APPLIED.";
		Error().Warn(msg);
		return true;
	}
	if (seq_length <= 0) {
		string msg = "An empty sequence is a trivial palindrome.";
		Error().Warn(msg);
		return true;
	}

	bool is_arm_long_enough = false;
	bool is_stalk_short_enough = false;

	size_t arm_length = 0;
	for (size_t i = 0; i < seq_length / 2; ++i) {
		auto j = seq_length - 1 - i;
		arm_length = i;
		if (seq[i] != seq[j]) {
			break;
		}
	}
	if (arm_length < arm_min_length) {
		return false;
	}

	size_t stalk_length = seq_length - 2 * arm_length;
	if (stalk_length > stalk_max_length) {
		return false;
	}

	return true;
}

// True if and only if seq is an exact tandem,
// with the constraints from config.
//bool is_tandem(string seq, Config config) {
//	int tandem_min_unit = config.tandem_min_unit;
//	int tandem_unit_copies = config.tandem_unit_copies;
//
//	if (tandem_min_unit < 1) {
//		return false;
//	}
//	if (tandem_unit_copies < 1) {
//		return false;
//	}
//	if (tandem_unit_copies == 1) {
//		return true;
//	}
//
//	char cExactTandem[100];
//	snprintf(cExactTandem, 100, "^(\\w{%d,})\\1{%d,}$", tandem_min_unit, tandem_unit_copies - 1);
//	static regex reExactTandem(cExactTandem, regex::icase);
//	// TODO: optimization: compile it and place in config
//
//	return regex_match(seq, reExactTandem);
//}

bool is_tandem(string seq, regex reExactTandem) {
	return regex_match(seq, reExactTandem);
}

/*
* Process type is either fpt (footprint) or crd (coordinates).
* Each version uses different configuration values that serve as parameters 
*	for essentially the same algorithms.
* Currently, these parameters are: 
*	min_repeat_length, copy_number.
* Also, different output files are generated.
* As part of the footprint processing, generate:
*	- The footpring file, one per input file, which is a csv file with islands counted and island end points marked.
*	- Summary file sum_{filename}.tab, one per the whole folder, with footprint densities.
* As part of the coordinates processing, generate:
*	- The coordinates file crd_{filename}.gb
*	- The copy number output file cop_{filename}.mfa
*/
void process_file(string filename, Config config, Process_Type pt)
{
	ifstream is(filename);
	if (!is) {
		Error().Fatal("Cannot open for reading file: " + filename);
	}

	std::cout << endl << "-- -- -- -- -- --\nInput file " << filename << endl;

	streamsize config_min_repeat_length;
	unsigned int config_copy_number;

	std::cout << "Process type " << (int)pt << endl;
	switch (pt)
	{
	case Process_Type::fpt:
		config_min_repeat_length = config.fpt_min_repeat_length;
		config_copy_number = config.fpt_copy_number;
		break;
	case Process_Type::crd:
		config_min_repeat_length = config.crd_min_repeat_length;
		config_copy_number = config.crd_copy_number;
		break;
	default:
		string msg = "***Unknown process type " + (int)pt;
		Error().Fatal(msg);
	}

	size_t buffer_capacity = 8192;
	if (buffer_capacity <= config_min_repeat_length) {
		// Unlikely, but who knows.
		buffer_capacity = (size_t)(2 * config_min_repeat_length);
	}
	char* buffer = new char[buffer_capacity];
	size_t buffer_actual_length = buffer_capacity; // how much of the capacity is used

	// Skip the header, if any.
	string line;
	const regex origin_shift(".*\\brange=\\w+:(\\d+)-.*", regex::icase);
	smatch matching_pieces;
	while (is.peek() == '>') {
		getline(is, line);
		// Extract values from the header.
		if (!regex_match(line, matching_pieces, origin_shift)) {
			continue;
		}
		auto piece = matching_pieces[1].str();
		config.absolute_origin = stoll(piece);
	}

	// Remember where the data starts.
	auto stream_start_data = is.tellg();

	// Finish the initial data load in the buffer.
	auto offset = buffer_capacity - config_min_repeat_length;
	is.read(buffer + offset, config_min_repeat_length);

	// Full buffer will contain all the data from the file.
	size_t fullbuffer_capacity = (size_t)(fs::file_size(filename) - stream_start_data);
	auto fullbuffer_size = fullbuffer_capacity; // will be less the number of control characters
	auto fullbuffer = new char[fullbuffer_capacity]; // only fullbuffer_size will be used
	// Initially, fill it in from the initial buffer.
	streamsize fullbuffer_position = config_min_repeat_length;
	memcpy(fullbuffer, buffer + offset, fullbuffer_position);

	long count_control_chars = 0;
	size_t count_meaningful_letters = 0;

	// Build a map sequence -> count,
	// and see how many different sequences of length config_min_repeat_length
	// we encounter as a function of the number of input size.			

	unordered_map<string, list<streamsize>> seq2positions;
	auto seq = new char[(size_t)config_min_repeat_length + 1];
	seq[(size_t)config_min_repeat_length] = '\0';

	auto stopwatch_start = chrono::high_resolution_clock::now();

	// Initialize the abcN (meaningful letters) machinery.
	streamsize abcN_last_N_position = -1; // negative means no position
	bool abcN_initial = true; // true means we are in the initial phase of filling the buffer

	streamsize position = 0;
	streamsize position_max = -1; // ignore
	for (; !is.eof(); ) {
		// Fill the buffer.
		memmove(buffer, buffer + offset, (size_t)config_min_repeat_length);
		auto read_this_many = buffer_capacity - config_min_repeat_length;
		is.read(buffer + config_min_repeat_length, read_this_many);
		if (is.eof()) {
			read_this_many = is.gcount();
			buffer_actual_length = (size_t)(config_min_repeat_length + read_this_many);
		}

		// abcN check the initial buffer for N letters.
		if (abcN_initial) {
			for (auto i = config_min_repeat_length - 1; i >= 0; --i) {
				if (config.is_N_letter(buffer[i])) {
					abcN_last_N_position = i;
					break;
				}
			}

			abcN_initial = false;
		}

		// Calculate the number of meaningful letters in the initial buffer.
		if (abcN_initial) {
			for (size_t i = 0; i < config_min_repeat_length; i++)
			{
				if (config.is_N_letter(buffer[i])) {
					continue;
				}
				if (iscntrl(buffer[i])) {
					continue;
				}
				++count_meaningful_letters;
			}
		}

		// Can process data in the buffer 
		// starting from byte 0 (inclusive) and up to byte startx_last (exclusive).
		int startx_last = (int)(buffer_actual_length - config_min_repeat_length);
		for (int startx = 0; startx < startx_last; ++startx, ++position) {

			auto index_check_letter = startx + config_min_repeat_length;
			if (index_check_letter > buffer_capacity) {
				string msg = "***Internal error: index_check_letter=" + (long)index_check_letter;
				msg += " should be less than buffer_capacity=" + buffer_capacity;
				Error().Fatal(msg);
			}
			auto check_letter = buffer[index_check_letter];

			if (iscntrl(check_letter)) {
				// Skip a nonprintable character.
				--position;
				++count_control_chars;
				continue;
			}

			// Append non-control characters to the full buffer.
			fullbuffer[fullbuffer_position++] = check_letter;

			// abcN: any meaningless letters? Then skip this sequence.
			auto abcN_position_check_letter = position + config_min_repeat_length;
			if (config.is_N_letter(check_letter)) {
				abcN_last_N_position = abcN_position_check_letter;
				continue;
			}
			++count_meaningful_letters;
			if (abcN_position_check_letter - abcN_last_N_position < config_min_repeat_length) {
				continue;
			}

			//memcpy(seq, buffer + startx, (size_t)config_min_repeat_length); // xSH TODO corrected?
			memcpy(seq, fullbuffer + fullbuffer_position - config_min_repeat_length, (size_t)config_min_repeat_length);

			try
			{
				if (seq2positions.count(seq) < 1) {
					list<streamsize> ll;
					seq2positions[seq] = ll;
				}
				seq2positions[seq].push_back(position);
			}
			catch (const std::exception& ex)
			{
				cerr << "***seq2positions size=" << seq2positions.size() << " seq=" << seq << " ex=" << ex.what();
				char ignore;
				cin >> ignore;
				exit(0);
			}
		}

		if (is.eof()) {
			break;
		}
	}

	delete[] seq;
	delete[] buffer;

	fullbuffer_size -= count_control_chars;

	// Remove the sequences which appear less than requested.
	auto seq_iter = seq2positions.begin();
	while (seq_iter != seq2positions.end())
	{
		if (seq_iter->second.size() < config_copy_number) {
			seq_iter = seq2positions.erase(seq_iter);
		}
		else {
			++seq_iter;
		}
	}

	auto stopwatch_finish = chrono::high_resolution_clock::now();
	auto stopwatch_elapsed = stopwatch_finish - stopwatch_start;

	// Statistics: number of duplicates.
	long how_many_duplicates = seq2positions.size();
	std::cout << how_many_duplicates << " distinct sequences (" << (100.0 * how_many_duplicates / position)
		<< "%) of length " << config_min_repeat_length << " have at least "
		<< config_copy_number << " copies." << endl;

	// Execution time.
	long milliseconds = (long)(stopwatch_elapsed.count() / 1000000);
	auto seconds = (int)round(milliseconds / 1000.0);
	std::cout << endl
		<< "First phase took " << seconds << " seconds for "
		<< position << " sequences." << endl;

	// PREPROCESSING FOR BUILDING LONGER SEQUENCES
	// Map starting position -> sequence
	unordered_map<streamsize, string> start2seq;
	for (auto const& [s, pp] : seq2positions) {
		for (auto const& p : pp) {
			start2seq[p] = s;
		}
	}

	std::cout << "The total number of such sequences, including duplicates, is "
		<< start2seq.size() << " (so, "
		<< start2seq.size() / (double)how_many_duplicates << " copies on average)." << endl;

	// Map: sequence length -> map position2seq.
	unordered_map<streamsize, unordered_map<streamsize, string>> length2map;
	unordered_map<streamsize, string> mss(start2seq);
	length2map[config_min_repeat_length] = mss;

	// NEXT PHASE
	// Extend the sequences, as far as possible.

	auto seq_length_max = config_min_repeat_length; // max observed
	for (auto seq_length = config_min_repeat_length + 1; ; ++seq_length) {

		// Try to extend each sequence by appending the next character
		// to each of the sequences from the previous step. 

		auto prefix_start2seq = length2map[seq_length - 1]; // where prefix sequences start

		seq2positions.clear();
		seq = new char[(size_t)seq_length + 1];
		seq[seq_length] = '\0';

		// Calculate seq2positions for all sequences of length seq_length.
		for (auto const& [start, prefix] : start2seq) {
			if (start + seq_length >= fullbuffer_size) {
				// prefix too close to the end
				continue;
			}

			memcpy(seq, fullbuffer + start, seq_length);

			if (seq2positions.count(seq) < 1) {
				list<streamsize> ll;
				seq2positions[seq] = ll;
			}
			seq2positions[seq].push_back(start);
		}

		std::cout << endl << "Sequence length " << seq_length
			<< ". Total " << seq2positions.size() << " distinct sequences." << endl;

		// Only leave seq2positions where seq appears enough.
		auto seq_iter = seq2positions.begin();
		while (seq_iter != seq2positions.end())
		{
			if (seq_iter->second.size() < config_copy_number) {
				seq_iter = seq2positions.erase(seq_iter);
			}
			else {
				++seq_iter;
			}
		}

		std::cout << seq2positions.size() << " of them appear enough";

		if (seq2positions.empty()) {
			std::cout << "." << endl << endl;
			break; // leave the loop
		}

		// Calculate map: start -> sequence for the current seq_length
		start2seq.clear();
		for (auto const& [s, plist] : seq2positions) {
			for (auto const& p : plist) {
				start2seq[p] = s;
			}
		}

		std::cout << ", for a total of " << start2seq.size() << " sequences (counting all copies)." << endl;

		// Update length2map.
		unordered_map<streamsize, string> mss(start2seq);
		length2map[seq_length] = mss;

		seq_length_max = seq_length;
		delete[] seq;
	} // for seq_length

	// Only look at palindromes, if so requested.
	// Leave only exact palindromes, as defined by is_palindrome.
	if (config.please_only_palindromes) {
		for (auto seq_length = seq_length_max; seq_length >= config_min_repeat_length; --seq_length) {
			auto current_level = &length2map[seq_length];

			auto level_iter = current_level->begin();
			while (level_iter != current_level->end()) {
				auto seq = level_iter->second;

				if (!is_palindrome(seq, config)) {
					level_iter = current_level->erase(level_iter);
				}
				else {
					++level_iter;
				}
			}
		}
	}

	// Consider palindromes with flanks.
	// TODO

	// Only look at tandems, if so requested.
	// Leave only exact tandems, as defined by is_tandem.
	int tandem_min_unit = config.tandem_min_unit;
	int tandem_unit_copies = config.tandem_unit_copies;

	if (config.please_only_tandems) {
		if (tandem_min_unit < 1) {
			Error().Fatal("Tandem min unit should be >1 but is " + tandem_min_unit);
		}
		if (tandem_unit_copies < 1) {
			Error().Fatal("Tandem unit copies should be positive but is " + tandem_unit_copies);
		}
		if (tandem_unit_copies == 1) {
			Error().Warn("Tandem that requires just 1 copy imposes no additional constraints");
			config.please_only_tandems = false;
		}

		char cExactTandem[100];
		snprintf(cExactTandem, 100, "^(\\w{%d,})\\1{%d,}$", tandem_min_unit, tandem_unit_copies - 1);
		static regex reExactTandem(cExactTandem, regex::icase);

		for (auto seq_length = seq_length_max; seq_length >= config_min_repeat_length; --seq_length) {
			auto current_level = &length2map[seq_length];

			auto level_iter = current_level->begin();
			while (level_iter != current_level->end()) {
				auto seq = level_iter->second;

				if (!is_tandem(seq, reExactTandem)) {
					level_iter = current_level->erase(level_iter);
				}
				else {
					++level_iter;
				}
			}
		}
	}

	// Consider tandems with flanks.
	// TODO

	// CULLING attempt. 
	// Logic:
	// 	- Start with the longest sequences. 
	//	- Walk positions 0 to (seq_length - 17 - 1) 
	//		and remove any shorter sequences that may start at those positions and fit inside.
	//	- When done, move on the the next longest sequences.
	//	- Stop when have reached the shortest sequences.
	//	- Note: at the end of this process, some of the remaining sequences may appear less than 3 times. 
	//		Just leave them in for now.

	std::cout << "Culling... ";

	// Working on length2map, generating length2map_culled.
	unordered_map<streamsize, unordered_map<streamsize, string>> length2map_culled(length2map);

	for (auto seq_length = seq_length_max; seq_length > config_min_repeat_length; --seq_length) {
		auto current_level = length2map_culled[seq_length];
		for (auto const& [seq_start, seq] : current_level) {
			// Any shorter sequences nested in seq?
			for (auto cull_length = seq_length - 1; cull_length >= config_min_repeat_length; --cull_length) {
				// Any sequences of cull_length nested in seq?
				auto cull_candidates = &length2map_culled[cull_length];
				for (auto cull_start = seq_start; cull_start + cull_length <= seq_start + seq_length; ++cull_start) {
					if (cull_candidates->count(cull_start) > 0) {
						cull_candidates->erase(cull_start);
					}
				}
			}
		}
	}

	// Consider all sequences long enough that also appear enough times.
	// Calculate the footprints and density.
	auto footprints_capacity = fullbuffer_capacity;
	auto footprints_size = fullbuffer_size;
	vector<bool> footprints(footprints_capacity, false);

	std::cout << endl << "Using the culled data, calculating the footprints... ";

	for (auto const& [seq_length, map] : length2map_culled) {
		for (auto const& [pos, seq] : map) {
			for (auto i = 0; i < seq_length; ++i) {
				footprints[pos + i] = true;
			}
		}
	}

	std::cout << "Calculating the density... ";

	size_t footprints_count = 0;
	for (auto f : footprints) {
		if (f) {
			++footprints_count;
		}
	}

	std::cout << endl << footprints_count << " combined footprints count; that is, "
		<< 100.0 * footprints_count / footprints_size << "% density." 
		<< " (" << 100.0 * footprints_count / count_meaningful_letters << "% subdensity.)"
		<< endl;

	// How many sequences are left after culling?
	long seq_total_count = 0;
	std::cout << endl << "Sequences left after culling, per sequence length:" << endl;
	for (auto const& [seq_length, level] : length2map_culled) {
		if (level.size() < 1) {
			continue;
		}
		std::cout << seq_length << ":\t" << level.size() << endl;
		seq_total_count += level.size();
	}
	std::cout << "Total:\t" << seq_total_count << endl;

	// Execution time.
	stopwatch_finish = chrono::high_resolution_clock::now();
	stopwatch_elapsed = stopwatch_finish - stopwatch_start;
	milliseconds = (long)(stopwatch_elapsed.count() / 1000000);
	seconds = (int)round(milliseconds / 1000.0);
	std::cout << endl
		<< "Calculations took " << seconds << " seconds on file " << filename << endl;

	// OUTPUT FILES
	std::cout << endl << "Now generating output files." << endl;
	
	// Make sure the output folder exists.
	auto output_folder_path = fs::path(config.output_folder_name);
	if (fs::exists(output_folder_path)) {
		if (!fs::is_directory(output_folder_path)) {
			string msg = "***Output folder name clashes with an existing plain file name.";
			Error().Fatal(msg);
		}
	}
	else {
		fs::create_directory(output_folder_path);
	}

	// Output flename: basename.
	auto basename = filename;
	auto input_prefix = config.config_map[config.label_input_filename_prefix];
	if (filename.find_first_of(input_prefix) == 0) {
		basename = basename.substr(input_prefix.length());
	}
	auto extension = fs::path(filename).extension().string();
	if (extension.length() > 0) {
		basename = basename.substr(0, basename.length() - extension.length());
	}
	// Output filenames
	string output_footprint_filename = "fpt_" + basename + ".csv";
	string output_elements_filename = "e_" + basename + ".tab";
	string output_coordinates_filename = "crd_" + basename + ".gb";
	string output_copynumber_filename = "cop_" + basename + ".mfa";
	string output_masked_filename = basename + "_srm.fa";

	output_footprint_filename = (fs::path(config.output_folder_name) /= output_footprint_filename)
		.string();
	output_elements_filename = (fs::path(config.output_folder_name) /= output_elements_filename)
		.string();
	output_coordinates_filename = (fs::path(config.output_folder_name) /= output_coordinates_filename)
		.string();
	output_copynumber_filename = (fs::path(config.output_folder_name) /= output_copynumber_filename)
		.string();
	output_masked_filename = (fs::path(config.output_folder_name) /= output_masked_filename)
		.string();

	ofstream of_fpt;
	ofstream of_elements;
	ofstream of_sum;
	ofstream of_crd;
	ofstream of_cop;
	ofstream of_srm;

	if (pt == Process_Type::fpt) {

		// 1. Footprint output file and elements output file.
		std::cout << "Generating " << output_footprint_filename << " and " << output_elements_filename << "...";

		of_fpt.open(output_footprint_filename);
		if (!of_fpt) {
			Error().Fatal("Cannot open for writing file: " + output_footprint_filename);
		}

		of_elements.open(output_elements_filename);
		if (!of_elements) {
			Error().Fatal("Cannot open for writing file: " + output_footprint_filename);
		}

		// Headers.
		of_elements << "track type=wiggle_0 name=Ch0 Custom UM 00 U" << endl;
		of_elements << "variablestep chrom=chr0" << endl;

		vector<streamsize> island_endpoints; // start, then end, of each island, in order		
		streamsize latest_start = 0;

		int island_count = 0;
		bool in_island = false;
		size_t value;
		for (auto fx = 0; fx < footprints_size; fx++)
		{
			auto fp = footprints[fx];
			if (fp) {
				if (in_island) continue;
				in_island = true;
				island_count++;
				value = fx + config.shift_coordinates;
				of_fpt << "island" << island_count << "," << value;
				// masked
				if (config.please_create_masked_file) {
					island_endpoints.push_back(value);
				}
				// elements
				latest_start = value;
				continue;
			}
			// !fp
			if (in_island) {
				in_island = false;
				value = fx - 1 + config.shift_coordinates;
				of_fpt << "," << value << endl;
				// masked
				if (config.please_create_masked_file) {
					island_endpoints.push_back(value);
				}
				// elements
				auto e = config.absolute_origin + (value + latest_start) / 2;
				of_elements << e << endl;
			}
		}
		of_elements.close();
		of_fpt.close();

		std::cout << endl;

		// 2. Summary file.
		auto output_summary_filename = summary_file_name(config);
		of_sum.open(output_summary_filename, ios::app);
		if (!of_sum) {
			Error().Fatal("Cannot open for writing file: " + output_summary_filename);
		}
		
		double density_percent = 100.0 * footprints_count / footprints_size;		
		double subdensity_percent = 100.0 * footprints_count / count_meaningful_letters;
		of_sum.setf(ios::fixed, ios::floatfield);
		of_sum << filename << "\t" << setprecision(4) << density_percent << "%" 
			<< "\t" << subdensity_percent << "%"
			<< endl;
		of_sum.close();

		std::cout << endl;

		// 3. Masked (srm) file.
		if (config.please_create_masked_file) {
			std::cout << "Generating " << output_masked_filename << "...";

			of_srm.open(output_masked_filename);
			if (!of_fpt) {
				Error().Fatal("Cannot open for writing file: " + output_masked_filename);
			}

			// Note: each island is inclusive on left, exclusive on right side.
			bool in_island = false;
			size_t island_x = 0; // index of the next island endpoint, if any (negative if none)
			for (streamsize pos = 0; pos < fullbuffer_size; pos++)
			{
				if (island_x < island_endpoints.size()) {
					if (island_endpoints[island_x] == pos) {
						if (in_island) {
							// finish the island
							in_island = false;
						}
						else {
							// start the island
							in_island = true;
						}
						// next endpoint
						++island_x;
					}
				}

				if (in_island) {
					of_srm << config.masking_character;
				}
				else {
					of_srm << fullbuffer[pos];
				}				
			} // for pos
			of_srm.close();

			std::cout << endl;
		} // if create masked file

	} // if footprint

	if (pt == Process_Type::crd) {

		// 1. Coordinates file.
		std::cout << "Generating " << output_coordinates_filename << "...";

		of_crd.open(output_coordinates_filename);
		if (!of_crd) {
			Error().Fatal("Cannot open for writing file: " + output_coordinates_filename);
		}

		// Building a flat map of seq => positions.
		unordered_map<string, vector<streamsize>> seq2pos;
		for (auto const& [len, m] : (config.please_cull_crd ? length2map_culled : length2map)) {
			for (auto const& [startx, seq] : m) {
				if (seq2pos.count(seq) < 1) {
					seq2pos[seq] = vector<streamsize>();					
				}
				seq2pos[seq].push_back(startx);
			}
		}
		// Print the map in GB format.
		of_crd << "LOCUS	Annotations" << endl;
		of_crd << "UNIMARK	Annotations" << endl;
		of_crd << "FEATURES	Location/Qualifiers" << endl;
		for (auto const& [seq, li] : seq2pos) {
			if (li.empty())continue;

			of_crd << "repeat_region	join(";
			of_crd << li[0] << ".." << (li[0] + seq.length());
			for (auto i = 1; i < li.size(); ++i) {
				of_crd << "," << li[i] << ".." << (li[i] + seq.length());
			}
			of_crd << ")" << endl;
			
			of_crd << "/repeat sequence:	" << seq << endl;
		}
		of_crd << "//" << endl;
		of_crd.close();

		std::cout << endl;

		// 2. Copy number file.
		std::cout << "Generating " << output_copynumber_filename << "...";
		
		of_cop.open(output_copynumber_filename);
		if (!of_cop) {
			Error().Fatal("Cannot open for writing file: " + output_copynumber_filename);
		}

		// Goal: sort by seq count then by length.
		// So, build a flat structure.

		// Use the original (not culled) data.

		map<int, vector<string>> count2seqs; // count => seqs, ordered by count

		for (auto const& [len, start2seq] : length2map) {
			unordered_map<string, int> seq2count;
			for (auto const& [start, seq] : start2seq) {
				if (seq2count.count(seq) < 1) {
					seq2count[seq] = 1;
				}
				else {
					seq2count[seq] = seq2count[seq] + 1;
				}
			}
			for (auto const& [seq, count] : seq2count) {
				if (count2seqs.count(count) < 1) {
					count2seqs[count] = vector<string>();
				}
				count2seqs[count].push_back(seq);
			}
		}

		// Now count2seqs has, for each count, sequences that appear that number of times, 
		// in a non-decreasing order of their lengths.
		
		for (auto itr = count2seqs.crbegin(); itr != count2seqs.crend(); ++itr) {
			auto count = itr->first;
			auto seqs = itr->second;
			for (auto itrseq = seqs.crbegin(); itrseq != seqs.crend(); ++itrseq) {
				auto seq = *itrseq;

				of_cop << ">" << count << endl << seq << endl;
			}
		}
		of_cop.close();

		std::cout << endl;

	} // if coordinates

	// Execution time.
	stopwatch_finish = chrono::high_resolution_clock::now();
	stopwatch_elapsed = stopwatch_finish - stopwatch_start;
	milliseconds = (long)(stopwatch_elapsed.count() / 1000000);
	seconds = (int)round(milliseconds / 1000.0);
	std::cout << endl
		<< "Task took " << seconds << " seconds on file " << filename << endl;

} // process_file

int main()
{
	auto folder_current_path = fs::current_path();
	Config config;
	config.Parse();

	try {
		// Make sure the output folder exists.
		auto output_folder_path = fs::path(config.output_folder_name);
		if (fs::exists(output_folder_path)) {
			if (!fs::is_directory(output_folder_path)) {
				string msg = "***Output folder name clashes with an existing plain file name.";
				Error().Fatal(msg);
			}
		}
		else {
			fs::create_directory(output_folder_path);
		}

		// Pre-create the summary file, one for the whole folder.
		auto output_summary_filename = summary_file_name(config);
		ofstream of_sum(output_summary_filename, ios::trunc);
		if (!of_sum) {
			Error().Fatal("Cannot open for writing file: " + output_summary_filename);
		}

		regex regex_fa(".+\\.(fa|fasta)", regex::icase);
		// All files matching regex_fa
		for (const auto& entry : fs::directory_iterator(folder_current_path)) {
			auto filename = entry.path().filename().string();
			if (!regex_match(filename, regex_fa)) {
				continue;
			}
			
			if (config.please_create_fpt_file) {
				process_file(filename, config, Process_Type::fpt);
			}
			if (config.please_create_crd_file) {
				process_file(filename, config, Process_Type::crd);
			}
		} // for each input data file
	}
	catch (exception ex) {
		std::cerr << ex.what();
	}
	
}
