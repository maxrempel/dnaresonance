#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>

#include "Util.h"
#include "Error.h"

using namespace std;
namespace fs = filesystem;

void process_file(string filename) 
{
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
	}

	// Read in the rest of the file buffer by buffer
	// while removing illegal letters/chars.



	// TODO



	// Execution time.
	auto stopwatch_finish = chrono::high_resolution_clock::now();
	auto stopwatch_elapsed = stopwatch_finish - stopwatch_start;

	long milliseconds = (long)(stopwatch_elapsed.count() / 1000000);
	auto seconds = (int)round(milliseconds / 1000.0);
	std::cout << endl
		<< "File " << filename << " took " << seconds << " seconds." << endl;
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
