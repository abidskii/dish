#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char* builtins[] = { "cd" };
int (*builtins_func[])(char**) = { &cd };

int num_builtins() { return sizeof(builtins) / sizeof(char*); }

int cd(char** args) {

	if (args[1] == NULL) {
		fprintf(stderr, "expected argument to \"cd\"\n");
	}

	int res = chdir(args[1]);
	if (res != 0) {
		perror("dish");
	}
	return 1;
}
int run(char** args, int* size) {

	// Check for built in commands
	for (int i = 0; i < num_builtins(); i++) {
		if (strcmp(args[0], builtins[i]) == 0) {
			return (*builtins_func[i])(args);
		}
	}

	// Check for pipes
	if (handle_delim(args, size, "|") == 0) {
		exec(args);
	}
	return 1;
}

int exec(char** args) {

	if (handle_redirect(args) == 0) {
		pid_t pid = fork();
		pid_t wpid;
		int status;
		if (pid == 0) {
			if (execvp(args[0], args) == -1) {
				perror("dish");
			}
			exit(EXIT_FAILURE);

		} else if (pid > 0) {
			// Parent process
			do {
				wpid = waitpid(pid, &status, WUNTRACED);
			} while (!WIFEXITED(status) && !WIFSIGNALED(status));

		} else {
			perror("dish");
		}
		return 1;
	}
	return 1;
}

int handle_delim(char** args, int* size, char* delim) {

	char*** commands = malloc(*size * sizeof(char**));
	for (int k = 0; k < *size; k++) {
		commands[k] = malloc((*size + 1) * sizeof(char*)); // room for *size words + NULL
	}

	int* pipes = malloc(0);
	int splits_found = 0;
	char* pipe_output;

	// Splits output
	char* output = NULL;
	char buf[256];
	ssize_t prev_size = 0;
	ssize_t n;
	int split_index = 0;

	for (int i = 0; i < *size; i++) {

		if (strcmp(args[i], delim) == 0) {
			splits_found += 1;
			pipes = realloc(pipes, splits_found * sizeof(int));
			pipes[i - 1] = i;
			split_index = 0;
			continue;
		}
		commands[splits_found][split_index] = args[i];
		//		printf("%s ", commands[splits_found][split_index]);
		//		printf("%d %d\n", splits_found, split_index);
		split_index++;
	}

	if (splits_found == 0) {
		return 0;
	}

	if (*delim == '|') {
		exec_pip(commands, splits_found + 1, split_index, splits_found);
	}

	return 1;
}

int exec_redir(char*** commands, int first_size, int splits_found, char** delims) {

	int fd[2];

	pipe(fd);

	int descriptor = open(commands[1][0], O_WRONLY | O_CREAT | O_TRUNC, 0644);

	int fd2;

	if (strcmp(delims[0], ">")) {
		fd2 = STDOUT_FILENO;
	} else if (strcmp(delims[0], "<")) {
		fd2 = STDIN_FILENO;
	} else if (strcmp(delims[0], ">>")) {
		fd2 = STDIN_FILENO;
	}

	dup2(descriptor, fd2);

	return 1;
}

int find_size_and_check_for_redirect(char** command) {

	int i = 0;

	int redirects_found = 0;

	while (command[i] != NULL) {

		if (strcmp(command[i], "<") == 0) {

			redirects_found++;

		} else if (strcmp(command[i], ">") == 0) {

			redirects_found++;

		} else if (strcmp(command[i], ">>") == 0) {

			redirects_found++;
		}
		i++;
	}

	if (redirects_found == 0) {
		return -1;
	}

	return i;
}

