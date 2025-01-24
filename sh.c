#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "./jobs.h"

// MACROS
#define BUFFER_SIZE 1024
#define TRUE 1
#define FALSE 0
#define FG 2
#define BG 3

// GLOBAL VARIABLES
job_list_t *job_list;
int jid; // the job id count
int is_bg; //value is TRUE if currently a background process, FALSE if foreground


/**
 * This function handles ignoring the SIGINT, SIGTSTP and SIGTTOU from shell. It is called 
 * at the start of the main function.
*/
void init_ignoring_signal(){
    if(signal(SIGINT, SIG_IGN) == SIG_ERR){
        printf("signal ignore error");
    }
    if(signal(SIGTSTP, SIG_IGN) == SIG_ERR){
        printf("signal ignore error");
    }
    if(signal(SIGTTOU, SIG_IGN) == SIG_ERR){
        printf("signal ignore error");
    }
}

/**
 * This function handles setting signals to default again. This is done within fork() for any
 * child process
*/
void default_child_signals(){
    if(signal(SIGINT, SIG_DFL) == SIG_ERR){
        printf("signal default error");
    }
    if(signal(SIGTSTP, SIG_DFL) == SIG_ERR){
        printf("signal default error");
    }
    if(signal(SIGTTOU, SIG_DFL) == SIG_ERR){
        printf("signal default error");
    }
}

/**
 * The change_location() function handles processing the 'fg' and 'bg' commands. It first processes the jid 
 * value given. If it is an fg command: Then it resumes a process in the foreground. If it is a bg command:
 * then it should resume a process in the background. 
 * @param new_ground: has the either FG or BG macro 
 * @param tokens[]: tokens array that contains the full file paths/command and
 * arguments
 * @param counter: number of elements in tokens
 * @return 0 if no error, -1 if error
*/
int change_location(int new_ground, char *tokens[], int counter){

    // error check to make sure there's a file path after the fg or bg command,
    // 2 because we incremented after adding elements above
    if(counter != 2){
        return 1;
    }

    char *process_jid_with_percent = tokens[1];
    char *pointer_to_percent = strrchr(process_jid_with_percent, '%');
    char *process_jid_char = pointer_to_percent + 1; 
    int process_jid = atoi(process_jid_char);
    pid_t process_pid;

    // error-checking for invalid jid
    if(process_jid > jid){
        fprintf(stderr, "job not found\n");
        return -1;
    }

    process_pid = get_job_pid(job_list, process_jid);

    // moving background process to foreground
    if (new_ground == FG){

        int grpid = getpgrp();

        // throw an error if group pid is less than 0
        if(grpid < 0){
            return -1;
        }
        
        tcsetpgrp(0, process_pid);
        kill(-process_pid, SIGCONT); 

        int status;
        waitpid(process_pid, &status, WUNTRACED);

        if(WIFEXITED(status)){
            // terminated normally, remove foreground process that terminated
            remove_job_jid(job_list, process_jid);
            jid--; 
        }

        if (WIFSTOPPED(status)) {
            // stopped/paused, update job's enum in job list
            update_job_pid(job_list, process_pid, STOPPED);
            fprintf(stdout, "[%d] (%d) suspended by signal %d\n", process_jid, process_pid, WSTOPSIG(status));
        }
        if (WIFSIGNALED(status)) {
            // terminated by a signal, remove foregroud process that terminated
            fprintf(stdout, "[%d] (%d) terminated by signal %d\n", process_jid, process_pid, WTERMSIG(status));
            remove_job_jid(job_list, process_jid);
            jid--; 

        } 
        tcsetpgrp(0, getpgrp()); 

    // moving process to background
    } else if (new_ground == BG){
        // update it in the job list
        if (update_job_jid(job_list, process_jid, RUNNING) == -1){
            return -1;
        }
        kill(-process_pid, SIGCONT); 

    }
    return 0;

}

