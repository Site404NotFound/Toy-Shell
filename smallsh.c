/*********************************************************************
** Author: James Hippler (HipplerJ)
** Oregon State University
** CS 344-400 (Spring 2018)
** Operating Systems I
**
** Description: Program 3 - smallsh (Block 3)
** Due: Sunday, May 27, 2018
**
** Filename: smallsh.c
**
** Objectives:
** In this assignment you will write your own shell in C, similar to
** bash. No other languages, including C++, are allowed, though you
** may use any version of C you like, such as C99. The shell will run
** command line instructions and return the results similar to other
** shells you have used, but without many of their fancier features.
**
** In this assignment you will write your own shell, called smallsh.
** This will work like the bash shell you are used to using, prompting
** for a command line and running commands, but it will not have many
** of the special features of the bash shell.
**
** Your shell will allow for the redirection of standard user_command and
** standard output and it will support both foreground and background
** processes (controllable by the command line and by receiving signals).
**
** Your shell will support three built in commands: exit , cd , and status.
** It will also support comments, which are lines beginning with the #
** character.
**
** EXTERNAL RESOURCES
** - http://www.cplusplus.com/reference/cstring/strtok/
** - http://www.cplusplus.com/reference/cstring/strstr/
** - https://stackoverflow.com/questions/298510/how-to-get-the-current-
**   directory-in-a-c-program?utm_medium=organic&utm_source=google_rich_
**   qa&utm_campaign=google_rich_qa
**
** NOTES
** There are comments for each line of code.  I use a package in VIM
** that uniformly justifies them off to the right so that they're not
** cluttering the code space.  Also there are Comment blocks for each
** function.
*********************************************************************/

/************************************************************************
** PREPROCESSOR DIRECTIVES
*************************************************************************/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdbool.h>

/************************************************************************
** GLOBAL CONSTANT DECLARATIONS
*************************************************************************/

#define MAX_COMMAND 2048                                                        // Define global constant for maximum command string length
#define MAX_ARGUMENTS 512                                                       // Define global constant for maximum number of arguments
#define MAX_PROCESSES 1000                                                      // Define global constant for the maximum number of users processes
#define HOME "HOME"                                                             // Define global constant for the Home path environment

/************************************************************************
** FUNCTION DECLARATIONS
*************************************************************************/

void catchSIGINT(int signo);
void catchSIGTSTP();
void clear_arrays();
void running();
void prompt_user();
void removeBreakLine();
void parse_command();
void route_command();
void fork_pid();
void check_background();
void check_redirection();
void expand_pid();
void check_expansion();
void change_directory();
void exit_program();
void status();
void execute();
void remove_redirection();
void flushIO();

/************************************************************************
** GLOBAL VARIABLE DECLARATIONS
** Believe me when I say I hate myself for using global variables.
*************************************************************************/

int   code = 0,                                                                 // Integer variable for storing the status code
      background[MAX_PROCESSES],                                                // Integer array for storing background process PIDs
      background_counter = 0,                                                   // Integer variable for storing the number of background processes
      argument_total = 0,                                                       // Integer variable for storing the total number of arguments
      redirection_in_location = 0,                                              // Integer variable for storing the location of < character
      redirection_out_location = 0,                                             // Integer variable for storing the location of > character
      termination = 0,                                                          // Integer variable for storing the value of the termination signal
      child_exit = -5,                                                          // Integer variable for storing the value of the child exit value
      expansion_location = 0;                                                   // Integer variable for storing the location of & characters

char* arguments[MAX_ARGUMENTS] = { 0 };                                         // String Array variable for storing the segmented commands

bool  die = false,                                                              // Boolean variable to determine if the program should be ended
      redirection_in = false,                                                   // Boolean variable to determine if stdin redirection occurred
      redirection_out = false,                                                  // Boolean variable to determine if stdout redirection occurred
      run_background = false,                                                   // Boolean variable to determine if process should be executed in the background
      foreground_mode = false;                                                  // Boolean variable to determine if foreground-only mode is enabled

char user_command[MAX_COMMAND];                                                 // Character array for storing the initial user command

