#pragma once
#include <string>
#include <vector>

// *******************************************************************
// * This file contains utility functions and data structures used in
// * the shell program. 
// *******************************************************************

std::vector <std::string> commands = {"cd", "ls", "pwd", "echo", "type", "exit"}; 

std::string PATH = getenv("PATH") ? getenv("PATH") : ".";
std::string HOME = getenv("HOME") ? getenv("HOME") : ".";

enum RedirectCode {
	STDOUT = 1,
	STDERR = 2,
};

struct BashData {
	std::string originalInput;
	std::string command;
	std::string args;
	std::string outputFile;
    std::string message;
	RedirectCode redirectCode;
	bool appendToFile;
    bool commandExecuted;
};

// Function to split a string by a delimiter
// This function takes a string and a delimiter character as input and returns a vector of strings
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start, end));
    return tokens;
}