/**
 * The check_built_in function handles the built in commands for shell. It
 * handles cd, ln, rm and exit. There is also error checking within the function
 * to ensure that cd has an argument following it and perror() is called for
 * syscall error checking. This function is called within handle_commands(), so
 * that we can first check if the command is a built-in shell command before
 * trying to proceed with the non-built in process.
 * @param tokens: tokens array that contains the full file paths/command and
 * arguments
 * @param argv: argv array that contains the binary path (command), and
 * arguments
 * @return - returns non-zero value if error occured
 *
 */
int check_built_in(char *tokens[], char *argv[], int counter) {
    const char *command = tokens[0];  // command will be at 0th index of tokens
    int return_val = 1;  // default return should be error, unless a built in
                         // command is executed

    // can check for 3 types of commands if string length is 2, otherwise tries
    // to go check for "exit" immediately, for better time efficiency
    if (strlen(command) == 2) {
        // check if cd
        if (!strncmp(command, "cd", 2)) {
            return_val = 0;  // sets return_val to 0, to indicate that this
                             // function carried out a built-in successfully

            // error check there should be an argument after cd
            if (argv[1] == NULL) {
                fprintf(stderr, "cd: syntax error\n");
                return -1;
            }

            // file/directory after cd will be in argv[1]
            const char *full_path = argv[1];
            // error check chdir() syscall
            if (chdir(full_path) != 0) {
                perror("cd");
            }

        }
        // check if ln
        if (!strncmp(command, "ln", 2)) {
            return_val = 0;
            const char *old_path = argv[1];
            const char *new_path = argv[2];
            // error check link() syscall
            if (link(old_path, new_path) != 0) {
                perror("link");
            }
        }
        // check if rm
        if (!strncmp(command, "rm", 2)) {
            return_val = 0;
            const char *path = argv[1];
            // error check unlink() syscall
            if (unlink(path) != 0) {
                perror("unlink");
            }
        }
        // check if bg
        if (!strncmp(command, "bg", 2)){
            return_val = 0;
            //error check
            if (change_location(BG, tokens, counter) != 0){
                return -1;
            }

        }
        // check if fg
        if (!strncmp(command, "fg", 2)){
            return_val = 0;
            //error check
            if (change_location(FG, tokens, counter) != 0){
                return -1;
            }
            
        }
    }
    
    // check if exit
    if (strlen(command) == 4) {
        if (!strncmp(command, "exit", 4)) {
            return_val = 0;
            cleanup_job_list(job_list);
            exit(0);  // exit doesn't require error checking
        }

        //check for jobs, call helper jobs()
        if (!strncmp(command, "jobs", 4)){
            return_val = 0;
            jobs(job_list);
        }
    }
    return return_val;
}

/**
 * The redirect_helper() is a helper function that is called in
 * redirection_handler() to avoid simplify code and avoid repetitiveness. It
 * redirects a given file descriptor, and opens it with the corresponding flags
 * and permission. There is error-checking for close() and open()
 *
 * @param redirections: an array that has only redirection symbols and
 * corresponding filenames
 * @param index: will be either 0 or 2, it is the index at which the current
 * redir symbol is at in the redirections array
 * @param fd: file descriptor
 * @param flags: the flags necessary for open() for that file
 * @param permission: the permissions for the file
 */
void redirect_helper(char *redirections[], int index, int fd, int flags,
                     int permission) {
    // error check close() syscall
    if (close(fd) != 0) {
        perror("close");
        cleanup_job_list(job_list);
        exit(1);
    }

    // it is index + 1, since the file is located at the index after the symbol
    // in redirections[]
    // error check open() syscall
    if (open(redirections[index + 1], flags, permission) == -1) {
        perror("open");
        cleanup_job_list(job_list);
        exit(1);
    }
}

/**
 * The redirection_handler() handles the redirection of file descriptors based
 * on what redirection symbol is being read. If it is <, then fd = 0, since we
 * are redirecting input, and so on.
 *
 * @param redirections: an array that has only redirection symbols and
 * corresponding filenames
 */