pid_t child_pid = -5;                                                           // PID Variable for storing child background processes as they are created

/************************************************************************
* Description: main function
* This is the main function where the program begins and ends.  This
* function orchestrates the functions and process of the program. My main
* function allows users to input additional arguments when executing the
* program.  They are ignored and never utilized.
*************************************************************************/

int main(int argc, char const *argv[]) {
	struct sigaction SIGINT_action = {{ 0 }};		                                  //Establish and intialize SIGINT Structure (Ctrl+C)
	SIGINT_action.sa_handler = catchSIGINT;		                                    // Create SIGINT Handler
	sigfillset(&SIGINT_action.sa_mask);                                           // Create SIGINT sigfillset
	sigaction(SIGINT, &SIGINT_action, NULL);	                                    // Create SIGINT sigaction

  struct sigaction SIGTSTP_action = {{ 0 }};                                    // Establish and initialize SIGSTP Structure (Ctrl+Z)
	SIGTSTP_action.sa_handler = catchSIGTSTP;	                                    // Create SIGTSTP Handler
	sigfillset(&SIGTSTP_action.sa_mask);                                          // Create SIGTSTP sigfillset
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);                                    // Create SIGTSTP sigaction

	do {                                                                          // Start a loop in main
    flushIO();                                                                  // Call function to clear stdio buffers
    clear_arrays();                                                             // Call function to initialize the variables necessary for storing user commands
		running();						                                                      // Call function to check if background process are running
    prompt_user();                                                              // Call function to display prompt and capture user user_command
    parse_command();                                                            // Call function to segment user command into individual arguments
    route_command();							                                              // Call function to determine how the user's command is handled
	} while (die == false);                                                       // Continue looping until exit is typed as the command
}

/************************************************************************
* Description: catchSIGINT function
* Function is designed to capture Keyboard Interrupt signals from the
* user (Ctrl+C).  Kills all running processeses, but does not exit the
* smallsh program.
*************************************************************************/

void catchSIGINT(int sig) {
	printf("terminated by signal %d\n", sig);                                     // Let user know that everything was terminated by signal
	flushIO();                                                                    // Call function to clear stdio buffers
}

/************************************************************************
* Description: catchSIGTSTP function
* Function is designed to capture the TSTP Signal from the user
* (Ctrl + Z).  Function toggles foreground-only mode and uses several
* boolean flags.
*************************************************************************/

void catchSIGTSTP() {
	if (foreground_mode == false) {                                               // If foreground only mode is not already enabled
		printf("Entering foreground-only mode (& is now ignored)\n");               // Inform the user that we are operating in foreground-only mode
		flushIO();                                                                  // Call function to flush stdio buffers
    run_background = false;                                                     // Mark the background flag as false
    foreground_mode = true;                                                     // Set the marker showing that SIGSTP is in effect
	} else {
		printf("Exiting foreground-only mode\n");                                   // Inform the user that we're now exiting foreground-only
		flushIO();                                                                  // Call function to clear stdio buffer
    foreground_mode = false;                                                    // Switch the background
	}
}

/************************************************************************
* Description: clear_arrays function
* Function set the memory for both the command array and the arguments
* multidimensional arrays to null (\0).
*************************************************************************/

void clear_arrays() {
  int i = 0;                                                                    // Establish a counter variable that loops through arguments array
  memset(user_command, '\0', MAX_COMMAND);                                      // Set memory allocation for commands array to null
  for(i = 0; i < MAX_ARGUMENTS; i++) {                                          // Loop through each array in the arguments array
    arguments[i] = NULL;                                                        // Assign each character array in the arguments array to NULL
  }
}

/************************************************************************
* Description: running function
* Function checks if there are any processes running in the background
* and will print its status depending on if it exited or was killed
* from a signal
*************************************************************************/

