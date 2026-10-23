#ifndef SHELL_H
#define SHELL_H

#define READ_SIZE 1024
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\r\n\a"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


int dish();
char * readline(void);
char ** parse(char*,int * num );
int exec (char **);
int run (char **,int *size);
int cd (char **);
int num_builtins();
int handle_delim(char **, int * num, char *delim);
int exec_pip(char*** commands, int first_size, int second_size, int splits_found);
int exec_redir(char*** commands, int first_size, int splits_found, char ** delims);
int handle_redirect(char ** );
int find_size_and_check_for_redirect(char **);
void on_sigint(int sig);
int dish_exit(char** args);




#endif
