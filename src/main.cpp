#include <iostream>
#include <string>
#include <array>

std::array<std::string, 3> commands = {"echo", "exit", "type"};

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
      for (const auto& command : commands) {
        if (input.substr(5) == command) {
          std::cout << command << " is a shell builtin\n"; 
          break;
        }
      }
      std::cout << input.substr(5) << ": not found\n";
      continue;
    }
    
    // Command not found
    std::cout << input << ": command not found" << std::endl;
  }
}