void redirection_handler(char *redirections[]) {
    int counter = 0;
    // preparing the counter for the for loop - possible counter values are: 0,
    // 1, 3
    if (redirections[0] != NULL) {
        counter++;
        if (redirections[2] != NULL) {
            counter += 2;
        }
    }

    // a loop that will iterate once if there is only redir symbol, and twice if
    // it's input AND output
    for (int i = 0; i < counter; i += 2) {
        char *redir_symbol = redirections[i];
        if (strlen(redir_symbol) == 1) {
            // < [path] where file [path] as standard input (fd 0)
            if (*redir_symbol == '<') {
                redirect_helper(redirections, i, 0, O_RDONLY, 0777);
                // > [path] where file [path] is standard output (fd 1)
            } else if (*redir_symbol == '>') {
                redirect_helper(redirections, i, 1,
                                O_WRONLY | O_CREAT | O_TRUNC, 0777);
            }
            // >> [path] where file [path] is standard output (fd 1)
        } else if ((!strncmp(redir_symbol, ">>", strlen(redir_symbol)))) {
            redirect_helper(redirections, i, 1, O_WRONLY | O_APPEND | O_CREAT,
                            0777);
        }
    }
}

/**
 * The reap() function is called right before the prompt is printed in the terminal. This is called in main(). 
 * This is to ensure that there are no zombie processes left in the background. This checks for all background 
 * by calling this in a while loop with waitpid(). Checks if process changes status, terminated normally,
 * terminated by a signal, is stopped/paused. 
 * @param wret: the pid of the specific background process
 * @param wstatus: status of the process which is updated by waitpid()
*/
void reap(int wret, int wstatus){

    if (WIFCONTINUED(wstatus)){
        //change status, and printf
        if (update_job_pid(job_list, wret, RUNNING) == 0){
            fprintf(stdout, "[%d] (%d) resumed\n", get_job_jid(job_list, wret), wret);
        }  
    
    }
    if (WIFEXITED(wstatus)) {
        //terminated normally
        fprintf(stdout, "[%d] (%d) terminated with exit status %d\n", get_job_jid(job_list, wret), wret, WEXITSTATUS(wstatus));
        remove_job_pid(job_list, wret);
        jid--;

    }

    if (WIFSIGNALED(wstatus)) {
        //terminated by a signal
        fprintf(stdout, "[%d] (%d) terminated by signal %d\n", get_job_jid(job_list, wret), wret, WTERMSIG(wstatus));
        remove_job_pid(job_list, wret);
        jid--;
    }

    if (WIFSTOPPED(wstatus)) {
        //stopped/paused
        if (update_job_pid(job_list, wret, STOPPED) == 0){
            fprintf(stdout, "[%d] (%d) suspended by signal %d\n", get_job_jid(job_list, wret), wret, WSTOPSIG(wstatus));
        }
    }
}


/**
 * handle_commands() is a function that is called in main() after parsing, to
 * handle built in and non-built commands along with redirection. It forks if it
 * is a non-built in command and calls execv() in the child process. It also handles
 * @param tokens: tokens array that contains the full file paths/command and
 * arguments
 * @param argv: argv array that contains the binary path (command), and
 * arguments
 * @param redirections: an array that has only redirection symbols and
 * corresponding filenames
 * @param is_bg: boolean that is TRUE if it is a background process (&), and FALSE if foreground
 * @param counter: number of elements in tokens and argv
 *
 * @return 0 if there is no error
 */
