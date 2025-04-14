#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

int main(int argc, char *argv[]) 
{
    char cmd_buff[514];
    char cwd[514];
    char *pinput;
    int batch_mode = 0;
    int too_long = 0;
    FILE *current_file;

    if (argc > 2){
        char error_message[30] = "An error has occurred\n";
        write(STDOUT_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    if (argc == 2){
        if (fopen(argv[1], "r") == NULL){
            char error_message[30] = "An error has occurred\n";
            write(STDOUT_FILENO, error_message, strlen(error_message));
            exit(1);
        } else {
            batch_mode = 1;
        }
    }
    
    if (batch_mode == 1){
        current_file = fopen(argv[1], "r");
    }

    while (1) {
        char *multiples_table[513];
        int multiples_number = 0;
        
        char pinput_copy[513];
        
        pid_t ret;
        int status;
        int valid_input = 0;
        int basic_input = 0;

        if (batch_mode == 0) {
            myPrint("myshell> ");
        }
        if (batch_mode == 0){
            pinput = fgets(cmd_buff, sizeof(cmd_buff), stdin);
        } else {
            pinput = fgets(cmd_buff, sizeof(cmd_buff), current_file);
        }
        if (!pinput) {
            exit (0);
        }

        while (strlen(pinput) == 513 && pinput[512] != '\n'){
            too_long = 1;
            myPrint(pinput);
            if (batch_mode == 0){
                pinput = fgets(cmd_buff, sizeof(cmd_buff), stdin);
            } else {
                pinput = fgets(cmd_buff, sizeof(cmd_buff), current_file);
            }
        }

        char whitespace_copy[513];
        strcpy(whitespace_copy, pinput);
        if (strcmp(whitespace_copy, "\n") == 0 && too_long == 1){
            myPrint("\n");
        } else {
            char *whitespace_token = strtok(whitespace_copy, " \t\n");
            if (whitespace_token != NULL) {
                myPrint(pinput);
            }
        }
        
        if (too_long == 1){
            valid_input = 0;
        } else {
            valid_input = 1;
        }

        char pinput_multiples_copy[513];
        strcpy(pinput_multiples_copy, pinput);
        char *multiples_token = strtok(pinput_multiples_copy, ";");
        while (multiples_token != NULL){
            multiples_table[multiples_number] = multiples_token;
            multiples_token = strtok(NULL, ";");
            multiples_number++;
        }
        
        if (strcmp(multiples_table[multiples_number - 1], "\n") != 0){
            multiples_table[multiples_number - 1] = strtok(multiples_table[multiples_number - 1], "\n");
        }

        for (unsigned int i = 0; i < multiples_number; i++) {
            char *intable[512];
            char our_input[513];
            int input_words = 0;
            basic_input = 0;
            
            if (too_long == 1){
                valid_input = 0;
            } else {
                valid_input = 1;
            }
            strcpy(our_input, multiples_table[i]); //our_input never changes
            
            // redirection
            char redirection_copy[513]; 
            strcpy(redirection_copy, our_input);
            
            char illegal_redirection_copy[513];
            strcpy(illegal_redirection_copy, multiples_table[i]);

            char *dest_file_name;
            int redirection = 0;
            int advanced_redirection = 0;

            if (redirection_copy[strlen(redirection_copy) - 1] == '>' || redirection_copy[0] == '>'
            || redirection_copy[strlen(redirection_copy) - 1] == '+'){
                valid_input = 0;
                redirection = 1;
            } else {
                char *redirection_token = strtok(redirection_copy, ">");
                strcpy(multiples_table[i], redirection_token);
                redirection_token = (strtok(NULL, ">"));
                if (redirection_token == NULL) {
                    redirection = 0;
                    advanced_redirection = 0;
                } else if (redirection_token[0] == '+'){
                    dest_file_name = strtok(redirection_token, "+ \t");
                    if (dest_file_name == NULL) {
                        valid_input = 0;
                    }
                    advanced_redirection = 1;
                } else {
                    if ((dest_file_name = strtok(redirection_token, " \t")) == NULL){
                        valid_input = 0;
                    }
                    redirection = 1;
                }
            }
            
            char* illegal_token = strtok(illegal_redirection_copy, ">");
            illegal_token = strtok(NULL, ">");
            illegal_token = strtok(NULL, ">");
            if (illegal_token != NULL && redirection == 1){
                valid_input = 0;
            }

            strcpy(illegal_redirection_copy, our_input);
            illegal_token = strtok(illegal_redirection_copy, ">+ \t");
            if (illegal_token == NULL && redirection == 1){
                valid_input = 0;
            }

            //splitting input by space
            strcpy(pinput_copy, multiples_table[i]);
            char *token = strtok(pinput_copy, " \t");
            while (token != NULL && strcmp(token, "\n") != 0){
                intable[input_words] = token;
                input_words++;
                token = strtok(NULL, " \t");
            }
            intable[input_words] = '\0';

            if (input_words > 0){
                
                //exit, cd, pwd
                if (strcmp(intable[0], "exit") == 0 || strcmp(intable[0], "pwd") == 0){
                    basic_input = 1;
                    if (input_words > 1){
                        valid_input = 0;
                    } else {
                        if (redirection == 1 || advanced_redirection == 1){
                            valid_input = 0;
                        } else {
                            valid_input = 1;
                            if (strcmp(intable[0], "exit") == 0){
                                exit(0);
                            }
                            if (strcmp(intable[0], "pwd") == 0){
                                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                                    myPrint(strcat(cwd, "\n"));
                                }
                            }   
                        }
                    }
                }
                
                // singular cd
                if (input_words == 1 && strcmp(intable[0], "cd") == 0){
                    basic_input = 1;
                    if (redirection == 1 || advanced_redirection == 1){
                        valid_input = 0;
                    } else {
                        chdir(getenv("HOME"));
                        valid_input = 1;                        
                    }
                }

                //cd somewhere
                if ((strcmp(intable[0], "cd") == 0) && input_words == 2){
                    if (redirection == 1 || advanced_redirection == 1){
                        valid_input = 0;
                    } else {
                        if (chdir(intable[1]) == 0) {
                            valid_input = 1;
                        } else {
                            valid_input = 0;
                        }
                    }
                    basic_input = 1;
                }

                // non-basic input
                if (basic_input == 0 && valid_input == 1) {
                    ret = fork();
                    if (ret == 0){
                        // advanced redirection
                        if (advanced_redirection == 1){
                            char ar_dest_copy[513];
                            if(dest_file_name[0] == '.'){
                                strcpy(ar_dest_copy, dest_file_name);
                                char *move_address = strtok(ar_dest_copy, "./");
                                if (chdir(move_address) < 0){
                                    exit(1);
                                }
                            }
                            int fd = open(dest_file_name, O_RDWR | O_CREAT, 0666);
                            if (fd < 0){
                                fd = creat(dest_file_name, 0666);
                                dup2(fd, STDOUT_FILENO);
                                close(fd);
                                execvp(intable[0], intable);
                                valid_input = 0;
                                exit(1);
                            } else {
                                char buffer[1024000];
                                int position = 0;
                                lseek(fd, 0, SEEK_SET);
                                size_t sz = read(fd, &buffer[position], 1);
                                while (sz > 0 && position < 1024000){
                                    position++;
                                    sz = read(fd, &buffer[position], 1);
                                }
                                buffer[position] = '\0';

                                // we fork and write our function output into temp file
                                int fd1 = creat("temp", 0666);
                                pid_t ar_ret = fork();
                                if (ar_ret == 0){
                                    dup2(fd1, STDOUT_FILENO);
                                    execvp(intable[0], intable);
                                    exit(1);
                                } else {
                                    waitpid(ar_ret, &status, 0);
                                }
                                close(fd1);
                                
                                // we read this temp file and store it into a temporary buffer
                                fd1 = open("temp", O_RDONLY);
                                lseek(fd1, 0, SEEK_SET);
                                char temp_buffer[1024000];
                                int temp_position = 0;
                                sz = read(fd1, &temp_buffer[temp_position], 1);
                                while (sz > 0 && temp_position < 1024000){
                                    temp_position++;
                                    sz = read(fd1, &temp_buffer[temp_position], 1);
                                }
                                temp_buffer[temp_position] = '\0';
                                close(fd1);
                            
                                // write everything back into our fd file
                                close(fd);
                                fd = open(dest_file_name, O_WRONLY | O_TRUNC);
                                write(fd, temp_buffer, strlen(temp_buffer));
                                write(fd, buffer, strlen(buffer));
                                close(fd);
                                exit(0);
                            }
                        //basic redirection
                        } else if (redirection == 1) {
                            int fd = open(dest_file_name, 0644);
                            if (fd < 0){
                                char dest_copy[513];
                                if(dest_file_name[0] == '.'){
                                    strcpy(dest_copy, dest_file_name);
                                    char *move_address = strtok(dest_copy, "./");
                                    if (chdir(move_address) < 0){
                                        exit(1);
                                    }
                                }
                                fd = creat(dest_file_name, 0644);
                                dup2(fd, STDOUT_FILENO);
                                close(fd);
                                execvp(intable[0], intable);
                                valid_input = 0;
                                exit(1);
                            } else {
                                valid_input = 0;
                                exit(1);
                            }

                        } else {
                            execvp(intable[0], intable);
                            valid_input = 0;
                            exit(1);
                        }
                    } else {
                        waitpid(ret, &status, 0);
                        if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
                            valid_input = 0;
                        } else {
                            if (too_long == 0){
                                valid_input = 1;
                            }
                        }
                    }
                } //end of non-basic input handling
            } // end of the big (if input_words > 0)
            if (valid_input == 0){
                char error_message[30] = "An error has occurred\n";
                write(STDOUT_FILENO, error_message, strlen(error_message));
            }
        } // end of the big for loop
        too_long = 0;
    } // end of big while loop

} // end of main loop