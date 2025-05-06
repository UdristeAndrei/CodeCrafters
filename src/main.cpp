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
      for (const auto& command : commands) {
        if (input.substr(5) == command) {
          std::cout << input.substr(5) << " is a shell builtin\n";
          found = true;
          break;
        }
      }
      
      // If the command is not found in the list of commands, check if it is a path
      if (!found){
        for (const auto& path : split(PATH, ':')) {
          if (std::filesystem::exists(path + "/" + input.substr(5))) {
            std::cout << input.substr(5) << " is " << path << "\n";
            found = true;
            break;
          }
        }

        // If the command is not found in the list of commands or the path, print not found
        if (!found) {
          std::cout << input.substr(5) << ": not found\n"; 
        }
      }
      continue;
    }

    // Command not found
    std::cout << input << ": command not found" << std::endl;
  }
}