int handle_commands(char *tokens[], char *argv[], char *redirections[], int counter) {
    pid_t pid;
    if (check_built_in(tokens, argv, counter) == 1) {
        // child process
        if ((pid = fork()) == 0) {
            setpgid(0, 0);
    
            // only for foreground
            if (!is_bg){
                int grpid = getpgrp();
                if(grpid < 0){
                    return -1; // throw an error it grp pid is less than 0
                }
                tcsetpgrp(0, grpid); // only for foreground, gives terminal control    
            }

            // signal handling, reset all signals ignored in the parent process 
            // back to their default behaviour
            default_child_signals();
            
            redirection_handler(redirections);
            execv(tokens[0], argv);

            // error checking execv
            perror("execv");
            cleanup_job_list(job_list);
            exit(1);
        }
    
        if (is_bg){ // if bg, add to jobs list 

            // if background state is runnning, add to job_list
            if (add_job(job_list, jid, pid, RUNNING, tokens[0]) == -1){
                return -1; // error check
            }

            // print background process that just started running
            fprintf(stdout, "[%d] (%d)\n", jid, get_job_pid(job_list, jid));
            jid++;
            
        }

        else{
            int wret; 
            int wstatus;
            // call waitpid once for foreground process
            wret = waitpid(pid, &wstatus, WUNTRACED);

            if (WIFSTOPPED(wstatus)) {
                // stopped/paused
                // if foreground, add to the job_list
                add_job(job_list, jid, wret, STOPPED, tokens[0]);
                fprintf(stdout, "[%d] (%d) suspended by signal %d\n", jid, wret, WSTOPSIG(wstatus));
                jid++;
            }
            if (WIFSIGNALED(wstatus)) {
                // terminated by a signal
                fprintf(stdout, "[%d] (%d) terminated by signal %d\n", jid, wret, WTERMSIG(wstatus));
            } 
            
        }
        // give back terminal control
        int grpid = getpgrp();
        if(grpid < 0){
            return -1; // throw an error if group pid is less than 0
        }
        tcsetpgrp(0, grpid);

    }
        
    return 0;
}


/**
 * The parse() function is responsible for parsing the user input once it is
 * read from the REPL. It is called within main(). It builds the tokens, argv
 * and redirections arrays through this. It is also responsible for catching
 * syntax errors and invalid user input early on for better time efficiency and
 * fewer wasteful operations are executed. Checks for & command for background processes
 *
 * @param buffer: input array
 * @param tokens: tokens array that contains the full file paths/command and
 * arguments
 * @param argv: argv array that contains the binary path (command), and
 * arguments
 * @param redirections: an array that has only redirection symbols and
 * corresponding filenames
 *
 * @return 0 if there is no error in parsing
 */
int parse(char buffer[BUFFER_SIZE], char *tokens[], char *argv[],
          char *redirections[]) {
    // setting up local variables
    char *one_token;       // the current token
    char *previous_token;  // token before current token
    char *last_char;
    is_bg = FALSE;

    // index counter for the tokens and argv array
    int counter = 0;
    // counter for number of input redir (shouldn't be more than 1)
    int input_redir_count = 0;
    // counter for number of output redir (shouldn't be more than 1)
    int output_redir_count = 0;
    // index counter for the redirections array
    int redirect_count = 0;

    // strtok takes care of the white space and tabs
    while ((one_token = strtok(buffer, " \t\n")) != NULL) {
        tokens[counter] = one_token;

        // error check if there's more than one input redir or output redir
        // symbol
        if (input_redir_count > 1) {
            fprintf(stderr, "syntax error: multiple input files\n");
            return 1;
        }

        if (output_redir_count > 1) {
            fprintf(stderr, "syntax error: multiple output files\n");
            return 1;
        }

        if (counter > 0) {
            previous_token = tokens[counter - 1];

            // if the token before one_token was redir symbol, then don't add
            // filename token to argv add only to redirections array to use for
            // redirections later
            if (*previous_token == '>' || *previous_token == '<') {
                if (*one_token == '>' || *one_token == '<') {
                    fprintf(stderr,
                            "Syntax error: file is a redirection symbol\n");
                    return 1;
                }

                redirections[redirect_count] = one_token;
                redirect_count++;
                counter--;  // reduce counter index, so that tokens[counter] can
                            // be overwritten as we
                // don't need redirection symbols and filenames in tokens
                buffer = NULL;
                // continue should just skip the stuff underneath and run next
                // iteration of the loop
                continue;
            }
        }

        // check if it is redirection symbol, don't add to argv, add to
        // redirections
        if (*one_token == '>') {
            redirections[redirect_count] = one_token;
            redirect_count++;
            output_redir_count++;
            // error check for redirect_count > 1
            counter++;
            buffer = NULL;
            // continue should just skip the stuff underneath and run next
            // iteration of the loop
            continue;
            // check if it is redirection symbol, don't add to argv, add to
            // redirections
        } else if (*one_token == '<') {
            redirections[redirect_count] = one_token;
            redirect_count++;
            input_redir_count++;
            // error check for redirect_count > 1
            counter++;
            buffer = NULL;
            // continue should just skip the stuff underneath and run next
            // iteration of the loop
            continue;
        }

        // if it's the 0th index, need to make sure full file path doesn't go
        // into argv
        if (counter == 0) {
            // finds last occurence of c in str and returns pointer to it
            last_char = strrchr(one_token, '/');

            if (last_char == NULL) {
                // should be same as tokens if no / is found
                argv[counter] = tokens[counter];

            } else {
                argv[counter] =
                    last_char + 1;  // since last_char[0] is '/', want to
                // pointer to be after that
            }

            // for anything after first token, we don't care if it has a /, so
            // argv should have same thing as tokens
        } else {
            argv[counter] = tokens[counter];
        }
        counter++;
        buffer = NULL;  // if str is NULL, strtok will return a pointer to
        // token right after the one returned in the previous call, or NULL if
        // no more tokens
    }

    // error check to make sure there is a filename after the redirect
    // symbol
    if (input_redir_count > 0 || output_redir_count > 0) {
        if (redirect_count != 2 && redirect_count != 4) {
            fprintf(stderr, "No file name given after redirect\n");
            return 1;
        }
    }

    // error check if it was all whitespace/tabs
    if (tokens[0] == NULL) {
        return 1;
    }

    argv[counter] = NULL;  // ends argv with null

    // check if it is a background process
    if (*argv[counter - 1] == '&'){
        is_bg = TRUE;
        // remove the & character from the argv array, since it is not an argument
        argv[counter - 1] = NULL;
    }


    // only if parsing is correct and inputs are valid do we
    // continue to run the command

    handle_commands(tokens, argv, redirections, counter);

    return 0;
}

