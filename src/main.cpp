#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>	

std::vector <std::string> commands = {"cd", "ls", "pwd", "echo", "type", "exit", "history"}; 

std::string PATH = getenv("PATH") ? getenv("PATH") : ".";
std::string HOME = getenv("HOME") ? getenv("HOME") : ".";

enum RedirectCode {
	STDOUT = 1,
	STDERR = 2,
	STDNONE = 3 // No redirection, don't write to a file or print to stdout
};

struct CommandData {
	std::string command{};
	std::string args{};
	std::string outputFile{};
    std::string stdoutCmd{};
	std::string stdinCmd{};
	RedirectCode redirectCode{STDOUT};
	bool appendToFile{false};
    bool commandExecuted{false};
	int save_stdout = dup(STDOUT_FILENO); // Save the original stdout file descriptor
};

struct BashData {
	std::string originalInput{};
	unsigned short commandCount{0};
	std::vector<CommandData> commandsData;
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
    static int commandsListIndex, programListIndex;
	static std::vector<std::string> customPrograms;

    if (!state) {
        commandsListIndex = 0;
		programListIndex = 0;
    }

	// Check to see if the command is in the list of commands
    while (commandsListIndex < commands.size()) {
		std::string name = commands[commandsListIndex++];
		if (name.find(text) == 0) {
			return strdup(name.c_str());
		}
	}
	// Clear the custom programs vector if you are starting a new search
	if (programListIndex == 0) {
		customPrograms.clear();
	}
	// If the command is not found in the list of commands, check if it is in a custom program
	if (customPrograms.empty()) {
		for (const auto& path : split(PATH, ':')) {
			// Check if the path is a directory
			if (std::filesystem::is_directory(path)) {
				// Iterate through the directory and add the files to the customPrograms vector
				for (const auto& entry : std::filesystem::directory_iterator(path)) {
					customPrograms.push_back(entry.path().filename().string());
				}
			}
		}
	}
	
	// Check to see if the custom program is in the customPrograms vector
	while (programListIndex < customPrograms.size()) {
		std::string name = customPrograms[programListIndex++];
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

	char *buffer = readline("$ ");
	if (buffer) {
		bashData.originalInput = buffer;
		free(buffer);
	}
}

// ---------------------------------------------------------------
// Function to separate the command, arguments, and output file
// ---------------------------------------------------------------
void separateCommand(BashData& inputData) {
	// Separate the input into multiple commands
	for (const auto& command : split(inputData.originalInput, '|')) {
		CommandData commandData;
		
		// Check if the command needs to be redirected
		std::string redirect_symbol;
		if (command.find("1>>") != std::string::npos) {
			redirect_symbol = "1>>";
			commandData.appendToFile = true;
		}else if (command.find("1>") != std::string::npos) {
			redirect_symbol = "1>";
		}else if (command.find("2>>") != std::string::npos) {
			redirect_symbol = "2>>";
			commandData.redirectCode = STDERR;
			commandData.appendToFile = true;
		}else if (command.find("2>") != std::string::npos) {
			redirect_symbol = "2>";
			commandData.redirectCode = STDERR;
		}else if (command.find(">>") != std::string::npos) {
			redirect_symbol = ">>";
			commandData.appendToFile = true;
		}else if (command.find('>') != std::string::npos) {
			redirect_symbol = '>';
		}
		// If the output needs to be redirected, separate the command and the output file
		if (!redirect_symbol.empty()) {
			commandData.outputFile = command.substr(command.find(redirect_symbol) + redirect_symbol.length());
			commandData.command = command.substr(0, command.find(redirect_symbol));
			// Remove leading whitespace from the output file name
			commandData.outputFile.erase(commandData.outputFile.begin(), std::find_if(commandData.outputFile.begin(), commandData.outputFile.end(), [](unsigned char ch) {
				return !std::isspace(ch);
			}));
		} else {
			commandData.outputFile.clear();
			commandData.command = command;
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

		// Add the command data to the vector of commands and increment the command count
		inputData.commandsData.push_back(commandData);
		inputData.commandCount++;
	}
}

// --------------------------------------------------------------
// Functions to handle navigation commands
// --------------------------------------------------------------

void NavigationCommands(CommandData& commandData) {
	// Check to see ifthe command has been executed already
	if(commandData.commandExecuted){return;}

	if (commandData.command == "pwd"){
		// Get the current working directory
		commandData.stdoutCmd = std::filesystem::current_path().string();
		commandData.commandExecuted = true;
		return;
	}

	if (commandData.command == "cd") {
		std::string path = commandData.args;
		// Check to see if you are trying to change to the home directory
		if (path == "~") {
			path = HOME;
		}

		// Check if the path is valid
		if (std::filesystem::exists(path)) {
			std::filesystem::current_path(path);
			commandData.redirectCode = STDNONE;
		} else {
			commandData.stdoutCmd = "cd: " + path + ": No such file or directory";
		}
		commandData.commandExecuted = true;
		return;
	}
}

// --------------------------------------------------------------
// Function to handle the base shell commands
// --------------------------------------------------------------

void BaseShellCommands(CommandData& commandData) {
	// Check to see if the command has been executed already
	if (commandData.commandExecuted) {return;}

	// Simulate the echo command
	if (commandData.command== "echo") {
		bool SingleQuote = false;
		bool DoubleQuote = false;
		bool Escape = false;
		for (char c : commandData.args) {
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
					commandData.stdoutCmd += '\\';
				}
				Escape = false;
				commandData.stdoutCmd += c;
			}
			else if (SingleQuote){
				commandData.stdoutCmd += c;
			}
			else if (Escape){
				commandData.stdoutCmd += c;
				Escape = false;
			}
			// Remove extra spaces
			else if (c == ' ' && commandData.stdoutCmd.back() == ' '){ 
				continue;
			}
			else{
				commandData.stdoutCmd += c;
			}
		}
		
		if (commandData.redirectCode == STDERR){
			std::cout << commandData.stdoutCmd << "\n";
			commandData.stdoutCmd.clear();
		}
		commandData.commandExecuted = true;
		return;
	}

