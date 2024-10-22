#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <vector>
#include <string>
#include <ctime>
#include <signal.h>
#include <algorithm>
#include "Tokenizer.h"
#include "Command.h"

// Color codes for prompt
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define WHITE   "\033[1;37m"
#define NC      "\033[0m"
#define PATH_MAX 500

using namespace std;

// Signal handler to clean up zombie background processes
void function_wait(int signo) {
    (void)signo;  // Avoid warnings for unused parameter
    while (waitpid(-1, nullptr, WNOHANG) > 0) {
        // Reap any finished child processes.
    }
}

// Print the prompt with time, user, and current directory
void print_prompt() {
    time_t cur_time = time(0);
    struct tm timeinfo;
    localtime_r(&cur_time, &timeinfo);
    char buf1[100];
    strftime(buf1, sizeof(buf1), "%b %d %H:%M:%S", &timeinfo);
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    std::string username = getenv("USER");
    std::cout << YELLOW << buf1 << " " << username << ":" << cwd << "$ " << NC;
}

// Function to process commands (foreground and background handling included)
void process_commands(Tokenizer& tknr) {
    int backwards_fds[2] = {-1, -1};
    int forwards_fds[2] = {-1, -1};

    for (size_t i = 0; i < tknr.commands.size(); i++) {
        auto cmd = tknr.commands[i];

        // Handle 'Background' at the start of the command
        bool is_background = false;
        if (cmd->args[0] == "Background") {
            is_background = true;
            cmd->args.erase(cmd->args.begin());  // Remove "Background" from the command
        }

        // Handle 'cd' built-in command
        if (cmd->args[0] == "cd") {
            if (cmd->args.size() > 1) {
                setenv("OLDPWD", getenv("PWD"), 1); 
                if (chdir(cmd->args[1].c_str()) == 0) {
                    setenv("PWD", cmd->args[1].c_str(), 1); 
                } else {
                    perror("cd error");
                }
            } else {
                setenv("OLDPWD", getenv("PWD"), 1);
                chdir(getenv("HOME"));
                setenv("PWD", getenv("HOME"), 1); 
            }
            continue;
        }

        // Setup pipes for inter-command communication
        if (i < tknr.commands.size() - 1) {
            if (pipe(forwards_fds) < 0) {
                perror("pipe failed");
                exit(1);
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork error");
            exit(2);
        }

        if (pid == 0) {  // Child process
            if (i < tknr.commands.size() - 1) {
                dup2(forwards_fds[1], STDOUT_FILENO);  // Redirect STDOUT to pipe
                close(forwards_fds[1]);
                close(forwards_fds[0]);
            }

            if (i > 0) {
                dup2(backwards_fds[0], STDIN_FILENO);  // Redirect STDIN from previous pipe
                close(backwards_fds[0]);
                close(backwards_fds[1]);
            }

            // Handle input redirection
            if (cmd->hasInput()) {
                int input = open(cmd->in_file.c_str(), O_RDONLY);
                if (input < 0) {
                    perror("redirection failed");
                    exit(1);
                }
                dup2(input, STDIN_FILENO);
                close(input);
            }

            // Handle output redirection
            if (cmd->hasOutput()) {
                int output = open(cmd->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output < 0) {
                    perror("redirection failed");
                    exit(1);
                }
                dup2(output, STDOUT_FILENO);
                close(output);
            }

            // Prepare command arguments for execvp
            vector<char*> args;
            for (auto& arg : cmd->args) {
                args.push_back(const_cast<char*>(arg.c_str()));
            }
            args.push_back(nullptr);

            execvp(args[0], args.data());
            perror("execvp failed");
            exit(2);

        } else {  // Parent process
            if (tknr.commands.size() > 1) {
                close(forwards_fds[1]);  // Close unused pipe ends
            }

            if (backwards_fds[0] != -1) {
                close(backwards_fds[0]);
            }

            if (i < tknr.commands.size() - 1) {
                backwards_fds[0] = forwards_fds[0];
                backwards_fds[1] = -1;
            }

            // Handle both background '&' and "Background" prefix
            if (is_background || cmd->args.back() == "&") {
                if (cmd->args.back() == "&") {
                    cmd->args.pop_back();  // Remove '&' from arguments
                }
                setpgid(pid, pid);  // Detach background process group

                // Do NOT wait for background processes to finish
                std::cout << GREEN << "[Background process started with PID: " << pid << "]" << NC << std::endl;
            } else {
                // Foreground process: wait for the child to finish
                int status;
                waitpid(pid, &status, 0);  // Only wait for foreground tasks

                if (status > 1) {
                    exit(status);
                }
            }
        }
    }
}

int main() {
    const char* cur_dir = getenv("PWD");
    setenv("OLDPWD", cur_dir, 1);

    // Setup signal handler to handle child process cleanup
    signal(SIGCHLD, function_wait);

    for (;;) {
        print_prompt();
        string input;
        getline(cin, input);

        // Convert input to lowercase for easy 'exit' comparison
        string lower_input = input;
        transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);

        // Exit shell on 'exit' command
        if (lower_input == "exit") {
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // Skip empty input
        if (input.empty()) {
            continue;
        }

        // Tokenize input and process commands
        Tokenizer tknr(input);
        process_commands(tknr);
    }

    return 0;
}
