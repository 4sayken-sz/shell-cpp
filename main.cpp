#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h> //execve
#include <sys/types.h>
#include <sys/wait.h> //wait

bool findExecFile(std::string, std::string);
std::vector<std::string> parseUserArguments(std::string);

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

    parseUserCommand >> cmd; // extract command
    if (cmd == "exit") return 0;
    std::getline(parseUserCommand >> std::ws, paramStr); // extract arguments

    if(cmd == "echo") {
      std::vector<std::string> echoParams = parseUserArguments(paramStr);

      for(const std::string &str : echoParams) std::cout << str << " ";
      std::cout << std::endl;
    } else if(cmd == "type") {
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
      std::vector<std::string> parameters = parseUserArguments(paramStr);
      std::vector<std::string> argStrings;
      argStrings.push_back(cmd);
      
      bool foundcmd = false;
      const char* pathVal = std::getenv("PATH");
      std::string Path = "";
      std::istringstream pathParse(pathVal);

      while(!foundcmd && std::getline(pathParse, Path, ':')) {
        foundcmd = findExecFile(Path, argStrings[0]);
      }
      
      if(foundcmd) {
        argStrings.insert(argStrings.end(), parameters.begin(), parameters.end());
        
        std::vector<char*> programArgs;
        for(auto &strptr : argStrings) {
          programArgs.push_back(const_cast<char*>(strptr.c_str()));
        }
        programArgs.push_back(nullptr);

        pid_t pid = fork();
        if(pid == 0) execvp(programArgs[0], programArgs.data());
        else if(pid > 0) wait(NULL);
      } else std::cout << argStrings[0] << ": command not found" << std::endl;
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

std::vector<std::string> parseUserArguments(std::string userParamStr) {
  std::vector<std::string> paramVector;
  std::string argExtract = "";
  bool singlequote = false;
  bool doublequote = false;
  std::vector<char> specialChars = {'"', '\\', '$', '`', 'n'};
  
  for (size_t i=0; i<userParamStr.size(); i++) {
    if(userParamStr[i] == '\\') {
      if(doublequote) {
        argExtract+=userParamStr[i++];
        for(char c : specialChars) {
          if(c == userParamStr[i]) {
            argExtract.pop_back();
            argExtract+=userParamStr[i];
            break;
          }
        }
      } else if(singlequote) argExtract+=userParamStr[i]; 
      else argExtract+=userParamStr[++i];
    } else {
      if(userParamStr[i] == '\'' && !doublequote) singlequote = !singlequote;
      else if(userParamStr[i] == '"' && !singlequote) doublequote = !doublequote;
      else if(singlequote) {
        if(userParamStr[i] == '\'') {
          paramVector.push_back(argExtract);
          argExtract.clear();                 // fast for loop
        } else argExtract+=userParamStr[i];
      } else if(doublequote) {
        if(userParamStr[i] == '"') {
          paramVector.push_back(argExtract);
          argExtract.clear();
        } else argExtract+=userParamStr[i];
      } else {
        if(userParamStr[i] == ' ' || userParamStr[i] == '\t') {
          paramVector.push_back(argExtract);
          argExtract = "";
          while(i + 1 < userParamStr.size() && (userParamStr[i + 1] == ' ' || userParamStr[i + 1] == '\t')) i++;
        } else argExtract += userParamStr[i];
      }
    }
  }
  paramVector.push_back(argExtract);
  
  return paramVector;
}