void running() {
	int i = 0;                                                                    // Establish a counter integer variable
	for (i = 0; i < background_counter; i++) {                                    // Loop through each process in the background
		if (waitpid(background[i], &child_exit, WNOHANG) > 0) {                     // If The process was terminated by a signal
			if (WIFSIGNALED(child_exit)) {                                            // And if WIFSIGNALED returns a value
				printf("background pid terminated is %d\n", background[i]);             // Print the pid of the background process that was terminated
				printf("terminated by signal %d\n", WTERMSIG(child_exit));              // Inform the user how the program was terminated
        flushIO();                                                              // Call function to flush the stio buffers
        termination = WTERMSIG(child_exit);                                     // Set the termination message to an integer value
			}
			if (WIFEXITED(child_exit)) {                                              // Check if the background process has exited
				printf("exit value %d\n", WEXITSTATUS(child_exit));                     // print the exit code (success or failure)
        flushIO();                                                              // Call function to flush stdio buffers
        termination = 0;                                                        // Reset the termination code
			}
		}
	}
}

/************************************************************************
* Description: display_prompt function
* Displays a prompt for the user to user_command a command into the shell.
* Uses fgets to store the string and then immediately calls a separate
* function to remove the trailing break line.  Receives a string array
* from main.  Returns nothing.
*************************************************************************/

void prompt_user() {
  printf(": ");                                                                 // Displays prompt icon on terminal
  flushIO();                                                                    // Call function to flush stdio after each prompt
  fgets(user_command, MAX_ARGUMENTS, stdin);                                    // Get user user_command (stdin) and store in command character array
  removeBreakLine(user_command);                                                // Call function to remove the trailing breakline (enter)
}

/************************************************************************
* Description: removeBreakLine function
* Function receives a string as an argument and replaces trailing
* break line characters with a null terminator.  Returns nothing
*************************************************************************/

void removeBreakLine() {
  user_command[strcspn(user_command, "\n")] = '\0';                             // Replace the break line with a null terminator
}

/************************************************************************
* Description: interpret_command function
* Function takes the user user_command command and segments each word into a
* separate argument.  Uses spaces " " as a delimiters to determine where
* one word ends and another begins.
*
* EXTERNAL RESOURCES:
* http://www.cplusplus.com/reference/cstring/strtok/
*************************************************************************/

void parse_command() {
  argument_total = 0;                                                           // Initialize the counter variable for the total number of arguments (Set to zero)
  char* modifier = strtok(user_command, " ");                                   // Initialize our strtok token (give it the command string and space delimiters
  while(modifier != NULL){                                                      // Traverse the string until NULL is reached
    arguments[argument_total] = modifier;                                       // Assign the current word to the current space in the string array
    modifier = strtok(NULL, " ");                                               // Move the strtok token to the next word in the string
    argument_total ++;                                                          // Increment the counter to also move forward in the array
  }
}

/************************************************************************
* Description: route_command function
* Function takes the user user_command from the command line and determines
* the appropriate function to perform the appropriate action.  Receives
* the arguments array of character arrays, the total number of arguments
* integer, and a bool pointer to determine if the program should be
* terminated.  Returns nothing to main.
*************************************************************************/

void route_command() {
  if ((arguments[0] == NULL) || (arguments[0][0] == '#')) {                     // If the user entered a blank line (return key only) or a line that begins with an octothorp (#)
    return;                                                                     // Ignore the command and return the prompt
  } else if (strcmp(arguments[0], "cd") == 0) {                                 // If the user has user_command the command 'cd' into the terminal
    change_directory();                                                         // Call function that manages changing the working directory
  } else if (strcmp(arguments[0], "exit") == 0) {                               // If the user has user_command the command 'exit' into the terminal
    exit_program();                                                             // Call the function to exit the program and kill outstanding processes
  } else if (strcmp(arguments[0], "status") == 0) {                             // If the user has user_command the command 'status' into the terminal
    status();                                                                   // Call the function that outputs the status information
  } else {                                                                      // Otherwise
    check_background();                                                         // Call function to determine if the process needs to be executed in the background (&)
    check_expansion();                                                          // Call function to determine if PID expansion is needed ($$)
    check_redirection();                                                        // Call function to determine if stdio redirection was user_command (<, >)
    fork_pid();                                                                 // Call function to fork the process and run non-builtin system command
  }
}

