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

std::vector<std::string> argumentParts = split(" test dasd asdas dsad", ' ');

// // Prepare the argument list for execvp
// char* argumentList[4] = {};
// for (auto& part :argumentParts){
//     // Add the argument to the argument list
//     argumentList[argumentParts.size()] = const_cast<char*>(part.c_str());
// }
// // Add a nullptr to the end of the argument list
// argumentList[argumentParts.size()] = nullptr;