/**
 * The main() function is responsible for reading user input through the REPL,
 * and showing the 33sh prompt.
 *
 * @return 0 if no error
 */
int main() {
    char buf[BUFFER_SIZE];
    ssize_t input_size;  // size of user input

    init_ignoring_signal();
    job_list = init_job_list();
    jid = 1;

    // show prompt initially when the program first runs:
    #ifdef PROMPT
        if (printf("33sh> ") < 0) {
            fprintf(stderr, "ERROR printing prompt\n");
        }

        if (fflush(stdout) < 0) {
            fprintf(stderr, "ERROR printing prompt\n");
        }
    #endif

    // REPL, keep reading input till read returns
    while ((input_size = read(STDIN_FILENO, buf, BUFFER_SIZE)) > 0) {
        // initialize all arrays with size of input_size, since it cannot be
        // larger than that, for better memory efficiency
        char *argv[input_size];
        char *redirections[input_size];  // contains redir symbols with file
                                         // name after
        char *tokens[input_size];

        // initializing all arrays to 0x0 so they are null terminated
        memset(tokens, 0, ((long unsigned int)input_size) * sizeof(char *));
        memset(argv, 0, ((long unsigned int)input_size) * sizeof(char *));
        memset(redirections, 0,
               ((long unsigned int)input_size) * sizeof(char *));

        // case for no input, only hit enter - should skip everything and
        // reprint prompt
        if (!(input_size == 1 && buf[0] == '\n')) {
            if (buf != NULL) {
                buf[input_size - 1] = '\0';  // null terminate buffer

                parse(buf, tokens, argv, redirections);
                    
            } else {
                continue;
            }
        }

        //reaping
        int wret_2;
        int wstatus_2;
        while((wret_2 = waitpid(-1, &wstatus_2, WNOHANG|WUNTRACED|WCONTINUED)) > 0){
            reap(wret_2, wstatus_2);
        }

// shows prompt
#ifdef PROMPT
        if (printf("33sh> ") < 0) {
            fprintf(stderr, "ERROR printing prompt\n");
        }

        if (fflush(stdout) < 0) {
            fprintf(stderr, "ERROR printing prompt\n");
        }
#endif

    }
    cleanup_job_list(job_list);
    return 0;
}