int handle_redirect(char** command) {

	// Create a array of arguments that is split by redirets

	int size = find_size_and_check_for_redirect(command);

	if (size == -1) {
		return 0;
	}
	printf("%d", size);

	char*** args = malloc(size * sizeof(char**));

	for (int k = 0; k < size; k++) {
		args[k] = malloc((size + 1) * sizeof(char*)); // room for *size words + NULL
	}
	char** redirects = malloc(0);
	int redirects_found = 0;
	int second_dimension_pointer = 0;

	for (int i = 0; i < size; i++) {

		if (strcmp(command[i], "<") == 0) {

			redirects_found++;
			redirects = realloc(redirects, redirects_found * sizeof(char*));
			redirects[redirects_found - 1] = command[i];
			second_dimension_pointer = 0;
			args[redirects_found][second_dimension_pointer] = command[i];
			continue;

		} else if (strcmp(command[i], ">") == 0) {

			redirects_found++;
			redirects = realloc(redirects, redirects_found * sizeof(char*));
			redirects[redirects_found - 1] = command[i];
			second_dimension_pointer = 0;
			args[redirects_found][second_dimension_pointer] = command[i];
			continue;

		} else if (strcmp(command[i], ">>") == 0) {

			redirects_found++;
			redirects = realloc(redirects, redirects_found * sizeof(char*));
			redirects[redirects_found - 1] = command[i];
			second_dimension_pointer = 0;
			args[redirects_found][second_dimension_pointer] = command[i];
			continue;
		}

		args[redirects_found][second_dimension_pointer] = command[i];
		second_dimension_pointer++;
	}

	for (int i = 0; i < redirects_found; i++) {
		printf("redirects[%d] = %s\n", i, redirects[i]);
	}

	return 1;
}

int exec_pip(char*** commands, int first_size, int second_size, int splits_found) {

	int fd[2];
	int prev_read = -1;

	int status;

	pid_t wpid;
	pid_t pid;
	pid_t pids[first_size];

	if (pipe(fd) != 0) {
		perror("dish");
	}

	for (int i = 0; i < first_size; i++) {

		if (pipe(fd) != 0) {
			perror("dish");
		}

		// Check for redirect

		if (handle_redirect(commands[i]) == 1) {
			continue;
		}

		pid = fork();

		if (pid == 0) {
			if (i == 0) {
				dup2(fd[1], STDOUT_FILENO);
			} else if (i == first_size - 1) {
				dup2(prev_read, STDIN_FILENO);
			} else {

				dup2(prev_read, STDIN_FILENO);
				dup2(fd[1], STDOUT_FILENO);
			}

			close(fd[0]);
			close(fd[1]);
			execvp(commands[i][0], commands[i]);
		}
		if (prev_read != -1) {
			close(prev_read);
		}

		if (i != first_size - 1) {
			prev_read = fd[0];
			close(fd[1]);
		} else {
			close(fd[0]);
			close(fd[1]);
		}
		pids[i] = pid;
	}
	close(fd[0]);
	close(fd[1]);
	for (int i = 0; i < first_size; i++) {
		waitpid(pids[i], NULL, 0);
	}
	return 0;
}

char** parse(char* input, int* size) {
	int tok_bufsize = TOK_BUFSIZE;
	char** args = malloc(TOK_BUFSIZE * sizeof(char*));
	char* token = strtok(input, TOK_DELIM);
	int index = 0;

	while (token != NULL) {
		if (index > tok_bufsize) {
			tok_bufsize += tok_bufsize;
			args = realloc(args, tok_bufsize * sizeof(char*));
		}
		args[index] = token;
		token = strtok(NULL, " ");
		index++;
	}

	*size = index;

	return args;
}

char* readline() {

	int ch;
	int i = 0;
	int buffer_size = READ_SIZE;
	char* buffer = malloc(buffer_size * (sizeof(char)));

	printf(" > ");

	while (1) {

		ch = getchar();

		if (i > buffer_size - 1) {
			buffer_size += READ_SIZE;
			buffer = realloc(buffer, buffer_size);
		}
		if (ch == EOF || ch == '\n') {
			break;
		}

		buffer[i] = ch;
		i++;
	}
	buffer[i] = '\0';
	return buffer;
}

void dish() {

	char* input;
	char** args;
	int args_num;

	input = readline();

	args = parse(input, &args_num);

	run(args, &args_num);

	free(input);
	free(args);
}

int main(int argc, char* argv[]) {

	while (1) {
		dish();
	}

	return EXIT_SUCCESS;
}
