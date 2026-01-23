#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h> //execve
#include <sys/types.h>
#include <sys/wait.h> //wait
#include <fcntl.h> // open
#include <termios.h> // terminal drivers

struct termios mainTermios;

bool findExecFile(std::string, std::string);
std::vector<std::string> parseCmdString(std::string);
std::string tabCompleter(std::string &, std::vector<std::string>);
void disableRawMode();
void enableRawMode();

int main() {
  std::vector<std::string> defaultCmds = {"echo", "type", "exit"};

  while (true) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    std::cout << "$ " << std::flush; // flush to fix memory buffer before enabling raw mode
    std::string cmdLine = "";
    char inputChar;
    
    enableRawMode();
    while(read(STDIN_FILENO, &inputChar, 1) && inputChar != '\n') {
      if(inputChar == '\t') {
        for(size_t i=0; i<cmdLine.size(); i++) std::cout << "\b \b" << std::flush; // handling buffer memory personally coz raw mode

        std::cout << tabCompleter(cmdLine, defaultCmds) << std::flush;
      } else {
        cmdLine+=inputChar;
        std::cout << inputChar << std::flush;
      }
    }
    disableRawMode();
    std::cout << std::flush << std::endl;

    const std::vector<std::string> cmdStrings = parseCmdString(cmdLine); // command parser
    if (cmdStrings[0] == "exit") return 0;

    int redirectIdx = -1, redirectVal = -1;
    std::string targetFile = "";
    for(size_t i=1; i<cmdStrings.size(); i++){
      if(cmdStrings[i] == ">" || cmdStrings[i] == "1>") {
        redirectIdx = i;
        redirectVal = 1;
        break;
      } else if(cmdStrings[i] == "2>") {
        redirectIdx = i;
        redirectVal = 2;
        break;
      } else if(cmdStrings[i] == ">>" || cmdStrings[i] == "1>>") {
        redirectIdx = i;
        redirectVal = 3;
        break;
      } else if(cmdStrings[i] == "2>>") {
        redirectIdx = i;
        redirectVal = 4;
        break;
      }
    }

    if(redirectIdx != -1) {
      if(redirectIdx + 1 < cmdStrings.size()) {
        targetFile = cmdStrings[redirectIdx+1];
      } else continue;

      pid_t redirectPid = fork();
      if(redirectPid < 0) {
        std::cout << "fork failed()" << std::endl;
        _exit(0); // exit(0) may flush praents I/O buffers, good safe exit in child process and signal handlers
      } else if(redirectPid == 0) { // child process
        const char *fileName = targetFile.c_str();

        std::vector<char*> programArgs;
        for(int i=0; i<redirectIdx; i++) {
          programArgs.push_back(const_cast<char*>(cmdStrings[i].c_str()));
        }
        programArgs.push_back(nullptr);

        int fd;
        if(redirectVal == 1 || redirectVal == 2) { // new/append file mode decision
          fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else if(redirectVal == 3 || redirectVal == 4) {
          fd = open(fileName, O_WRONLY | O_CREAT | O_APPEND, 0644);
        }

        if(redirectVal == 1 || redirectVal == 3) { // output/error redirection decision
          if(fd == -1) exit(1);
          if(dup2(fd, STDOUT_FILENO) == -1) exit(1);
          close(fd);
        } else if(redirectVal == 2 || redirectVal == 4) {
          if(fd == -1) exit(1);
          if(dup2(fd, STDERR_FILENO) == -1) exit(1);
          close(fd);
        }

        execvp(programArgs[0], programArgs.data());

      } else {
        wait(NULL);
        continue;
      }
    }


    if(cmdStrings[0] == "echo") {
      for(size_t i=1; i<cmdStrings.size(); i++){
        std::cout << cmdStrings[i] << " ";
      } 
      std::cout << std::endl;

    } else if(cmdStrings[0] == "type") {
      bool foundcmd = false;

      for(int i=0; i<defaultCmds.size(); i++) {
        if(defaultCmds[i] == cmdStrings[1]) {
          std::cout << cmdStrings[1] << " is a shell builtin" << std::endl;
          foundcmd = true;
          break;
        }
      }

      if(!foundcmd) {
        const char* pathVal = std::getenv("PATH");
        std::string Path = "";
        std::istringstream pathParse(pathVal);

        while(!foundcmd && std::getline(pathParse, Path, ':')) {
          foundcmd = findExecFile(Path, cmdStrings[1]);
        }

        if(foundcmd) std::cout << cmdStrings[1] << " is " << Path << "/" << cmdStrings[1] << std::endl;
        else std::cout << cmdStrings[1] << ": not found" << std::endl;
      }

    } else {
      bool foundcmd = false;
      const char* pathVal = std::getenv("PATH");
      std::string Path = "";
      std::istringstream pathParse(pathVal);

      while(!foundcmd && std::getline(pathParse, Path, ':')) {
        foundcmd = findExecFile(Path, cmdStrings[0]);
      }
      
      if(foundcmd) {
        std::vector<char*> programArgs;
        for(auto &strptr : cmdStrings) {
          programArgs.push_back(const_cast<char*>(strptr.c_str()));
        }
        programArgs.push_back(nullptr);

        pid_t pid = fork();
        if(pid == 0) execvp(programArgs[0], programArgs.data());
        else if(pid > 0) wait(NULL);
      } else std::cout << cmdStrings[0] << ": command not found" << std::endl;
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

std::vector<std::string> parseCmdString(std::string userParamStr) {
  std::vector<std::string> paramVector;
  std::string argExtract = "";
  bool singlequote = false;
  bool doublequote = false;
  std::vector<char> specialChars = {'"', '\\', '$', '`', 'n'};
  
  for (size_t i=0; i<userParamStr.size(); i++) {
    if(userParamStr[i] == '\\') {
      if(doublequote) {
        argExtract+=userParamStr[i]; 
        if(i+1 < userParamStr.size()) {
          for(char c : specialChars) {
            if(c == userParamStr[i+1]) {
              argExtract.pop_back();
              argExtract+=userParamStr[++i];
              break;
            }
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
          argExtract.clear();                 // fast for loop instead of calling assignment
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

std::string tabCompleter(std::string &incompleteString, std::vector<std::string> inbuiltCmd) {
  for(const auto &str : inbuiltCmd) {
    if(str.find(incompleteString) == 0) {
      incompleteString = (str+" ");
      return incompleteString;
    }
  }
  return incompleteString;
}

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &mainTermios); // reassign standard output to main terminal 
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &mainTermios); // save main terminal state
  atexit(disableRawMode); // auto call to disable raw mode to fix terminal if program ends or crashes without disabling raw mode, a safe switch

  struct termios raw = mainTermios; // copy main terminal state for changing and passing
  raw.c_lflag &= ~(ICANON | ECHO); // disable buffer until \n and disable auto print everycharacter on terminal;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // set output to changed terminal state only after pending output has transmitted and discard unread input
}

