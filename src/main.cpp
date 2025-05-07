#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <filesystem>

std::array<std::string, 3> commands = {"echo", "exit", "type"};
std::string PATH = getenv("PATH");

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

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    while (true){
      	std::cout << "$ ";

		// Read a line of input from the user
		std::string input;
		std::getline(std::cin, input);

		// Exit the loop if the user types "exit"
		if (input == "exit 0") {
			break;
		}

		// Simulate the echo command
		if (input.find("echo ") == 0) {
			std::cout << input.substr(5) << "\n"; 
			continue;
		}

		// Simulate the type command
		if (input.find("type ") == 0) {
			bool found = false;
			std::string command = input.substr(5);

			// Check if the command is in the list of builtin commands
			for (const auto& command_iter : commands) {
				if (command_iter == command) {
					std::cout << command << " is a shell builtin\n";
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
						std::cout << command << " is " << command_path << "\n";
						found = true;
						break;
					}
				}
			}
			continue;
		}

		// Check to see if the command is an executable file in a directory in the PATH environment variable
		std::string command = input.substr(0, input.find(" "));
		bool found = false;
		for (const auto& path : split(PATH, ':')) {
			std::string command_path = path + "/" + command;
			// Check if the command exists in the path
			if (std::filesystem::exists(command_path)) {
				// Execute the command using system call
				system((command_path + " " + input.substr(input.find(" "))).c_str());
				std::cout << command_path + " " + input.substr(input.find(" ")) << "\n";
				found = true;
				break;
			}
		}
		// If the command is not found in the list of commands or the path, print not found
		if (!found) {
			std::cout << input << ": command not found" << std::endl;
		}
	}
}