/************************************************************************
* Description: change_directory function
* This function is called when the user user_commands 'cd' into the terminal
* The function changes the working directory of the shell.  By itself -
* with no arguments - it changes to the directory specified in the HOME
* environment variable.  Otherwise it navigates to the specified directory
* declared in the second argument.
*
* EXTERNAL RESOURCES:
* https://stackoverflow.com/questions/298510/how-to-get-the-current-
* directory-in-a-c-program?utm_medium=organic&utm_source=google_rich_
* qa&utm_campaign=google_rich_qa
*************************************************************************/

void change_directory() {
  code = EXIT_SUCCESS;                                                          // Set status to EXIT_SUCCESS (0) indicating that command processed as expected.
	if (argument_total == 1) {                                                    // If user user_command the cd command with no other arguments
		chdir(getenv(HOME));                                                        // Change directory to the environment variable listed as "HOME"
	} else {                                                                      // Otherwise,
    int success = chdir(arguments[1]);                                          // Call change directory function (chdir) and assign argument 1 as the directory
		if (success == -1) {                                                        // If chdir returns an error status (-1)
      code = EXIT_FAILURE;                                                      // Set status to EXIT_FAILURE (1) indicating and issue occurred.
      printf("cd: No such file or directory: %s\n", arguments[1]);              // Output error message to the console (Used standard linux cd error)
			flushIO();                                                                // Call function to clear stdout
    }
	}
}

/************************************************************************
* Description: exit_program function
* This function is called when the user user_commands 'exit' into the terminal
* The function takes no arguments and has not returned values.  When this
* function will executed, it will terminate the shell and close any other
* processes or jobs that your shell has started before it terminates the
* program
*************************************************************************/

void exit_program() {
  int i = 0;                                                                    // Establish and integer variable for use as counter
  for(i = 0; i < background_counter; i ++) {                                    // Go through each running process in the background.
    if(background[i] < 0) {                                                     // If there's a valid process located in the array
      int kill_status = kill(background[i], SIGKILL);                           // Send it a kill signal
      if(kill_status < 0) {                                                     // If the kill signal failed
        printf("Failed to terminate PID %d\n", background[i]);                  // Print pid number to the screen that could not be killed
        flushIO();                                                              // Call function to clear the stdio buffers
      }
    }
  }
  die = true;                                                                   // Change the boolean kill variable from false and true
}

/************************************************************************
* Description: status function
* This function is called when the user user_commands 'status' into the terminal.
* Function prints out either the exit status or the terminating signals
* of the last foreground process (not both, processes killed by signals
* do not have exit statuses!) ran by the shell.
*************************************************************************/

void status() {
  if (termination > 0) {                                                        // If the termination signal is zero
    printf("terminated by signal %d\n", termination);                           // Print the signal value to the console
  } else {                                                                      // Otherwise,
    printf("exit value %d\n", code);                                            // Print the exit value (status) to the console
  }
  flushIO();                                                                    // Call function to flush the stdout buffer
}

/************************************************************************
* Description: check_background function
* Function determines if the last modifier in the command sequence is
* an ampersand '&' indicating the process should be run in the backgound
* of the program without locking the command line.
*************************************************************************/

void check_background() {
  if(strcmp(arguments[argument_total - 1], "&") == 0) {                         // Check if the last command in the argument string is '&'
    if(foreground_mode == true) {                                               // Confirm that foreground-only mode is not enabled (Ctrl+Z)
      run_background = false;                                                   // If it is, then do not allow for background process to be enabled.
    } else {                                                                    // Otherwise
      run_background = true;                                                    // Enable background process
    }
    arguments[argument_total - 1] = NULL;                                       // Remove the '&' from the end of the argument array
    argument_total --;                                                          // Reduce the argument total by 1
  }
}

/************************************************************************
* Description: check_redirection function
* Function checks for output and user_command redirection in the user's
* command.  The funciton is passed the array of arguments, the argument
* total, and two boolean status indicator for redirection that are passed by
* reference. No return values, but integer pointer for redirection is
* manipulated if certain constraints are met.
*************************************************************************/

