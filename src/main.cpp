#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <set>
#include <unistd.h>
#include <sys/wait.h>
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
	std::string originalInput{};
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

bool searchPath(const CommandData& commandData, std::string& foundPath) {
	std::string Command = commandData.command;

	// Check to see if the coomand is between quotes
	if (commandData.isQuoted) {
		// Remove the quotes from the command and add the path
		Command.erase(0, 1); // Remove the first quote
		Command.erase(Command.size() - 1); // Remove the last quote
	}

	for (const auto& path : split(PATH, ':')) {

		std::string command_path = path + "/" + Command;
		// Check if the command or unquoted command exists in the path 
		if (std::filesystem::exists(command_path)) {
			foundPath = path + "/" + commandData.command; // Set the path to the command
			return true;
		}
	}
	return false; // If the command is not found in the path, return false
}

// --------------------------------------------------------------
// Function to handle the autocompletion of commands
// --------------------------------------------------------------

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

void AutocompletePath(CommandData& commandData) {
	rl_attempted_completion_function = commandCompletion;

	char *buffer = readline("$ ");
	if (buffer) {
		commandData.originalInput = buffer;
		free(buffer);
	}
}

// ---------------------------------------------------------------
// Function to separate the command, arguments, and output file
// ---------------------------------------------------------------

void removeBlankSpaces(std::string& str) {
	// Remove leading and trailing whitespace from the string
	str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
		return !std::isspace(ch);
	}));
	str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
	}).base(), str.end());
}

