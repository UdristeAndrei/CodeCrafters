#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <set>
#include <readline/readline.h>
#include <readline/history.h>	

std::vector <std::string> commands = {"cd", "ls", "pwd", "echo", "type", "exit"}; 

std::string PATH = getenv("PATH") ? getenv("PATH") : ".";
std::string HOME = getenv("HOME") ? getenv("HOME") : ".";

enum RedirectCode {
	STDOUT = 1,
	STDERR = 2,
	STDNONE = 3 // No redirection, don't write to a file or print to stdout
};

struct BashData {
	std::string originalInput{};
	std::string command{};
	std::string args{};
	std::string outputFile{};
    std::string message{};
	RedirectCode redirectCode{STDOUT};
	bool appendToFile{false};
    bool commandExecuted{false};
};
// --------------------------------------------------------------
// Utility functions
// --------------------------------------------------------------

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

// --------------------------------------------------------------
// Function to handle the autocompletion of commands
// --------------------------------------------------------------
// Function to generate matches for autocompletion

// Function to generate command matches
char* commandGenerator(const char *text, int state)
{
    static int list_index;

    if (!state) {
        list_index = 0;
    }

    while (list_index < commands.size()) {
		std::string name = commands[list_index++];
		if (name.find(text) == 0) {
			return strdup(name.c_str());
		}
	}

    return nullptr;
}

char** commandCompletion(const char *text, int start, int end)
{
 	if (start != 0) {
        return nullptr; // Only autocomplete at the start of the line
    }

    return rl_completion_matches(text, commandGenerator);
}

void AutocompletePath(BashData& bashData) {
	rl_attempted_completion_function = commandCompletion;

	char *buffer = readline("");
	if (buffer) {
		bashData.originalInput = buffer;
		free(buffer);
	}
}

