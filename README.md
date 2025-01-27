# shell-and-repl

This project was completed as a partnered project. We worked collaboratively on all aspects of this project, discussing implementations and debugging issues together. I was responsible for more implementation in the makefile and for foreground/background commands, while my partnered focused more on debugging for reaping and jobs commands. Together, we created implementation for ignoring and resetting signals, running processes in the background, and reaping setup.

# Instructions for use
Run the "make clean all" command to updateto current version of code and run the "/.33sh" command with or without the PROMPT macro to enter the shell.

# Design
Parsing: When a user inputs a command in the 33sh command prompt line, the entered command will be be parsed, which is done by using strtok to separate each token in the input. Redirections are stored in the redirections array, all tokens except redirections are stored in the tokens array, and all tokens except redirections and the full file path (instead only the binary path is stored) are stored in the argv array. If there is an incorrect number of redirections in relation to the file to which to be redirected, an error is thrown to the user. If only whitespace is entered as input, a new line appears and no error is thrown.

Built-in commands: If parsing returns without error, then the commands within the input are handled. First, the tokens array is checked for built-ins. If the first argument in the tokens array is equal to "cd," "ln," "rm," or "exit," chdir, link, unlink, or exit is run respectively, using the inputted file paths. If incorrect arguments are supplied or one of the syscalls fails, an appropriate error is thrown to the user.

Non-built-in commands: If the function that handles built-in commands returns 1, non-built-in commands are then handled because there were no built-in commands to handle. A child process is set up and redirections for input and output of the process that the user wants to run are handled (described below). The syscall to execv is used to replace the newly created child process and the full file path (the first element of the tokens array) and the entire argv array are passed into the execv call. If execv returns, meaning the command was unsuccessful, an error is thrown to the user and the system exits. While this child process is run, the parent process waits.

Redirections: If the redirections array is not null, meaning there are redirections to handle, the counter variable is updated to store how many redirections there are to handle (either 1 or 2 redirections; however the counter is incremented by two to accomodate the files to which to redirect that are also stored in the redirections array). Then each redirection symbol is checked and handled appropriately. For input (<), the automatic input file (stdin) is closed and the inputted file is opened to be read from, and read from only. For output (> or >>), the automatic output file (stdout) is closed and the inputted file is opened to be written to, and written to only. If the inputted output file doesn't exist, it is created. Depending on the specified type of output (> vs >>), the inputted output file is either truncated or appended with the output respectively. If any other of the files fail to be opened or closed, an error is thorwn to the user and the system is exited.

Ignoring and resetting signals: When the shell is first run, the signals SIGINT (control-C), SIGTSTP (control-Z), and SIGTTOU (a backgorund process attempting to write to stdout) are ignored, so that of the user attempts to input these commands, they don't work. Then once fork is called and a child process is created, each of these signals is set back to its default so that the child process does not ignore the signals and treats them normally. When each signal is ignored, an error messges prints if ignoring the signal failed.

Handling background processes: If an ampersand is included at the end of the entered commands with a space before it, the argv and tokens array receive that as its last index. If the ampersand is in the argv array, the is_bg boolean is set to true. Once built-in commands are checked for, non-built-in commands are handled, in which terminal control between foreground and background processes it set. Once fork is called, terminal control is given to the child process created as a result of fork if the is_bg boolean is false, meaning the ampersand was not included in the input and the process should be run in the foreground. 
If the process should be run in the background and the is_bg boolean is true, terminal control remains with the shell. Outside of the child process, background processes are then added to the jobs list and assigned a jid and pid. Foreground processes are waited on by using waitpid, which takes in the foreground's pid so know which process to wait for, and returns the process's pid. The predefined macros WIFSTOPPED (if the process is stopped/paused) and WIFSIGNALED (if the process is temrinated by a signal) are then checked to determine if either of these things happened to the foreground process. If the foreground process is stopped, it is added to the jobs list and a message is printed to stdout (whatever that may be set as). If the foreground process is terminated by a signal, a message is printed to atdout (again whatever that may be set as). Finally, temrinal control is set to actually remain with the shell, as mentioned above.

Reaping: At the end of the main function, before the next prompt is printed (if running with the PROMPT macro), waitpid is called in a while loop, so that all background processes are reaped before allowing the user to enter anther input. For every iteration of the loop, the returned pid and status is passed into the reap function. In the reap function, each predefined macro determining the process's status is checked: WIFCONTINUED (if a background process is continued), WIFEXITED (if a background process is terminated normally), WIFSIGNALED (if a background process is terminated by a signal), and WIFSTOPPED (if a background process is stopped/paused). For WIFCONTINUED, the job is updated with the RUNNING enum in the job list and a message is printed that the action was completed. For WIFEXITED, a messsage is printed that the action was successful and then job is removed from the job list. For WIFSIGNALED, a message is printed that the action was successful and the job is removed from the job list. For WIFSTOPPED, the job is updated with the STOPPED enum in the job list and a message is printed that the action was successful. This function then returns and is continually called in the while loop until all background proccesses are reaped, so that the prompt can then be printed again. FInally, every time the program exits, the cleanup function for the jobs list is called to free up memory that was previously allocated to processes running in the background.

Handling changing grounds: If the "fg" or "bg" command is entered into the shell, it is treated as a built in commands because we don't want to fork into a child process if the input is to change the location of a process. In the function that handles built-in commands, if the first index in the argv array is "fg" or "bg" and the next index in the array isn't null, meaning the entry also includes a process to move, the  function to change processes' locations is called. Within this function, the process's jid is determined from the argv array and the pid from the jid. The jid is then checked to make sure it correlates to a process that is indeed in the jobs list (and a mesage is printed if it isn't). 
If the process is to be moved to the foreground, it is given terminal control and killed, so that the process is continued with the correct terminal control (in this case it has terminal control). Waitpid is then called because we must wait for this process to terminated as it is now a foreground process. WIFEXITED, WIFSTOPPED, and WIFSIGNALED are checked for and handled as described above in reaping. 
If the process is to be moved to the background, the job is updated in the jobs list with the running enum and killed, so that the process is continued with the correct terminal control (in this case it does not have terminal control).
If the "jobs" commans is entered into the shell, it is treated as a built in command because again, we don't want to fork into a child process if the input is to run the built-in jobs command. In the function that handles built-in commans, if the first index in the argv array is "jobs," the jobs function is called to print out the current jobs.

# Known bugs
There are no known bugs in our program.
