// Asher Bolleddu and Kiara Vaz

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_COMMANDS 100 // Commands refer to the user input e.g. ls | wc or touch a
#define MAX_ARGS 1024    // Arguments refer to the command itself e.g ls, wc, mkdir, touch, etc.
#define MAX_HISTORY 100;

char *historyArray[100];
int historyCounter = 0;
int check = -1; // Keeps track of if the command being executed is from history array

// Parse input via " "
int parseArgument(char *cmd, char *args[])
{
    int i = 0;
    char *savePtr;
    args[i] = strtok_r(cmd, " ", &savePtr);
    i++;
    while (i < MAX_ARGS && (args[i] = strtok_r(NULL, " ", &savePtr)) != NULL)
    {
        i++;
    }
    args[i] = NULL; // Set the last element to null to be able to use for execvp()

    return i; // Need to return i to get the number of arguments
}

// Parse input via "|"
int parseCommand(char *cmd, char *commands[])
{
    int i = 0;
    char *savePtr;
    commands[i] = strtok_r(cmd, "|", &savePtr);
    i++;
    while (i < MAX_COMMANDS &&
           (commands[i] = strtok_r(NULL, "|", &savePtr)) != NULL)
    {
        i++;
    }
    return i; // Need to return i to get the number of commands
}

void clearHistory()
{
    int i;
    for (i = 0; i < 100; i++)
    { // Loops through array and frees memory
        free(historyArray[i]);
    }
    historyCounter = 0; // Reset history counter
}

void updateHistory(int historyCounter, char *newCommand)
{
    if (historyCounter < 100)
    {                                                      // If history array isnt maxed out
        historyArray[historyCounter] = strdup(newCommand); // Copy into array
    }
    else
    {
        free(historyArray[0]); // Free 0 index
        for (int i = 1; i < 100; i++)
        {
            historyArray[i - 1] = historyArray[i]; // Shift one over
        }
        historyArray[100 - 1] = strdup(newCommand); // Copy into last element
        historyCounter--;                           // Subtract history counter as one index was freed
    }
}

// Function to output the history ()
void showHistory()
{
    for (int i = 0; i < historyCounter; i++)
    { // Loop thru array and print index and command
        printf("%d", i);
        printf(" %s", historyArray[i]);
        printf("\n");
    }
}

// Checks if command is exit, returns a bool depending on whether it is or isn't
bool checkExit(char *args[])
{
    if (strcmp(args[0], "exit") == 0)
    {
        printf("Exiting...\n");
        return true;
    }
    return false;
}

