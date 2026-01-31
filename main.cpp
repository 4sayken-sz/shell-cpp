#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <unistd.h> //execve
#include <sys/types.h>
#include <sys/wait.h> //wait
#include <fcntl.h> // open
#include <termios.h> // terminal drivers
#include <fstream>

bool isExecFile(std::string, std::string);
bool isitExecFile(std::string);
std::vector<std::string> parseCmdString(std::string);
std::string tabCompleter(std::string &, std::vector<std::string>);
void disableRawMode();
void enableRawMode();
bool isBuiltinCommand(std::string, std::vector<std::string> &);
void execBuiltin(std::string, std::vector<char*> &, std::vector<std::string> &, std::vector<std::string> &, std::string &);
void redirect(int (&)[3],std::string &);
void displaycmdHistory(int, std::vector<std::string> &);
void loadHistory(const std::string, std::vector<std::string> &);
void saveHistory(const std::string, std::string, std::vector<std::string> &);
bool isaNumber(std::string);
void loadHistoryOnStartup(std::vector<std::string> &);
void saveHistoryOnExit(std::vector<std::string> &historyvec);

struct termios mainTermios;

int main() {
  std::string currentPath = std::filesystem::current_path();
  std::vector<std::string> defaultCmds = {"echo", "type", "history", "exit", "pwd", "cd"};
  std::vector<std::string> commandHistory;
  loadHistoryOnStartup(commandHistory);

  while (true) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    char inputChar;
    std::string cmdLine = "";
    std::cout << "$ " << std::flush; // flush to clear memory buffer before enabling raw mode, clears output issue later
    
    enableRawMode();
    const int minHistory = 0, maxHistory = commandHistory.size();
    int currHistory = maxHistory;
    while(read(STDIN_FILENO, &inputChar, 1) && inputChar != '\n') {
      if(inputChar == 27) {
        char seq[2];
        if(read(STDIN_FILENO, &seq[0], 1) && read(STDIN_FILENO, &seq[1], 1)) {
          if(seq[0] == '['  && !commandHistory.empty()) {
            if(seq[1] == 'A') {
              for(size_t i=0; i<cmdLine.size(); i++) std::cout << "\b \b" << std::flush;
              if(--currHistory < minHistory) currHistory = 0;
              cmdLine = commandHistory[currHistory];
              std::cout << cmdLine << std::flush;
            } else if(seq[1] == 'B') {
              for(size_t i=0; i<cmdLine.size(); i++) std::cout << "\b \b" << std::flush;
              if(++currHistory > maxHistory-1) currHistory = maxHistory-1;
              cmdLine = commandHistory[currHistory];
              std::cout << cmdLine << std::flush;
            }
          }
        }
        continue;
      }
      currHistory = maxHistory;
      if(inputChar == '\t') {
        for(size_t i=0; i<cmdLine.size(); i++) std::cout << "\b \b" << std::flush; // handling buffer memory personally coz raw mode
        tabCompleter(cmdLine, defaultCmds);
        if(cmdLine.back() == '\n') {
          cmdLine.pop_back();
          std::cout << cmdLine << std::flush;
          break;
        } else std::cout << cmdLine << std::flush;
      } else if(inputChar == 127) {
        if(!cmdLine.empty()) {
          cmdLine.pop_back();
          std::cout << "\b \b" << std::flush;
        }
      } else {
        cmdLine+=inputChar;
        std::cout << inputChar << std::flush;
      }
    }
    std::cout << std::endl;
    disableRawMode();

    const std::vector<std::string> withPipeStrings = parseCmdString(cmdLine); // command parser
    std::vector<std::vector<std::string>> cmdStrings;
    
    std::vector<std::string> cmdL;
    for(auto &str : withPipeStrings) {
      if(str == "|") {
        cmdStrings.push_back(cmdL);
        cmdL.clear();
      } else cmdL.push_back(str);
    }
    if (!cmdL.empty()) cmdStrings.push_back(cmdL);
    commandHistory.push_back(cmdLine);

    if(cmdStrings.size() < 1) continue;
    if (cmdStrings[0][0] == "exit") break;

    const int original_inputfd = dup(STDIN_FILENO);
    const int original_outputfd = dup(STDOUT_FILENO);
    int prev_pipeIn = dup(STDIN_FILENO);

    for(size_t i=0; i<cmdStrings.size(); i++) { // main pipelining loop
      int newpipefds[2];
      bool isLast = (i == cmdStrings.size()-1);
      if(!isLast) {
        if(pipe(newpipefds) == -1) {
          std::cerr << "failed pipe" << std::endl;
          break;
        } // 0-read, 1-write
      }

      std::vector<char*> currentProgArgs;
      const char* outFile = nullptr;
      int redirvalues[3] = {0}; // loc 0 for redircheck(1=T/0=F), loc 1 for new/append file, loc 2 for output/error redirection
      
      for(size_t j=0; j<cmdStrings[i].size(); j++) { // redirector decider - argument parser
        std::string str = cmdStrings[i][j];
        if( str.back() == '>') {
          if(j+1 < cmdStrings[i].size()) {
            redirect(redirvalues, str);
            if(redirvalues[0] != 0) {
              outFile = (cmdStrings[i][j+1]).c_str();
              break;
            }
          } else {
            std::cerr << "redirection failed : no file specified" << std::endl;
            break;
          }
        } else currentProgArgs.push_back(const_cast<char*>(cmdStrings[i][j].c_str()));
      }
      currentProgArgs.push_back(nullptr);
      
      if(isBuiltinCommand(currentProgArgs[0], defaultCmds)) {
        if(dup2(prev_pipeIn, STDIN_FILENO) == -1) {
          std::cerr << "pipe command execution 1 failed for builting" << std::endl;
          close(prev_pipeIn);
          close(newpipefds[0]);
          close(newpipefds[1]);
          break;
        }
        
        if(!isLast && dup2(newpipefds[1], STDOUT_FILENO) == -1) {
          std::cerr << "pipe command execution 2 failed for builting" << std::endl;
          close(prev_pipeIn);
          close(newpipefds[0]);
          close(newpipefds[1]);
          break;
        }
        
        if(redirvalues[0] == 1 && outFile != nullptr) {
          int newfd;
          const int stdout_fd = dup(STDOUT_FILENO);
          const int stderr_fd = dup(STDERR_FILENO);
          if(redirvalues[1] == 1) newfd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          else if(redirvalues[1] == 2) newfd = open(outFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
          if(newfd == -1) {
            std::cerr << "couldn't open specified file" << std::endl;
            continue;
          }
          
          if(redirvalues[2] == 1) {
            if(dup2(newfd, STDOUT_FILENO) == -1) {
              std::cerr << "output redirect failed" << std::endl;
              continue;
            }
            close(newfd);
          } else if(redirvalues[2] == 2) {
            if(dup2(newfd, STDERR_FILENO) == -1) {
              std::cerr << "error redirect failed" << std::endl;
              continue;
            }
            close(newfd);
          }
          
          execBuiltin(currentProgArgs[0], currentProgArgs, defaultCmds, commandHistory, currentPath);
          
          dup2(stdout_fd, STDOUT_FILENO);
          dup2(stderr_fd, STDERR_FILENO);
          close(stdout_fd);
          close(stderr_fd);
        } else {
          execBuiltin(currentProgArgs[0], currentProgArgs, defaultCmds, commandHistory, currentPath);
        }

        dup2(original_inputfd, STDIN_FILENO);
        dup2(original_outputfd, STDOUT_FILENO);
        close(prev_pipeIn);
        
        if(!isLast) {
          prev_pipeIn = newpipefds[0];
          close(newpipefds[1]);
        }
        
      } else if(isitExecFile(currentProgArgs[0])) {
        pid_t pid = fork();
        
        if(pid == 0) { // child
          if(dup2(prev_pipeIn, STDIN_FILENO) == -1) {
            std::cerr << "pipe command execution 1 failed for exec" << std::endl;
            exit(1);
          }

          if(!isLast && (dup2(newpipefds[1], STDOUT_FILENO) == -1)) {
            std::cerr << "pipe command execution 2 failed for exec" << std::endl;
            exit(1);
          }

          close(prev_pipeIn);
          if(!isLast) {
            close(newpipefds[0]);
            close(newpipefds[1]);
          }

          if(redirvalues[0] == 1 && outFile != nullptr) {
            int fd;
            if(redirvalues[1] == 1) fd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            else if(redirvalues[1] == 2) fd = open(outFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if(fd == -1) {
              std::cerr << "couldn't open specified file" << std::endl;
              exit(1);
            }
  
            if(redirvalues[2] == 1) {
              if(dup2(fd, STDOUT_FILENO) == -1) {
                std::cerr << "output redirect failed" << std::endl;
                exit(1);
              }
              close(fd);
            } else if(redirvalues[2] == 2) {
              if(dup2(fd, STDERR_FILENO) == -1) {
                std::cerr << "error redirect failed" << std::endl;
                exit(1);
              }
              close(fd);
            }
          }

          execvp(currentProgArgs[0], currentProgArgs.data());
          std::cerr << "Command exec failed" << std::endl;
          exit(1);
        } else { // parent
          close(prev_pipeIn);
          if(!isLast) {
            prev_pipeIn = newpipefds[0];
            close(newpipefds[1]);
          }
        }
      } else std::cout << currentProgArgs[0] << ": command not found" << std::endl;
    }

    // to be sure
    dup2(original_inputfd, STDIN_FILENO);
    close(original_inputfd);
    dup2(original_outputfd, STDOUT_FILENO);
    close(original_outputfd);

    for(size_t i=0; i<cmdStrings.size(); i++) wait(NULL);
  }

  saveHistoryOnExit(commandHistory);

  return 0;
}


bool isExecFile(std::string envPath, std::string cmdName) {
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

bool isitExecFile(std::string cmdName) {
  bool foundcmd = false;
  const char* pathVal = std::getenv("PATH");
  std::istringstream pathParse(pathVal);
  std::string currentPath = "";
  if(pathVal != nullptr) {
    while(std::getline(pathParse, currentPath, ':') && !foundcmd) {
      std::filesystem::path currentFileSysPath = currentPath;

      if(std::filesystem::exists(currentFileSysPath) && std::filesystem::is_directory(currentFileSysPath)) {
        for(const auto &file : std::filesystem::directory_iterator(currentFileSysPath)) {
          if(file.path().filename() == cmdName) {
            std::filesystem::file_status fileStatus = std::filesystem::status(file.path());
            std::filesystem::perms filePermissions = fileStatus.permissions();

            if((filePermissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
              foundcmd = true;
              break;
            }
          }
        }
      } else std::cout << currentPath << " : directory doesn't exists" << std::endl;
    }
  } else std::cout << "not found path variables" << std::endl;

  return foundcmd;
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
  if(argExtract != "" && !argExtract.empty()) paramVector.push_back(argExtract);
  
  return paramVector;
}

std::string tabCompleter(std::string &incompleteString, std::vector<std::string> inbuiltCmd) {
  for(const auto &str : inbuiltCmd) { // builtin completer
    if(str.find(incompleteString) == 0) {
      incompleteString = (str+" ");
      return incompleteString;
    }
  }

  // exec finder and completer
  const char* pathVal = std::getenv("PATH");
  std::string Path = "";
  std::istringstream pathParse(pathVal); 
  std::vector<std::string> matchedFiles;

  while(std::getline(pathParse, Path, ':')) { // collect execs of paths
    std::filesystem::path currentFileSysPath = Path;

    if(std::filesystem::exists(currentFileSysPath) && std::filesystem::is_directory(currentFileSysPath)) {
      for(const std::filesystem::directory_entry file : std::filesystem::directory_iterator(currentFileSysPath)) {
        std::string f = file.path().filename().c_str();
        if(f.find(incompleteString) == 0) {
          std::filesystem::file_status fileStatus = std::filesystem::status(file.path());
          std::filesystem::perms filePermissions = fileStatus.permissions();

          if((filePermissions & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) {
            matchedFiles.push_back(f);
          }
        }
      }
    }
  }

  if(matchedFiles.size() == 1) {
    incompleteString=matchedFiles[0]+" ";
    return incompleteString;
  } else if(matchedFiles.size() > 1) {
    std::sort(matchedFiles.begin(), matchedFiles.end());

    std::string firstFile = matchedFiles[0];
    std::string lastFile = matchedFiles[matchedFiles.size()-1];

    // longest common prefix
    std::string lprefix = "";
    for(size_t i=0; i<firstFile.size(); i++) {
      if(firstFile[i] != lastFile[i]) break;
      lprefix+=firstFile[i];
    }    

    incompleteString=lprefix;
    std::cout << incompleteString << '\a' << std::flush;

    char nextChar;
    if(read(STDIN_FILENO, &nextChar, 1) && nextChar == '\t') {
      std::cout << std::endl;
      for(std::string &matchedfile : matchedFiles) {
        std::cout << matchedfile << "  " ;
      }
      std::cout << std::endl << "$ ";
    } else {
      for(size_t i=0; i<incompleteString.size(); i++) std::cout << "\b \b" << std::flush;
      incompleteString+=nextChar;
      return incompleteString;
    }
  } else std::cout << '\a' << std::flush; // \a is bell character, beep sound

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

bool isBuiltinCommand(std::string cmdstr, std::vector<std::string> &cmdbuiltin) {
  for(int i=0; i<cmdbuiltin.size(); i++) {
    if(cmdbuiltin[i] == cmdstr) {
      return true;
    }
  }

  return false;
}

void execBuiltin(std::string inbuiltcmd, std::vector<char*> &progArgArray, std::vector<std::string> &defbuiltins, std::vector<std::string> &cmdHisttoryvec, std::string &pwdPath) {
  if(inbuiltcmd == "echo") {
    for(size_t i=1; i<progArgArray.size() && progArgArray[i] != nullptr; i++) {
      std::cout << progArgArray[i];
      if(i + 1 < progArgArray.size() && progArgArray[i+1] != nullptr) std::cout << " ";
    } 
    std::cout << std::endl;

  } else if(inbuiltcmd == "type") {
    bool foundcmd = false;

    foundcmd = isBuiltinCommand(progArgArray[1], defbuiltins);
    if(foundcmd) std::cout << progArgArray[1] << " is a shell builtin" << std::endl;

    if(!foundcmd) {
      const char* pathVal = std::getenv("PATH");
      std::string Path = "";
      std::istringstream pathParse(pathVal);

      while(!foundcmd && std::getline(pathParse, Path, ':')) {
        foundcmd = isExecFile(Path, progArgArray[1]);
      }

      if(foundcmd) std::cout << progArgArray[1] << " is " << Path << "/" << progArgArray[1] << std::endl;
      else std::cout << progArgArray[1] << ": not found" << std::endl;
    }
  } else if(inbuiltcmd == "history") {
    int size = cmdHisttoryvec.size();
    if(progArgArray.size() == 2) {
      displaycmdHistory(size, cmdHisttoryvec);
    } else if(progArgArray.size() > 2) {
      std::string secondArg = progArgArray[1];
      if(isaNumber(secondArg)) {
        size = atoi(progArgArray[1]);
        displaycmdHistory(size, cmdHisttoryvec);
      } else if(secondArg == "-r") {
        loadHistory(progArgArray[2], cmdHisttoryvec);
      } else if(secondArg == "-w" || secondArg == "-a"){
        saveHistory(progArgArray[2], secondArg, cmdHisttoryvec);
        cmdHisttoryvec.clear();
      } else {
        std::cout << "history : wrong arguments" << std::endl;
      }
    }
  } else if(inbuiltcmd == "pwd") {
    std::cout << pwdPath << std::endl;
  } else if(inbuiltcmd == "cd") {
    if(progArgArray[1] != nullptr) {
      std::string path = progArgArray[1];
      if(path == "~") {
        const char* temp = std::getenv("HOME");
        if(temp == NULL) std::cerr << "cd: cant find directory HOME" << std::endl;
        else {
          std::filesystem::current_path(temp);
          pwdPath = std::filesystem::current_path();
        }
      } else {
        if(std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
          std::filesystem::current_path(path);
          pwdPath = std::filesystem::current_path();
        }
        else std::cout << "cd: " << path << ": No such file or directory" << std::endl;
      }
    } 
  }
}

void redirect(int (&arr)[3],std::string &str) {
  // arr[0] = 0-f / 1-t
  // arr[1] = 1-new / 2-append file
  // arr[2] = 1-output / 2-error redirection
  arr[0] = 1;
  if(str == ">" || str == "1>") {
    arr[1] = 1;
    arr[2] = 1;
  } else if(str == "2>") {
    arr[1] = 1;
    arr[2] = 2;
  } else if(str == ">>" || str == "1>>") {
    arr[1] = 2;
    arr[2] = 1;
  } else if(str == "2>>") {
    arr[1] = 2;
    arr[2] = 2;
  } else arr[0] = 0;
}

void displaycmdHistory(int n_limit, std::vector<std::string> &historyvec) {
  int displayLimit = historyvec.size() - n_limit;
  if (displayLimit < 0) displayLimit = 0; 
  for(int i=displayLimit; i<historyvec.size(); i++) {
      std::cout << i << " " << historyvec[i] << std::endl;
  }
}

void loadHistory(const std::string path, std::vector<std::string> &historyvec) {
   std::ifstream histIn(path, std::ios::in);

  if(!histIn.is_open()) {
    std::cerr << "couldnt load history" << std::endl;
    return;
  }

  std::string line;
  while(std::getline(histIn, line)) {
    historyvec.push_back(line);
  }

  return;
}

void saveHistory(const std::string path, std::string mode, std::vector<std::string> &historyvec) {
  std::ofstream histOut;
  if(mode == "-w") histOut.open(path, std::ios::out);
  else if(mode == "-a") histOut.open(path, std::ios::app);
  else if(mode == "-e") histOut.open(path, std::ios::out); // for other modes in future for exit

  if(!histOut.is_open()) {
    std::cerr << "couldnt save history" << std::endl;
    return;
  }

  for(auto &cmd : historyvec) {
    histOut << cmd << std::endl;
  }

  histOut.close();
}

bool isaNumber(std::string str) {
  for(size_t i=0; i<str.size(); i++) {
    if(!isdigit(str[i])) return false;
  }

  return true;
}

void loadHistoryOnStartup(std::vector<std::string> &historyvec) {
  const char* pathVal = std::getenv("HISTFILE");
  if(pathVal == NULL) return;
  loadHistory(pathVal, historyvec);
  return;
}

void saveHistoryOnExit(std::vector<std::string> &historyvec) {
  const char* pathVal = std::getenv("HISTFILE");
  if(pathVal == NULL) return;
  saveHistory(pathVal, "-e", historyvec);
  return;
}


//