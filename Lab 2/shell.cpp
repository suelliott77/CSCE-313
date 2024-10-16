/****************
LE2: Introduction to Unnamed Pipes
****************/
#include <unistd.h> // pipe, fork, dup2, execvp, close
using namespace std;
// ?pipe
// ?fork 
// ?dup2
// ?fds
// ?execvp

int main () {
    // lists all the files in the root directory in the long format
    char* cmd1[] = {(char*) "ls", (char*) "-al", (char*) "/", nullptr};
    // translates all input from lowercase to uppercase
    char* cmd2[] = {(char*) "tr", (char*) "a-z", (char*) "A-Z", nullptr};

    // TODO: add functionality
    // Create pipe
    int fds[2];
    pipe(fds);

    // Create child to run first command
    int pipe = fork();
    if (pipe == 0) {
        // In child, redirect output to write end of pipe
        dup2(fds[1],1); 
        // Close the read end of the pipe on the child side.
        close(fds[0]);
        // In child, execute the command
        execvp(cmd1[0], cmd1);
    }

    // Create another child to run second command
    int pipe2 = fork();
    if (pipe2 == 0) {
        // In child, redirect input to the read end of the pipe
        dup2(fds[0],0);
        // Close the write end of the pipe on the child side.
        close(fds[1]);
        // Execute the second command.
        execvp(cmd2[0], cmd2);
    }

    // Reset the input and output file descriptors of the parent.
}