// ---------------------------------------------------------------
// Function to separate the command, arguments, and output file
// ---------------------------------------------------------------
void separateCommand(BashData& commandData) {
	// Check if the output needs to be redirected
	std::string redirect_symbol;
	if (commandData.originalInput.find("1>>") != std::string::npos) {
		redirect_symbol = "1>>";
		commandData.appendToFile = true;
	}else if (commandData.originalInput.find("1>") != std::string::npos) {
		redirect_symbol = "1>";
	}else if (commandData.originalInput.find("2>>") != std::string::npos) {
		redirect_symbol = "2>>";
		commandData.redirectCode = STDERR;
		commandData.appendToFile = true;
	}else if (commandData.originalInput.find("2>") != std::string::npos) {
		redirect_symbol = "2>";
		commandData.redirectCode = STDERR;
	}else if (commandData.originalInput.find(">>") != std::string::npos) {
		redirect_symbol = ">>";
		commandData.appendToFile = true;
	}else if (commandData.originalInput.find('>') != std::string::npos) {
		redirect_symbol = '>';
	}
	// If the output needs to be redirected, separate the command and the output file
	if (!redirect_symbol.empty()) {
		commandData.outputFile = commandData.originalInput.substr(commandData.originalInput.find(redirect_symbol) + redirect_symbol.length());
		commandData.command = commandData.originalInput.substr(0, commandData.originalInput.find(redirect_symbol));
		// Remove leading whitespace from the output file name
		commandData.outputFile.erase(commandData.outputFile.begin(), std::find_if(commandData.outputFile.begin(), commandData.outputFile.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
	} else {
		commandData.outputFile.clear();
		commandData.command = commandData.originalInput;
	}
	// Check if the command is enclosed in quotes
	std::string delimiter;
	bool isQuoted = false;
	if (commandData.command.find('\'') == 0 ){
		delimiter = "'";
		isQuoted = true;
	} else if (commandData.command.find('\"') == 0) {
		delimiter = "\"";
		isQuoted = true;
	} else {
		delimiter = " ";
	}
	// Separate the command and the arguments
	commandData.args = commandData.command.substr(commandData.command.find(delimiter, 1) + 1);
	commandData.command = commandData.command.substr(isQuoted, commandData.command.find(delimiter, 1) - isQuoted);
}

// --------------------------------------------------------------
// Functions to handle navigation commands
// --------------------------------------------------------------

void NavigationCommands(BashData& bashData) {
	// Check to see ifthe command has been executed already
	if(bashData.commandExecuted){return;}

	if (bashData.command == "pwd"){
		// Get the current working directory
		bashData.message = std::filesystem::current_path().string();
		bashData.commandExecuted = true;
		return;
	}

	if (bashData.command == "cd") {
		std::string path = bashData.args;
		// Check to see if you are trying to change to the home directory
		if (path == "~") {
			path = HOME;
		}

		// Check if the path is valid
		if (std::filesystem::exists(path)) {
			std::filesystem::current_path(path);
			bashData.redirectCode = STDNONE;
		} else {
			bashData.message = "cd: " + path + ": No such file or directory";
		}
		bashData.commandExecuted = true;
		return;
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

}

// --------------------------------------------------------------
// Function to handle the base shell commands
// --------------------------------------------------------------

void BaseShellCommands(BashData& bashData) {
	// Check to see if the command has been executed already
	if (bashData.commandExecuted) {return;}

	// Simulate the echo command
	if (bashData.command== "echo") {
		bool SingleQuote = false;
		bool DoubleQuote = false;
		bool Escape = false;
		for (char c : bashData.args) {
			// Check for quotes and escape characters
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
			
			// Check to see if you are in a quote
			if (DoubleQuote) {
				// When trying to add a "\" inside a double quote, without trying to escape a character
				if (Escape && c != '\\' && c != '$' && c != '\"'){
					bashData.message += '\\';
				}
				Escape = false;
				bashData.message += c;
			}
			else if (SingleQuote){
				bashData.message += c;
			}
			else if (Escape){
				bashData.message += c;
				Escape = false;
			}
			// Remove extra spaces
			else if (c == ' ' && bashData.message.back() == ' '){ 
				continue;
			}
			else{
				bashData.message += c;
			}
		}
		
		if (bashData.redirectCode == STDERR){
			std::cout << bashData.message << "\n";
			bashData.message.clear();
		}
		bashData.commandExecuted = true;
		return;
	}

	// Simulate the type command
	if (bashData.command == "type") {

		// Check if the command is in the list of builtin commands
		for (const auto& command_iter : commands) {
			if (command_iter == bashData.args) {
				bashData.message = bashData.args + " is a shell builtin";
				bashData.commandExecuted = true;
				return;
			}
		}
		
		// If the command is not found in the list of commands, check if it is in a path
		if (!bashData.commandExecuted){
			for (const auto& path : split(PATH, ':')) {
				std::string command_path = path + "/" + bashData.args;
				// Check if the command exists in the path
				if (std::filesystem::exists(command_path)) {
					bashData.message = bashData.args + " is " + command_path;
					bashData.commandExecuted = true;
					return;
				}
			}
		}
		// If the command is not found in the list of commands or the path, print not found
		if (!bashData.commandExecuted) {
			bashData.message = bashData.args + ": not found";
			bashData.commandExecuted = true;
		}
		return;
	}
}

// --------------------------------------------------------------
// Function to handle unknown commands
// --------------------------------------------------------------

void UnknownCommand(BashData& bashData) {
	// Check to see if the command has been executed already
	if (bashData.commandExecuted) {return;}

	// Check to see if the command is an executable file in a directory in the PATH environment variable
	if (std::filesystem::exists(bashData.command)) {
		system((bashData.command + " " + bashData.args).c_str());
		bashData.commandExecuted = true;
		bashData.redirectCode = STDNONE;
		return;
	}
	
	for (const auto& path : split(PATH, ':')) {
		std::string command_path = path + "/" + bashData.command;
		// Check if the command exists in the path
		if (std::filesystem::exists(command_path)) {
			// Execute the command using system call
			system(bashData.originalInput.c_str());
			bashData.commandExecuted = true;
			bashData.redirectCode = STDNONE;
			return;
		}
	}
	
	// If the command is not found in the list of commands or the path, print not found
	if (!bashData.commandExecuted) {
		bashData.message = bashData.command + ": command not found";
		bashData.commandExecuted = true;
	}
}

// --------------------------------------------------------------
// Function to write a string to a file or stdout
// --------------------------------------------------------------

// Function to write a string to a file
void stdoutBash(const BashData& bashInformation) {
	// Check if the filename is empty, and if so, print to stdout
	if (bashInformation.outputFile.empty()) {
		std::cout << bashInformation.message << "\n";
		return;
	}
	
	std::ofstream file(bashInformation.outputFile, bashInformation.appendToFile ? std::ios::app : std::ios::out);
	if (!file) {
		std::cerr << "Error opening file: " << bashInformation.outputFile << std::endl;
		return;
	}
	if (!bashInformation.message.empty()){
		file << bashInformation.message << "\n";
	}
	file.close();
}

// --------------------------------------------------------------
// Main function
// --------------------------------------------------------------

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
   	// Configure readline to auto-complete paths when the tab key is hit.
   	rl_attempted_completion_function = commandCompletion;
    

    while (true){
		std::cout << "$ ";

        BashData bashData{};
		
		// Get the input from the user and try to autocomplete it
		AutocompletePath(bashData);
		if (bashData.originalInput.empty()) {
			continue; // Skip empty input
		}else if (bashData.originalInput == "exit 0") {
			break; // Exit the shell
		}
	
		// Process the input command
		separateCommand(bashData);

		// Check to see if you the user is trying to use a navigation command
		NavigationCommands(bashData);
		
		// Check to see if you the user is trying to use a base shell command
		BaseShellCommands(bashData);

		// Check to see if you the user is trying to use an unknown command
		UnknownCommand(bashData);

		// Print the message to the output file or stdout
		if (bashData.redirectCode != STDNONE){
			stdoutBash(bashData);
		}	
	}
	return 0;
}
