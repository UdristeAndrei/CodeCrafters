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
std::vector <std::string> commandHistory; // Vector to store command history

std::string PATH = getenv("PATH") ? getenv("PATH") : ".";
std::string HOME = getenv("HOME") ? getenv("HOME") : ".";

unsigned short navigationHistoryIndex = 0; // Index for the navigation history

enum RedirectCode {
	STDOUT_FILE = 1,
	STDERR_FILE = 2,
	STDOUT_NONE = 3 // No redirection, don't write to a file or print to stdout
};

struct CommandData {
	std::string command{};
	std::string args{};
	std::string outputFile{};
    std::string stdoutCmd{};
	std::string stdinCmd{};
	RedirectCode redirectCode{STDOUT_FILE};
	bool appendToFile{false};
    bool commandExecuted{false};
	bool isQuoted{false}; // Indicates if the command is enclosed in quotes
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
			commandData.redirectCode = STDERR_FILE;
			commandData.appendToFile = true;
		}else if (command.find("2>") != std::string::npos) {
			redirect_symbol = "2>";
			commandData.redirectCode = STDERR_FILE;
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
		commandData.isQuoted = false;
		if (commandData.command.find('\'') == 0 ){
			delimiter = "'";
			commandData.isQuoted = true;
		} else if (commandData.command.find('\"') == 0) {
			delimiter = "\"";
			commandData.isQuoted = true;
		} else {
			delimiter = " ";
		}

		// Separate the command and the arguments
		commandData.args = commandData.command.substr(commandData.command.find(delimiter, 1) + 1);
		commandData.command = commandData.command.substr(0, commandData.command.find(delimiter, 1) + commandData.isQuoted);

		// Remove leading whitespace from the command
		commandData.command.erase(commandData.command.begin(), std::find_if(commandData.command.begin(), commandData.command.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));

		// Add the command data to the vector of commands and increment the command count
		inputData.commandsData.push_back(commandData);
		inputData.commandCount++;
	}
}

// --------------------------------------------------------------
// Function to handle history commands
// --------------------------------------------------------------

int historyNavFct (int count, int key) {
	// If you press the up arrow, go to the previous command
	if (key == 65) {
		if (navigationHistoryIndex > 0) {
			navigationHistoryIndex--;
		}
	// If you press the down arrow, go to the next command
	} else if (key == 66) {
		if (navigationHistoryIndex < commandHistory.size()) {
			navigationHistoryIndex++;
		}
	}
	// Clear current line and write the command to the line
	std::string command = navigationHistoryIndex < commandHistory.size() ? commandHistory[navigationHistoryIndex] : "";
	rl_replace_line(command.c_str(), 1);
	rl_point = rl_end;
	rl_redisplay();
	return 0;
}

void arrowNavigation() {
	rl_command_func_t historyNavFct;
	rl_bind_keyseq ("\\e[A", historyNavFct); // ascii code for UP ARROW
	rl_bind_keyseq ("\\e[B", historyNavFct); // ascii code for DOWN ARROW
}

void AddToHistory(const std::string& command) {
	// Add the command to the history in the right format
	navigationHistoryIndex++;
	commandHistory.push_back(command);
}

