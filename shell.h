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
int exec_pip(char*** commands, int first_size);
int extract_redirects(char ** , char *** args, int * flags);
int find_size_and_check_for_redirect(char **);
void on_sigint(int sig);
void redirect_in_child(char*** rows, int out_flags);
int dish_exit(char** args);




#endif
