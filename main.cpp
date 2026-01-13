#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <filesystem>

int main() {
  // Flush after every std::cout / std:cerr
  // TODO: Uncomment the code below to pass the first stage
  std::vector<std::string> commandstrings = {"echo", "type", "exit"};

  while (true) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    std::string userCommandInput = "";
    std::string command = "";
    std::string parameters = "";
    
    std::cout << "$ ";
    std::getline(std::cin, userCommandInput);
    std::istringstream parseCommand(userCommandInput);

    parseCommand >> command;
    std::getline(parseCommand >> std::ws, parameters);
    if(command == "echo") std::cout << parameters << std::endl;
    else if(command == "type") {
      std::string parameterCmd = "";
      std::istringstream parseTemp(parameters);
      parseTemp >> parameterCmd;
      bool foundcmd = false;

      for(int i=0; i<commandstrings.size(); i++) {
        if(commandstrings[i] == parameterCmd) {
          std::cout << parameterCmd << " is a shell builtin" << std::endl;
          foundcmd = true;
          break;
        }
      }

      if(!foundcmd) {
        const char* pathVal = std::getenv("PATH");
        std::istringstream pathParse(pathVal);
        std::string currentPath = "";
        if(pathVal != nullptr) {
          while(std::getline(pathParse, currentPath, ':') && !foundcmd) {
            std::filesystem::path currentFileSysPath = currentPath;

            if(std::filesystem::exists(currentFileSysPath) && std::filesystem::is_directory(currentFileSysPath)) {
              for(const auto &file : std::filesystem::directory_iterator(currentFileSysPath)) {
                if(file.path().filename() == parameterCmd) {
                  std::filesystem::file_status fileStatus = std::filesystem::status(file.path());
                  std::filesystem::perms filePermissions = fileStatus.permissions();

                  if((filePermissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
                    std::cout << parameterCmd << " is " << currentPath << "/" << parameterCmd << std::endl;
                    foundcmd = true;
                    break;
                  }
                }
              }
            } else std::cout << currentPath << " : directory doesn't exists" << std::endl;
          }
        } else std::cout << "not found path variables" << std::endl;
      }

      if(!foundcmd) std::cout << parameterCmd << ": not found" << std::endl;
    }
    else if (command == "exit") return 0;
    else std::cout << command << ": command not found" << std::endl;
  }
  
  return 0;
}