void check_redirection() {
  int original_arguments = argument_total;
  redirection_in = false;                                                       // Initialize stdin redirection boolean to false
  redirection_out = false;                                                      // Initialize stdout redirection boolean to false
  int i = 0;                                                                    // Establish loop counter variable and initialize to zero
  for(i = 0; i < original_arguments; i ++) {                                    // Loop through each string in the arguments array
    if(arguments[i][0] == '>') {                                                // If a stdin redirection found (>)
        redirection_out = true;                                                 // Change the indicator value to true (bool passed by reference)
        redirection_out_location = i;                                           // Store the location of where the stdout redirection occurred
        arguments[redirection_out_location] = NULL;                             // Remove the argument from the array
    } else if(arguments[i][0] ==  '<') {                                        // If a stdout redirection found (<)
        redirection_in = true;                                                  // Change the indicator value to true (bool passed by reference)
        redirection_in_location = i;                                            // Store the location of where the stdin redirection occurred
        arguments[redirection_in_location] = NULL;                              // Remove the command from the argument
    } else {
      continue;                                                                 // Continue through the list if neither < or > are found
    }
  }

}

/************************************************************************
* Description: check_expansion function
* The function checks to see if the user user_command two dollar signs ($$)
* signifying that they want to perform an expansion.  Function receives the
* list of arguments, the number of arguments, and a boolean variable (passed
* by reference) to indicate if an expansion request is located.
*
* EXTERNAL RESOURCES:
* http://www.cplusplus.com/reference/cstring/strstr/
*************************************************************************/

void check_expansion() {
  int i = 0;                                                                    // Create a counter variable to loop through all arguments
  for(i = 0; i < argument_total; i ++){                                         // For each argument in the command
    if(strstr(arguments[i], "$$") != NULL){                                     // Search through the character array for an instance of "$$"
      expansion_location = i;                                                   // Store the location of where the expansion location was found
      expand_pid();                                                             // Call function to replace the $$ signs with the pid
    }
  }
}

/************************************************************************
* Description: expand_pid function
* Function receives the array of arguments and the location where the
* Double $$ expansion was made.  It expands the $$ into the shell PID
* and appends that the arguments in place of the expansion signifier.
*************************************************************************/

void expand_pid() {
  int new_length = 0;                                                           // Create and initialize integer variable to store the updated length of the string
  char expanded[MAX_ARGUMENTS] = { 0 };			                                    // Create and initialize array to temporarily store expansion string
  new_length = (strlen(arguments[expansion_location]) - 2);                     // Establish a variable to store the size of the string minus ending $$
  memset(expanded, '\0', MAX_ARGUMENTS);                                        // Clear the expansion array before initializing (safe)
  strncpy(expanded, arguments[expansion_location], new_length);		              // Copy the user user_command string into the expansion holder.  Remove last two $$ characters
  strcpy(arguments[expansion_location], expanded);					                    // Copy expansion back to argument array
  sprintf(expanded, "%d", getppid());		                                        // Get the shell PID information and append that to the expansion holder
	strcat(arguments[expansion_location], expanded);					                    // Copy temporary expansion string back to user argument
}

/************************************************************************
* Description: fork_pid function
* Function
*************************************************************************/

void fork_pid() {
	child_pid = fork();                                                           // Fork a new child process and store the associated PID
	if (child_pid < 0) {                                                          // If the new PID is an invalid number
		perror("bash: Unable to fork child process\n");                             // Then print and error message to the console
		flushIO();                                                                  // Call function to flush the stdio buffers
		exit(EXIT_FAILURE);                                                         // Exit with a status that indicates a failure occurred
  } else if (child_pid == 0) {                                                  // If the child process is successfully created, then
    execute();                                                                  // Call function to execute the commands in the child process
  } else {                                                                      // Otherwise,
    if(run_background == true) {                                                // If the command was executed in the background
      background[background_counter] = child_pid;                               // Add process to the queue of background PIDs
      background_counter ++;                                                    // Increase the counter for total number of background processes
      waitpid(child_pid, &child_exit, WNOHANG);                                 // establish a waitpid but the prompt is not hung until completion.
      printf("background pid is %d\n", child_pid);                              // Print the child PID to the console for the user to view.
      flushIO();                                                                // Call function to flush the stdio buffers
    } else {                                                                    // Otherwise if the command is issued in the foreground
      waitpid(child_pid, &child_exit, 0);                                       // Call waitpid function but keep the console prompt hung until completion
      if (WIFEXITED(child_exit)) {                                              // Listen for when the child process has ended.
         code = WEXITSTATUS(child_exit);                                        // Assign the exit status code to the parent process status code variable
       }
    }
  }
}