void HistoryCommands(CommandData& commandData) {
	// If the command is "history", print the command history
	if (commandData.command == "history") {
		// Get hte index from where the history should start
		unsigned int historyIndex{0};
		// Check if the user specified an index
		if (all_of(commandData.args.begin(), commandData.args.end(), ::isdigit)) {
			// Print the last n commands if the user specified an index
			unsigned int index = std::stoi(commandData.args);
			if (index > 0 && index <= commandHistory.size()) {
				historyIndex = commandHistory.size() - index; 
			}
		}

		// Go through the command history and add it to the stdoutCmd
		for (historyIndex; historyIndex < commandHistory.size(); ++historyIndex) {
			commandData.stdoutCmd += "    " + std::to_string(commandHistory.size() + 1) + "  " +  commandHistory[historyIndex];
			if (historyIndex != commandHistory.size() - 1) {
				commandData.stdoutCmd += "\n";
			}
		}
		commandHistory.clear();
		commandData.commandExecuted = true;
		return;
	}

	// If the command is "clear", clear the command history
	if (commandData.command == "clear") {
		commandHistory.clear();
		commandData.stdoutCmd = "Command history cleared.";
		commandData.commandExecuted = true;
		return;
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
			commandData.redirectCode = STDOUT_NONE;
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
		
		if (commandData.redirectCode == STDERR_FILE){
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

void RedirectOutputFile(CommandData& commandData) {
	if (commandData.outputFile.empty()) {return;}

	// Open the file with the appropriate mode (append or overwrite)
	int flags = O_WRONLY | O_CREAT | (commandData.appendToFile ? O_APPEND : O_TRUNC);
	int fd = open(commandData.outputFile.c_str(), flags, 0777); // 0777 permissions
	if (fd == -1) {
		std::cerr << "Error opening file: " << commandData.outputFile << std::endl;
		return;
	}
	// Redirect STDOUT or STDERR to the file
	dup2(fd, commandData.redirectCode);
	close(fd);

	if (commandData.redirectCode == STDOUT_FILE) {
		commandData.outputFile.clear();
	}
}

// --------------------------------------------------------------
// Function to handle unknown commands
// --------------------------------------------------------------

void UnknownCommand(CommandData& commandData) {
	// Check to see if the command has been executed already
	if (commandData.commandExecuted) {return;}

	int pipefd[2];
    pipe(pipefd);

	// if (std::filesystem::exists(commandData.command)) {
	// 	std::cout << "Executing command 1: " << commandData.command << "\n";
	// 	// If the command is a file, execute it
	// 	system((commandData.command + " " + commandData.args).c_str());
	// 	commandData.commandExecuted = true;
	// 	commandData.redirectCode = STDOUT_NONE;
	// 	return;
	// }

	pid_t pid = fork();
    if (pid == 0) {
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);

		for (const auto& path : split(PATH, ':')) {
			std::string originalCommand = commandData.command;

			// Check to see if the coomand is between quotes
			if (commandData.isQuoted) {
				// Remove the quotes from the command and add the path
				commandData.command.erase(0, 1); // Remove the first quote
				commandData.command.erase(commandData.command.size() - 1); // Remove the last quote
			}
			std::string command_path = path + "/" + commandData.command;
			// Check if the command or unquoted command exists in the path 
			if (std::filesystem::exists(command_path)) {
				commandData.commandExecuted = true;
				commandData.redirectCode = STDOUT_NONE;
				execlp(originalCommand.c_str(), commandData.args.c_str(), NULL);
				//system((originalCommand + " " + commandData.args).c_str());
				break; // Exit the loop if the command is found
			}
		}
		// If the command is not found in the list of commands or the path, print not found
		if (!commandData.commandExecuted) {
			commandData.stdoutCmd = commandData.command + ": command not found";
			commandData.commandExecuted = true;
		}

		exit(1); // Exit the child process
	}
	// Parent: write echo output to pipe, close write end
	close(pipefd[0]);
	write(pipefd[1], commandData.stdinCmd.c_str(), commandData.stdinCmd.size());
	close(pipefd[1]);
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
		int OrigStdout = dup(STDOUT_FILENO);
		int OrigStderr = dup(STDERR_FILENO);
        BashData bashData{};

		arrowNavigation();

		// Get the input from the user and try to autocomplete it
		AutocompletePath(bashData);
		if (bashData.originalInput.empty()) {
			continue; // Skip empty input
		}else if (bashData.originalInput == "exit 0") {
			break; // Exit the shell
		}

		// Add the command to the history
		AddToHistory(bashData.originalInput);
	
		// Process the input command
		separateCommand(bashData);
		std::string previousStdout{};

		for (auto& commandData : bashData.commandsData) {
			commandData.stdinCmd = previousStdout; // Set the stdin for the command

			// Execute hystory commands
			HistoryCommands(commandData);

			// Check to see if you the user is trying to use a navigation command
			NavigationCommands(commandData);
			
			// Check to see if you the user is trying to use a base shell command
			BaseShellCommands(commandData);

			// Redirect the output of the command to a file or stdout
			RedirectOutputFile(commandData);

			// Check to see if you the user is trying to use an unknown command
			UnknownCommand(commandData);
			
			previousStdout = commandData.stdoutCmd; // Set the stdin for the next command
		}

		CommandData& commandData = bashData.commandsData.back(); // Get the last command data
		// Print the message to the output file or stdout
		if (commandData.redirectCode != STDOUT_NONE) {
			std::cout << commandData.stdoutCmd << "\n";
		}

		std::fflush(stdout);
		std::fflush(stderr);  // Flush stdout and stderr to ensure all output is written
		dup2(OrigStdout, STDOUT_FILENO); // Restore original stdout
		dup2(OrigStderr, STDERR_FILENO); // Restore original stderr

    	close(OrigStdout);
		close(OrigStderr);
	}
	return 0;
}
