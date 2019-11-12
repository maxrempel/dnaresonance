#pragma once

#include <string>
#include <iostream>

class Error
{
	std::string Message = "";

public:
	Error(std::string message = "") {
		Message = message;
	}

	void Fatal(std::string message = "") {
		Message += message;
		std::cerr << "Fatal ERROR: " << Message << std::endl;
		throw new std::exception((const char* const)Message.c_str());
	}

	void Warn(std::string message = "") {
		Message += message;
		std::cerr << "Continuing with a WARNING: " << Message << std::endl;
	}
};