// Executes the command passed in
void executeCommand(char *args[], int inputfd, int outputfd, char *command, int numCommand)
{
    // First check for "cd" and execute that command
    if (strcmp(args[0], "cd") == 0)
    {
        // Error checking
        if (chdir(args[1]) == -1)
        {
            perror("Failed to change directory or directory doesn't exist");
            return;
        }

        if (check != 0)
        {
            updateHistory(historyCounter, command);
            historyCounter++;
        }
    }

    else
    {
        int pid = fork();
        if (pid == -1)
        {
            perror("Failed to create fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0)
        { // Child process
            // Redirect input and output
            if (inputfd != STDIN_FILENO)
            {
                dup2(inputfd, STDIN_FILENO);
                close(inputfd);
            }
            if (outputfd != STDOUT_FILENO)
            {
                dup2(outputfd, STDOUT_FILENO);
                close(outputfd);
            }
            int status = execvp(args[0], args);
            if (status == -1)
            {
                perror("Execution failed"); // execvp failed
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            wait(NULL); // Wait for child process
        }

        if (numCommand < 2)
        { // Check if it's NOT a piped command
            if (check != 0)
            {
                updateHistory(historyCounter, command);
                historyCounter++;
            }
        }
    }
}

void executePipedCommands(char *commands[], int numOfCommands, char *command)
{
    // Initialize file descriptors to appropriate file descriptors (Best way I can say)
    int inputfd = STDIN_FILENO;
    int outputfd = STDOUT_FILENO;
    int fd[2];
    int i = 0;

    // Iterate through the commands
    for (i = 0; i < numOfCommands; i++)
    {
        // If not the last command create a pipe (last command needs to be output to console)
        if (i < numOfCommands - 1)
        {
            if (pipe(fd) == -1)
            {
                perror("Failed to create pipe");
                exit(EXIT_FAILURE);
            }
            outputfd = fd[1]; // Set output to write end of the -pipe
        }
        else
        {
            outputfd = STDOUT_FILENO; // For the last command, output needs to be stdout
        }

        char *args[MAX_ARGS];
        char *originalCommand = commands[i];
        parseArgument(commands[i], args);
        executeCommand(args, inputfd, outputfd, originalCommand, 2);

        // If not the last command, close the write end and set input for the next command
        if (i < numOfCommands - 1)
        {
            close(fd[1]);
            inputfd = fd[0];
        }
    }

    if (check != 0)
    { // If check equals 0 then the command isn't from history offset
        updateHistory(historyCounter, command);
        historyCounter++;
    }
}

int main(int argc, char const *argv[])
{
    // Variables
    char *inputCommand = NULL;
    size_t len = 0;
    char *args[MAX_ARGS];
    char *originalCommand; // Original command before parsing
    int numOfCommands = 0;
    char *ptr; // To check if input is a numerical value

// Shell itself
LOOP:
    while (1)
    {
        check = -1; // Reset check
        printf("sish> ");
        fflush(stdout); // Clear the input buffer

        getline(&inputCommand, &len, stdin);
        inputCommand[strcspn(inputCommand, "\n")] = '\0';
        originalCommand = strdup(inputCommand); // Copy original command
        char *commands[MAX_COMMANDS];

    CHECK:
        numOfCommands = parseCommand(inputCommand, commands); // Parse input by command (|)

        if (numOfCommands == 1)
        {
            int argCount = parseArgument(commands[0], args); // Parse input by arguments (" ")

            if (checkExit(args))
            { // If user inputted exit
                break;
            }
            else if (strcmp(args[0], "history") == 0)
            { // If first elemnt in arg array is history
                if (argCount > 1)
                { // If number of arguments is more than 1
                    if (strcmp(args[1], "-c") == 0)
                    { // If second argument is -c
                        clearHistory();
                        goto LOOP;
                    }

                    strtol(args[1], &ptr, 10); // ptr is a pointer to the first character that couldn't be converted to a number
                    if (*ptr == '\0')
                    {                               // If it is  the null character ('\0'), then the entire string is a valid integer
                        int offset = atoi(args[1]); // Convert to integer
                        if (offset <= historyCounter)
                        { // If offset is less than or equal to history counter
                            // originalCommand = inputCommand;
                            inputCommand = strdup(historyArray[offset]); // Copy comand from history array at offset
                            check = 0;                                   // Set check to 0 because it's the command from the array
                            updateHistory(historyCounter, originalCommand);
                            historyCounter++;
                            originalCommand = inputCommand;
                            goto CHECK;
                        }
                        else
                        {
                            updateHistory(historyCounter, originalCommand); // if history counter is less than offset
                            historyCounter++;
                            printf("Offset is invalid\n");
                            goto LOOP;
                        }
                    }
                }
                else
                { // No arguments
                    if (check != 0)
                    {
                        updateHistory(historyCounter, originalCommand);
                        historyCounter++;
                    }
                    showHistory();
                    goto LOOP;
                }
            }
            executeCommand(args, STDIN_FILENO, STDOUT_FILENO, originalCommand, 0);
        }
        else
        {
            executePipedCommands(commands, numOfCommands, originalCommand);
        }
    }
    free(inputCommand);
    free(originalCommand);
    return 0;
}