	// Simulate the type command
	if (commandData.command == "type") {
		// Check if the command is in the list of builtin commands
		for (const auto& command_iter : commands) {
			if (command_iter == commandData.args) {
				commandData.stdoutCmd = commandData.args + " is a shell builtin";
				commandData.commandExecuted = true;
				return;
			}
		}
		
		// If the command is not found in the list of commands, check if it is in a path
		if (!commandData.commandExecuted){
			for (const auto& path : split(PATH, ':')) {
				std::string command_path = path + "/" + commandData.args;
				// Check if the command exists in the path
				if (std::filesystem::exists(command_path)) {
					commandData.stdoutCmd = commandData.args + " is " + command_path;
					commandData.commandExecuted = true;
					return;
				}
			}
		}
		// If the command is not found in the list of commands or the path, print not found
		if (!commandData.commandExecuted) {
			commandData.stdoutCmd = commandData.args + ": not found";
			commandData.commandExecuted = true;
		}
		return;
	}
}

// --------------------------------------------------------------
// Fnction to redirect the output of a command
// --------------------------------------------------------------

void redirectOutput(CommandData& commandData) {
	if (commandData.outputFile.empty()) {return;}

	// Open the file with the appropriate mode (append or overwrite)
	int flags = O_CREAT | (commandData.appendToFile ? O_APPEND : O_WRONLY);
	int fd = open(commandData.outputFile.c_str(), flags, 0777); // 0777 permissions
	if (fd == -1) {
		std::cerr << "Error opening file: " << commandData.outputFile << std::endl;
		return;
	}
	// Redirect STDOUT or STDERR to the file
	dup2(fd, commandData.redirectCode);
	close(fd);
}

// --------------------------------------------------------------
// Function to handle unknown commands
// --------------------------------------------------------------

void UnknownCommand(CommandData& commandData) {
	// Check to see if the command has been executed already
	if (commandData.commandExecuted) {return;}

	// Check to see if the command is an executable file in a directory in the PATH environment variable
	// if (std::filesystem::exists(commandData.command)) {
	// 	system((commandData.command + " " + commandData.args).c_str());
	// 	commandData.commandExecuted = true;
	// 	commandData.redirectCode = STDNONE;
	// 	return;
	// }
	
	for (const auto& path : split(PATH, ':')) {
		std::string command_path = path + "/" + commandData.command;
		// Check if the command exists in the path
		if (std::filesystem::exists(command_path)) {
			system((command_path + " " + commandData.args).c_str());
			commandData.commandExecuted = true;
			commandData.redirectCode = STDNONE;
			fflush(stdout);

			dup2(commandData.save_stdout, STDOUT_FILENO); // Restore original stdout
			close(commandData.save_stdout); // Close the saved stdout file descriptor
			std::cout << commandData.stdoutCmd << "\n"; // Print the output to stdout
			return;
		}
	}
	
	// If the command is not found in the list of commands or the path, print not found
	if (!commandData.commandExecuted) {
		commandData.stdoutCmd = commandData.command + ": command not found";
		commandData.commandExecuted = true;
	}
}

// --------------------------------------------------------------
// Function to write a string to a file or stdout
// --------------------------------------------------------------

// Function to write a string to a file
void stdoutBash(const CommandData& bashInformation) {
	// Check if the filename is empty, and if so, print to stdout
	if (bashInformation.outputFile.empty()) {
		std::cout << bashInformation.stdoutCmd << "\n";
		return;
	}
	std::ofstream file(bashInformation.outputFile, bashInformation.appendToFile ? std::ios::app : std::ios::out);
	if (!file) {
		std::cerr << "Error opening file: " << bashInformation.outputFile << std::endl;
		return;
	}
	if (!bashInformation.stdoutCmd.empty()){
		file << bashInformation.stdoutCmd << "\n";
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
		//std::cout << "$ ";

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

		for (auto& commandData : bashData.commandsData) {
			// Check to see if you the user is trying to use a navigation command
			NavigationCommands(commandData);
			
			// Check to see if you the user is trying to use a base shell command
			BaseShellCommands(commandData);

			redirectOutput(commandData);

			// Check to see if you the user is trying to use an unknown command
			UnknownCommand(commandData);

			// Print the message to the output file or stdout
			if (commandData.redirectCode != STDNONE){
				stdoutBash(commandData);
			}	
		}
	}
	return 0;
}
