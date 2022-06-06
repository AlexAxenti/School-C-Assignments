#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 80 // Maximum length command as given in the shell outline in the assignment
#define HISTORY_STRING "history"
#define EXIT_STRING "exit"

int main() {

    int should_run = 1; // Boolean that determines whether the shell is in use
    char history[5][MAX_LINE]={""}; // Input history, array of strings that contains the last 5 inputs (default value set to "")
    int current_command = 0;
    int run_concurrently = 0; // Determines if parent and child processes run concurrently, based off "&" in input

    while (should_run) {
        printf("osh> ");
        fflush(stdout);
        fflush(stdin);

        // Gets input from the user
        char input[MAX_LINE];
        scanf(" %[^\n]", input); 
        
        if(strcmp(input, EXIT_STRING) == 0){ // If the user enters exit, the while loop ends and the shell closes
            should_run = 0;
        } else if (strcmp(input, HISTORY_STRING) == 0) { // If the user enters history, print the elements in the history array
            for (int i = 0; i < 5; i++){
                if(strcmp(history[i],"") != 0) { // default value means no history for that index, so it's not printed
                    int cmd_number = current_command - i;
                    printf("%d %s \n", cmd_number, history[i]);
                }
            }
        } else { // User enters a command
            if (strcmp(input, "!!")==0){ // History feature: most recent command
                if (strcmp(history[0], "") == 0) { // If the first index is set to default value, then no inputs have been entered
                    printf("No commands in history\n");
                    continue;
                }
                strcpy(input, history[0]); // Set the input variable that is used below to the most recent command entered
                printf("%s\n", input);
            } else if (input[0] == '!' && input[1] == ' '){  // History feature: specific command number
                // Convert the substring following the "! " into an integer
                int input_length = strlen(input);
                char cmd_index_string[input_length - 2];
                strncpy(cmd_index_string, &input[2], input_length - 2);
                int cmd_index = (int)strtol(cmd_index_string, (char **)NULL, 10);

                if (cmd_index > current_command || cmd_index < 1) { // Check that the specified command exists
                    printf("No such command in history\n");
                    continue;
                }

                // Get the specific command from the history array
                cmd_index = current_command - cmd_index;
                strcpy(input, history[cmd_index]);
                printf("%s\n", input);
            }

            // Shift the values in the history array, which removes the value at history[4] and adds the most recent command into history[0]
            for (int i = 3; i >= 0; i--) {
                strcpy(history[i+1], history[i]);
            }
            strcpy(history[0], input);
            current_command++;
            
            // Determines how many spaces there are in the input
            int arrayLength = 1;
            char delim = ' ';
            for (int i = 0; input[i]; i++){
                if(input[i] == delim) {
                    arrayLength++;
                }
            }
            // Create an arguments array, that will contain each individual word in the input
            char *arguments[arrayLength + 1];

            // Split the string with spaces as a delimiter, and input each word into the arguments array
            char *s = strtok(input, " "); 
            int i = 0;
            while (s != NULL) {
                arguments[i++] = s;
                s = strtok(NULL, " ");
            }
            arguments[arrayLength] = NULL;

            run_concurrently = 0;
            // If the last word in the inputted command is "&" then set run_concurrently to true, and remove "&" from the arguments array
            if(strcmp(arguments[arrayLength - 1], "&") == 0) {
                run_concurrently = 1;
                arguments[arrayLength - 1] = NULL;
            }

            // Create a child process to execute the command
            pid_t pid;
            pid = fork();

            if (pid == 0) {
                execvp(arguments[0], arguments); // Pass the arguments to execvp to execute the shell command
            } else {
                if (run_concurrently == 0) { // If & was not inputted, then wait for child process to finish.
                    wait(NULL);
                }
            }
        }       
    }

    return 0;
}