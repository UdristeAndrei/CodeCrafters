#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <set>

std::array<std::string, 4> commands = {"echo", "exit", "type", "pwd"};
std::string PATH = getenv("PATH") ? getenv("PATH") : ".";
std::string HOME = getenv("HOME") ? getenv("HOME") : ".";

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

// Function to write a string to a file
void stdoutBash(const std::string& filename, const std::string& content, bool append = false) {
	// Check if the filename is empty, and if so, print to stdout
	if (filename.empty()) {
		std::cout << content << "\n";
		return;
	}
	
	std::ofstream file(filename, append ? std::ios::app : std::ios::out);
	if (!file) {
		std::cerr << "Error opening file: " << filename << std::endl;
		return;
	}
	file << content << "\n";
	file.close();
}

void separateCommand(const std::string& input, std::string& command, std::string& args, std::string& output_file, unsigned int& redirect_code, bool& append) {
	// Check if the output needs to be redirected
	std::string redirect_symbol;
	if (input.find("1>>") != std::string::npos) {
		redirect_symbol = "1>>";
		append = true;
	}else if (input.find("1>") != std::string::npos) {
		redirect_symbol = "1>";
		redirect_code = 1;
	}else if (input.find("2>") != std::string::npos) {
		redirect_symbol = "2>";
		redirect_code = 2;
	}else if (input.find("2>>") != std::string::npos) {
		redirect_symbol = "2>>";
		redirect_code = 2;
		append = true;
	}else if (input.find(">>") != std::string::npos) {
		redirect_symbol = ">>";
		append = true;
	}else if (input.find('>') != std::string::npos) {
		redirect_symbol = '>';
	}
	// If the output needs to be redirected, separate the command and the output file
	if (!redirect_symbol.empty()) {
		output_file = input.substr(input.find(redirect_symbol) + redirect_symbol.length());
		command = input.substr(0, input.find(redirect_symbol));
		// Remove leading whitespace from the output file name
		output_file.erase(output_file.begin(), std::find_if(output_file.begin(), output_file.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
	} else {
		output_file.clear();
		command = input;
	}
	// Check if the command is enclosed in quotes
	std::string delimiter;
	bool isQuoted = false;
	if (command.find('\'') == 0 ){
		delimiter = "'";
		isQuoted = true;
	} else if (command.find('\"') == 0) {
		delimiter = "\"";
		isQuoted = true;
	} else {
		delimiter = " ";
	}
	args = command.substr(command.find(delimiter, 1) + 1);
	command = command.substr(isQuoted, command.find(delimiter, 1) - isQuoted);
}

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    while (true){
		std::cout << "$ ";

		// Read a line of input from the user
		std::string input;
		std::getline(std::cin, input);
		// Check if the input is empty
		if (input.empty()) {
			continue;
		}

		std::cout << input << "\n";
		// Separate the command, arguments, and output file
		bool append = false;
		unsigned int redirect_code = 0;
		std::string command, args, output_file;
		separateCommand(input, command, args, output_file, redirect_code, append);
		// ---------------------------------------------------------
		// Navigation commands
		// ---------------------------------------------------------
		if (command == "pwd"){
			// Get the current working directory
			stdoutBash(output_file, std::filesystem::current_path().string());
			continue;
		}

		// if (command == "ls") {
		// 	std::string path = args.substr(3); //Need to fix for the more complex cases
		// 	path.pop_back();
		// 	// Check to see if you are trying to list the home directory
		// 	if (path == "~") {
		// 		path = HOME;
		// 	}
		// 	// Check if the path is valid
		// 	if (std::filesystem::exists(path)) {
		// 		std::string outputMessage;
		// 		std::set<std::string> files;
		// 		// Iterate through the directory and add the files to the set to order them alphabetically
		// 		for (const auto& entry : std::filesystem::directory_iterator(path)) {
		// 			files.insert(entry.path().filename().string() + "\n");
		// 		}
		// 		// Iterate through the set and add the files to the output message
		// 		for (const auto& file : files) {
		// 			outputMessage += file;
		// 		}
		// 		stdoutBash(output_file, outputMessage);
		// 	} else {
		// 		stdoutBash(output_file, "ls: " + path + ": No such file or directory");
		// 	}
		// 	continue;
		// }

		if (command == "cd") {
			std::string path = args;
			// Check to see if you are trying to change to the home directory
			if (path == "~") {
				path = HOME;
			}

			// Check if the path is valid
			if (std::filesystem::exists(path)) {
				std::filesystem::current_path(path);
			} else {
				stdoutBash(output_file, "cd: " + path + ": No such file or directory", append);
			}
			continue;
		}

		// ---------------------------------------------------------
		// Base shell loop commands
		// ---------------------------------------------------------

		// Exit the loop if the user types "exit"
		if (command == "exit" && args == "0") {
			break;
		}

		// Simulate the echo command
		if (command == "echo") {
			std::string message;
			bool SingleQuote = false;
			bool DoubleQuote = false;
			bool Escape = false;
			for (char c : args) {
				if (c == '\"' && !SingleQuote && !Escape){
					DoubleQuote = !DoubleQuote;
					continue;
				}
				else if (c == '\'' && !DoubleQuote && !Escape){
					SingleQuote = !SingleQuote;
					continue;
				}
				else if (c == '\\' && !Escape && !SingleQuote){
					Escape = true;
					continue;
				}
				
				if (DoubleQuote) {
					if (Escape && c != '\\' && c != '$' && c != '\"'){
						message += '\\';
					}
					Escape = false;
					message += c;
				}
				else if (SingleQuote){
					message += c;
				}
				else if (Escape){
					message += c;
					Escape = false;
				}
				else if (c == ' ' && message.back() == ' '){ 
					continue;
				}
				else{
					message += c;
				}
			}
			
			if (redirect_code == 2){
				std::cout << message << "\n";
				std::ofstream file(output_file);
				continue;
			}
			stdoutBash(output_file, message, append);
			continue;
		}

		// Simulate the type command
		if (command == "type") {
			bool found = false;
			std::string command = args;
			std::string outputMessage;

			// Check if the command is in the list of builtin commands
			for (const auto& command_iter : commands) {
				if (command_iter == command) {
					outputMessage = command + " is a shell builtin";
					found = true;
					break;
				}
			}
			
			// If the command is not found in the list of commands, check if it is in a path
			if (!found){
				for (const auto& path : split(PATH, ':')) {
					std::string command_path = path + "/" + command;
					// Check if the command exists in the path
					if (std::filesystem::exists(command_path)) {
						outputMessage = command + " is " + command_path;
						found = true;
						break;
					}
				}
			}
			// If the command is not found in the list of commands or the path, print not found
			if (!found) {
				outputMessage = command + ": not found";
			}
			stdoutBash(output_file, outputMessage, append);
			continue;
		}

		// Check to see if the command is an executable file in a directory in the PATH environment variable
		std::string command_test;
		if (std::filesystem::exists(command)) {
			system((command + " " + args).c_str());
			continue;
		}

		bool found = false;
		for (const auto& path : split(PATH, ':')) {
			std::string command_path = path + "/" + command;
			// Check if the command exists in the path
			if (std::filesystem::exists(command_path)) {
				// Execute the command using system call
				system(input.c_str());
				found = true;
				break;
			}
		}
		// If the command is not found in the list of commands or the path, print not found
		if (!found) {
			stdoutBash(output_file, command + ": command not found", append);
		}
	}
	return 0;
}
