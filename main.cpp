#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

bool findExecFile(std::string, std::string);

int main() {
  // Flush after every std::cout / std:cerr
  // TODO: Uncomment the code below to pass the first stage
  std::vector<std::string> commandstrings = {"echo", "type", "exit"};

  while (true) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    std::cout << "$ ";
    std::string userCommandInput = "";
    std::getline(std::cin, userCommandInput);

    std::string cmd = "";
    std::string paramStr = "";
    std::istringstream parseUserCommand(userCommandInput);

    parseUserCommand >> cmd;
    if (cmd == "exit") return 0;

    std::getline(parseUserCommand >> std::ws, paramStr);
    if(cmd == "echo") std::cout << paramStr << std::endl;
    else if(cmd == "type") {
      bool foundcmd = false;
      std::string parameterCmd = "";
      std::istringstream parseTemp(paramStr);

      if(parseTemp >> parameterCmd) {
        for(int i=0; i<commandstrings.size(); i++) {
          if(commandstrings[i] == parameterCmd) {
            std::cout << parameterCmd << " is a shell builtin" << std::endl;
            foundcmd = true;
            break;
          }
        }

        if(!foundcmd) {
          const char* pathVal = std::getenv("PATH");
          std::string Path = "";
          std::istringstream pathParse(pathVal);

          while(!foundcmd && std::getline(pathParse, Path, ':')) {
            foundcmd = findExecFile(Path, parameterCmd);
          }

          if(foundcmd) std::cout << parameterCmd << " is " << Path << "/" << parameterCmd << std::endl;
          else std::cout << parameterCmd << ": not found" << std::endl;
        }
      }
    } else {
      std::istringstream parseProgram_Args(userCommandInput);
      std::vector<std::string> argStrings;
      std::string temp = "";
      
      bool foundcmd = false;
      if(parseProgram_Args >> temp) {
        const char* pathVal = std::getenv("PATH");
        std::string Path = "";
        std::istringstream pathParse(pathVal);

        while(!foundcmd && std::getline(pathParse, Path, ':')) {
            foundcmd = findExecFile(Path, temp);
          }
      }

      if(foundcmd) {
        argStrings.push_back(temp);
        while(parseProgram_Args >> temp) {
          argStrings.push_back(temp);
        }
        
        std::vector<char*> programArgs;
        for(auto &strptr : argStrings) {
          programArgs.push_back(const_cast<char*>(strptr.c_str()));
        }
        programArgs.push_back(nullptr);

        pid_t pid = fork();
        if(pid == 0) execvp(programArgs[0], programArgs.data());
        else if(pid > 0) wait(NULL);
      } else std::cout << temp << ": command not found" << std::endl;
    }
  }
  return 0;
}

bool findExecFile(std::string envPath, std::string cmdName) {
  bool fileExists = false; 
  std::filesystem::path currentFileSysPath = envPath;

  if(std::filesystem::exists(currentFileSysPath) && std::filesystem::is_directory(currentFileSysPath)) {
    for(const std::filesystem::directory_entry file : std::filesystem::directory_iterator(currentFileSysPath)) {
      if(file.path().filename() == cmdName) {
        std::filesystem::file_status fileStatus = std::filesystem::status(file.path());
        std::filesystem::perms filePermissions = fileStatus.permissions();

        if((filePermissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
          fileExists = true;
        }
      }
    }
  }

  return fileExists;
}
