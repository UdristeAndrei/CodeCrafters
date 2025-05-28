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

int main(){

    int pfd = open("file.txt", O_WRONLY | O_CREAT, 0777);
    int saved = dup(1);
    close(1);
    dup(pfd);
    close(pfd);
    std::printf("This goes into file\n");
    std::fflush(stdout);  // <-- THIS

    // restore it back
    dup2(saved, 1);
    close(saved);
    std::printf("this goes to stdout");
}