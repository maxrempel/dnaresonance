#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>

#include "Util.h"
#include "Error.h"
#include "Config.h"

using namespace std;
namespace fs = filesystem;

regex create_regex_from_config(Config config)
{
	char gappedTandem[100];
	snprintf(gappedTandem, 100, "^(\\w{%d,}\\w{0,%d})\\1{%d,}$", 
		config.min_tandem_component_length, config.max_gap_length, config.min_repeat_count - 1);
	regex reGappedTandem(gappedTandem, regex::icase);
	return reGappedTandem;
}

char* create_from_legit_letters(ifstream& is, size_t filesize, size_t& actual_size)
{
	// Remember where the data starts.
	auto stream_start_data = is.tellg();

	// Full buffer will contain all the data from the file.
	size_t fullbuffer_capacity = (size_t)(filesize - stream_start_data);
	actual_size = 0; // fullbuffer_capacity minus the number of control characters
	auto fullbuffer = new char[fullbuffer_capacity]; // only fullbuffer_size will be used

	size_t buffer_capacity = 8; // 8192;
	char* buffer = new char[buffer_capacity];

	char* pch = fullbuffer;
	for (; !is.eof();) {
		auto read_this_many = buffer_capacity;
		is.read(buffer, read_this_many);
		if (is.eof()) {
			read_this_many = is.gcount();
		}

		actual_size += read_this_many;
		for (size_t i = 0; i < read_this_many; ++i) {
			auto ch = buffer[i];
			if (!isalpha(ch)) {
				--actual_size;
				continue;
			}
			*pch = ch;
			++pch;
		}
	}

	delete[] buffer;

	return fullbuffer;
}

void process_file(string filename)
{
	Config config;
	config.Parse();

	ifstream is(filename);
	if (!is) {
		Error().Fatal("Cannot open for reading file: " + filename);
	}

	cout << endl << "Input file: " << filename << endl;

	// Execution time.
	auto stopwatch_start = chrono::high_resolution_clock::now();

	// Skip/remember the header if any.
	string inputline, header;
	while (is.peek() == '>' && getline(is, inputline)) {
		header += inputline;
		// TODO: is origin_shift relevant?
	}

	// Read in the rest of the file buffer by buffer
	// while removing illegal letters/chars.
	size_t fullbuffer_size;
	auto fullbuffer = create_from_legit_letters(is, fs::file_size(filename), fullbuffer_size);

	regex re_tandem = create_regex_from_config(config);




	// TODO



	// Execution time.
	auto stopwatch_finish = chrono::high_resolution_clock::now();
	auto stopwatch_elapsed = stopwatch_finish - stopwatch_start;

	long milliseconds = (long)(stopwatch_elapsed.count() / 1000000);
	auto seconds = (int)round(milliseconds / 1000.0);
	std::cout << endl
		<< "File " << filename << " took " << seconds << " seconds." << endl;

	delete[] fullbuffer;
}

int main()
{
	cout << "GAPPED TANDEMS " << Util::TimestampCurrent() << endl;

	auto folder_input = fs::current_path();
	cout << endl << "Folder: " << folder_input << endl;

	// Execution time.
	auto stopwatch_start = chrono::high_resolution_clock::now();

	// Read in only certain files in the given folder.
	regex regex_fa(".+\\.(fa|fasta)", regex::icase); // describes input file names

	// All the matching files:
	for (const auto& entry : fs::directory_iterator(folder_input)) {
		auto filename = entry.path().filename().string();
		if (!regex_match(filename, regex_fa)) {
			continue;
		}

		// Work on a single file.
		process_file(filename);
	} // for each input data file

	// TODO

	// Execution time.
	auto stopwatch_finish = chrono::high_resolution_clock::now();
	auto stopwatch_elapsed = stopwatch_finish - stopwatch_start;

	long milliseconds = (long)(stopwatch_elapsed.count() / 1000000);
	auto seconds = (int)round(milliseconds / 1000.0);
	std::cout << endl
		<< "The folder took " << seconds << " seconds." << endl;
}