void separateCommand(CommandData& inputData) {
	// If the command is empty, skip it
	if (inputData.originalInput.empty()) {
		return;
	}

	//Remove leading and trailing whitespace from the command
	removeBlankSpaces(inputData.originalInput);
	
	// Check if the command needs to be redirected
	std::string redirect_symbol;
	if (inputData.originalInput.find("1>>") != std::string::npos) {
		redirect_symbol = "1>>";
		inputData.appendToFile = true;
	}else if (inputData.originalInput.find("1>") != std::string::npos) {
		redirect_symbol = "1>";
	}else if (inputData.originalInput.find("2>>") != std::string::npos) {
		redirect_symbol = "2>>";
		inputData.redirectCode = STDERR_FILE;
		inputData.appendToFile = true;
	}else if (inputData.originalInput.find("2>") != std::string::npos) {
		redirect_symbol = "2>";
		inputData.redirectCode = STDERR_FILE;
	}else if (inputData.originalInput.find(">>") != std::string::npos) {
		redirect_symbol = ">>";
		inputData.appendToFile = true;
	}else if (inputData.originalInput.find('>') != std::string::npos) {
		redirect_symbol = '>';
	}
	// If the output needs to be redirected, separate the command and the output file
	if (!redirect_symbol.empty()) {
		inputData.outputFile = inputData.originalInput.substr(inputData.originalInput.find(redirect_symbol) + redirect_symbol.length());
		inputData.command = inputData.originalInput.substr(0, inputData.originalInput.find(redirect_symbol));
		// Remove leading whitespace from the output file name
		inputData.outputFile.erase(inputData.outputFile.begin(), std::find_if(inputData.outputFile.begin(), inputData.outputFile.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
	} else {
		inputData.outputFile.clear();
		inputData.command = inputData.originalInput;
	}
	// Check if the command is enclosed in quotes
	std::string delimiter;
	inputData.isQuoted = false;
	if (inputData.command.find('\'') == 0 ){
		delimiter = "'";
		inputData.isQuoted = true;
	} else if (inputData.command.find('\"') == 0) {
		delimiter = "\"";
		inputData.isQuoted = true;
	} else {
		delimiter = " ";
	}

	//Check to see if you ony have a command without arguments
	if (inputData.command.find(delimiter, 1) == std::string::npos) {
		// If the command is only a command without arguments, set the args to an empty string
		inputData.args.clear();
		inputData.command = inputData.command.substr(0, inputData.command.find(delimiter, 1) + inputData.isQuoted);
		// Remove leading and trailing whitespace from the command
		removeBlankSpaces(inputData.command);
		return;
	}
	// Separate the command and the arguments
	inputData.args = inputData.command.substr(inputData.command.find(delimiter, 1) + 1);
	inputData.command = inputData.command.substr(0, inputData.command.find(delimiter, 1) + inputData.isQuoted);
	// Remove leading and trailing whitespace from the command and arguments
	removeBlankSpaces(inputData.command);
	removeBlankSpaces(inputData.args);
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
		if (all_of(commandData.args.begin(), commandData.args.end(), ::isdigit) && !commandData.args.empty()) {
			// Print the last n commands if the user specified an index
			unsigned int index = std::stoi(commandData.args);
			if (index > 0 && index <= commandHistory.size()) {
				historyIndex = commandHistory.size() - index; 
			}
		}

		// Go through the command history and add it to the stdoutCmd
		for (historyIndex; historyIndex < commandHistory.size(); ++historyIndex) {
			commandData.stdoutCmd += "    " + std::to_string(commandHistory.size() + 1) + "  " +  commandHistory[historyIndex] + "\n";
		}
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
		commandData.stdoutCmd = std::filesystem::current_path().string() + "\n";
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
			commandData.stdoutCmd = "cd: " + path + ": No such file or directory\n";
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
			commandData.commandExecuted = true;
			return;
		}
		commandData.stdoutCmd += "\n"; // Add a newline at the end of the output
		commandData.commandExecuted = true;
		return;
	}

	// Simulate the type command
	if (commandData.command == "type") {
		// Check if the command is in the list of builtin commands
		for (const auto& command_iter : commands) {
			if (command_iter == commandData.args) {
				commandData.stdoutCmd = commandData.args + " is a shell builtin\n";
				commandData.commandExecuted = true;
				return;
			}
		}
		
		// If the command is not found in the list of commands, check if it is in a path
		if (!commandData.commandExecuted){
			for (const auto& path : split(PATH, ':')) {
				std::string command_path = path + "/" + commandData.args;
				
				// Check if the command exists in the path
				if (std::filesystem::exists(command_path) && access(command_path.c_str(), X_OK) == 0) {
					commandData.stdoutCmd = commandData.args + " is " + command_path + "\n";
					commandData.commandExecuted = true;
					return; // Exit the function after executing the command
				}
			}
		}
		// If the command is not found in the list of commands or the path, print not found
		if (!commandData.commandExecuted) {
			commandData.stdoutCmd = commandData.args + ": not found\n";
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

void RunUnknownCommand(CommandData& commandData) {
	// Check to see if the command has been executed already
	if (commandData.commandExecuted) {return;}

	std::string commandPath{};
	// Check if the command is in the path
	if (searchPath(commandData, commandPath)) {
		// If the command is found in the path execute it
		commandData.commandExecuted = true;
		system((commandData.command + " " + commandData.args).c_str()); // Execute the command using system()
		return; // Exit the function after executing the command
	
	// If the command is not found in the list of commands or the path, print not found
	}else{
		commandData.stdoutCmd = commandData.command + ": command not found\n";
		commandData.commandExecuted = true;
	}
	
	
}

// --------------------------------------------------------------
// Function to handle pipes and process execution
// --------------------------------------------------------------

void RunPipeCommand(CommandData& commandData) {
	// Check to see if the command has been executed already
	if (commandData.commandExecuted) {return;}

	// Check to see if the command is in the path
	std::string commandPath;
	if (searchPath(commandData, commandPath)) {
		commandData.commandExecuted = true;

		// Prepare the argument list for execvp
		std::vector<std::string> args; 
		std::vector<char*> argsVector;

		argsVector.push_back(const_cast<char*>(commandPath.c_str())); // Add the command
		if (!commandData.args.empty()){
			// Split the arguments by spaces and add them to the argsVector
			args = split(commandData.args, ' ');
			for (const auto &arg : args) {
				argsVector.push_back(const_cast<char*>(arg.c_str())); // Add the argument and null-terminate it
			}
		}
		argsVector.push_back(nullptr); // Null-terminate the argument list
		
		execvp(commandPath.c_str(), argsVector.data());
		perror("execvp failed");
		exit(EXIT_FAILURE); // Exit if execvp fails
	
	// If the command is not found in the list of commands or the path, print not found
	}else{
		commandData.stdoutCmd = commandData.command + ": command not found\n";
		commandData.commandExecuted = true;
	}
	
}

bool isBuiltInCommand(const std::string& command) {
	// Check if the command is in the list of builtin commands
	return std::find(commands.begin(), commands.end(), command) != commands.end();
}

void runBuidInCommands(CommandData& commandData) {
	// Check to see if the command has been executed already
	if (commandData.commandExecuted) {return;}

	// Execute hystory commands
	HistoryCommands(commandData);

	// Check to see if you the user is trying to use a navigation command
	NavigationCommands(commandData);
	
	// Check to see if you the user is trying to use a base shell command
	BaseShellCommands(commandData);

	// Redirect the output of the command to a file or stdout
	RedirectOutputFile(commandData);
}

void runPipes(std::string& command) {
	// Separate the command into the command and arguments
	std::vector<CommandData> commandsData;
	for (const auto& cmd : split(command, '|')) {
		CommandData commandData;
		commandData.originalInput = cmd;
		separateCommand(commandData);
		if (!commandData.command.empty()) {
			commandsData.push_back(commandData);
		}
	}

	if (commandsData.size() < 2) {
        std::cerr << "Error: Need at least 2 commands for pipe\n";
        return;
    }

    // Create pipes for inter-process communication
	int numPipes = commandsData.size() - 1;
    std::vector<int[2]> pipes(numPipes);

	// Create all pipes
    for (int i = 0; i < numPipes; i++) {
        if (pipe(pipes[i]) == -1) {
            std::cerr << "Error creating pipe " << i << std::endl;
            return;
        }
    }

	std::vector<pid_t> pids;

    // Fork and execute each command
    for (size_t i = 0; i < commandsData.size(); i++) {
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error forking process for command " << i << std::endl;
            return;
        }
        
        if (pid == 0) {
            // Child process
            
            // Set up input redirection (except for first command)
            if (i > 0) {
                // Redirect stdin from previous pipe
                if (dup2(pipes[i-1][0], STDIN_FILENO) == -1) {
                    std::cerr << "Error redirecting stdin for command " << i << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            
            // Set up output redirection (except for last command)
            if (i < commandsData.size() - 1) {
                // Redirect stdout to next pipe
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    std::cerr << "Error redirecting stdout for command " << i << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close all pipe file descriptors in child
            for (int j = 0; j < numPipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command
            if (isBuiltInCommand(commandsData[i].command)) {
                runBuidInCommands(commandsData[i]);
                if (!commandsData[i].stdoutCmd.empty()) {
                    std::cout << commandsData[i].stdoutCmd;
                }
                exit(EXIT_SUCCESS);
            } else {
                RunPipeCommand(commandsData[i]);
            }
        } else {
            // Parent process - store child PID
            pids.push_back(pid);
        }
    }

    // Close all pipe file descriptors in parent
    for (int i = 0; i < numPipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes to finish
    for (pid_t pid : pids) {
        waitpid(pid, nullptr, 0);
    }
	
	return;
}

// --------------------------------------------------------------
// Main function
// --------------------------------------------------------------

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
   	// Configure readline to auto-complete paths when the tab key is hit.
   	rl_attempted_completion_function = commandCompletion;
	// char* argumentList[] = {"-f",  "/temp/file-83", NULL};

	// execvp("tail", argumentList);
    
    while (true){
		int OrigStdout = dup(STDOUT_FILENO);
		int OrigStderr = dup(STDERR_FILENO);
		int OrigStdin = dup(STDIN_FILENO);
        CommandData bashData{};

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

		if (bashData.originalInput.find('|') != std::string::npos) {
			// If the command contains a pipe, run the pipe function
			runPipes(bashData.originalInput);
			continue; // Skip the rest of the loop
		}
	
		// Process the input command
		separateCommand(bashData);

		// Execute hystory commands
		HistoryCommands(bashData);

		// Check to see if you the user is trying to use a navigation command
		NavigationCommands(bashData);
		
		// Check to see if you the user is trying to use a base shell command
		BaseShellCommands(bashData);

		// Redirect the output of the command to a file or stdout
		RedirectOutputFile(bashData);

		// Check to see if you the user is trying to use an unknown command
		RunUnknownCommand(bashData);

		// If the command has been executed, print the output
		if (bashData.commandExecuted && !bashData.stdoutCmd.empty()) {
			std::cout << bashData.stdoutCmd;
		}
		
		// Restore the original stdout and stderr
		// Flush stdout and stderr to ensure all output is written
		std::fflush(stdout);
		std::fflush(stderr); 

		// Restore the original stdout and stderr
		dup2(OrigStdout, STDOUT_FILENO); 
		dup2(OrigStderr, STDERR_FILENO);
		dup2(OrigStdin, STDIN_FILENO);

		// Close the original stdout and stderr file descriptors
    	close(OrigStdout);
		close(OrigStderr);
		close(OrigStdin);
	}
	return 0;
}
