#include <iostream>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true){
    std::cout << "$ ";

    // Read a line of input from the user
    std::string input;
    std::getline(std::cin, input);

    if (input == "exit 0") {
      break; // Exit the loop if the user types "exit"
    }

    if (input.find("echo ") == 0) {
      std::cout << input.substr(5) << "\n"; // Simulate the echo command
      continue;
    }
    // Command not found
    std::cout << input << ": command not found" << std::endl;
  }
}