/************************************************************************
* Description: execute function
* Function is used to actually execute the commands via the execvp()
* function.  It also uses dup2() to catch any redirection filenames.
*************************************************************************/

void execute() {
  char* fileNameIn = NULL;                                                      // Establish character array for storing the fileNameIn
  char* fileNameOut = NULL;                                                     // Establish character array for storing the fileNameOut
  int fileInDesc = 0,                                                           // Establish integer for file in status code
	    fileOutDesc = 0;                                                          // Establish integer for file out status code

  if(redirection_in == true) {                                                  // If stdin redirection occurred
    if(arguments[redirection_in_location + 1] == NULL){                         // If there is no argument provided after the <
      fileNameIn = "/dev/null";                                                 // Then use /dev/null
    } else {                                                                    // Otherwise,
      fileNameIn = arguments[redirection_in_location + 1];                      // Use the argument provided on the commandline as the filname
      arguments[redirection_in_location + 1] = NULL;                            // Remove argument from the array
    }
    fileInDesc = open(fileNameIn, O_RDONLY);                                    // Open the file as read only and assign code to integer
    if(fileInDesc < 0) {                                                        // If there was an error
      printf("%s: No such file or directory\n", fileNameIn);                    // Print and error message to the screen
      flushIO();                                                                // Call function to flush stdio buffers
      exit(EXIT_FAILURE);                                                       // Exit with error status
    } else {                                                                    // Otherwise
      dup2(fileInDesc, 0);                                                      // Assign dup2 the fileInDesc and the indicator that its stdin
    }
    fcntl(fileInDesc, F_SETFD, FD_CLOEXEC);                                     // Close the file once executed in dup2
  }

  if(redirection_out == true) {                                                 // If redirection_out is specified
    if(arguments[redirection_out_location + 1] == NULL){                        // If there is no argument provided after the >
      fileNameOut = "/dev/null";                                                // Then use /dev/null
    } else {                                                                    // Otherwise,
      fileNameOut = arguments[redirection_out_location + 1];                    // Use the argument provided on the commandline as the filname
      arguments[redirection_out_location + 1] = NULL;                           // Remove argument from the array
    }
    fileOutDesc = open(fileNameOut, O_WRONLY | O_CREAT | O_TRUNC, 0644);        // Open the file as write only and assign code to integer.  Create if necessary
    if(fileOutDesc < 0) {                                                       // If there was an error
      printf ("%s: No such file or directory\n", fileNameOut);                  // Print and error message to the screen
      flushIO();                                                                // Call function to flush stdio buffers
      exit(EXIT_FAILURE);                                                       // Exit with error status
    } else {                                                                    // Otherwise
      dup2(fileOutDesc, 1);                                                     // Assign dup2 the fileInDesc and the indicator that its stdout
    }
    fcntl(fileOutDesc, F_SETFD, FD_CLOEXEC);                                    // Close the file once executed in dup2
  }

  if((code = execvp(arguments[0], arguments)) != 0) {                           // Run the command in the execvp() function.  If an error occurred then...
    perror("Error");                                                            // Print the error to the screen
    flushIO();                                                                  // Call the function to flush the stdio buffers
    exit(code);                                                                 // Exit and return the status code that occurred in the error
  }
}

/************************************************************************
* Description: flushIO function
* Simple function that is called after any output is printed to the
* console.  Functions calls fflush twice to flush both stdin and stdout
* buffers to ensure that all text reaches the screen.
*************************************************************************/

void flushIO() {
  fflush(stdout);                                                               // Call function to flush standard out (stdout)
  fflush(stdin);                                                                // Call function to flush standard in (stdin)
}
