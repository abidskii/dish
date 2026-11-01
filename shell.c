#include "shell.h"
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char* builtins[] = { "cd", "exit" };
int (*builtins_func[])(char**) = { &cd, &dish_exit };

int num_builtins() { return sizeof(builtins) / sizeof(char*); }

int cd(char** args) {

	if (args[1] == NULL) {
		fprintf(stderr, "expected argument to \"cd\"\n");
		return 1;
	}

	int res = chdir(args[1]);
	if (res != 0) {
		perror("dish");
	}
	return 1;
}

int dish_exit(char** args) {
	write(STDOUT_FILENO, "\n", 1);
	return 0;
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

		char*** temp = calloc(3, sizeof(char**));
		int flags;

		if (extract_redirects(args, temp, &flags) == -1) {

			return 1;
		}
		apply_redirects(temp, &flags);
	}
	return 1;
}

int exec(char** args) {

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

		while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
			;
	} else {
		perror("dish");
	}
	return 1;
}

int handle_delim(char** args, int* size, char* delim) {

	char*** commands = calloc(*size, sizeof(char**));
	for (int k = 0; k < *size; k++) {
		commands[k] = calloc((*size + 1), sizeof(char*));
	}

	int splits_found = 0;

	// Splits output
	int split_index = 0;

	for (int i = 0; i < *size; i++) {

		if (strcmp(args[i], delim) == 0) {
			splits_found += 1;
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

int extract_redirects(char** command, char*** args, int* flags) {

	int size = 0;
	while (command[size] != NULL)
		size++;

	int com = 0;
	int in = 1;
	int out = 2;
	*flags = 0;

	if (size == -1) {
		return 0;
	}

	for (int k = 0; k < 3; k++) {
		args[k] = calloc((size + 1), sizeof(char*)); // room for *size words + NULL
	}
	int second_dimension_pointer = 0;

	for (int i = 0; i < size; i++) {

		if (strcmp(command[i], "<") == 0) {

			if (command[i + 1] == NULL) {
				fprintf(stderr, "dish: expected a filename after \"%s\"\n", command[i]);
				return -1; // or free what you allocated and return
			}
			args[in][0] = command[i + 1];
			i++;
			continue;

		} else if (strcmp(command[i], ">") == 0) {

			if (command[i + 1] == NULL) {
				fprintf(stderr, "dish: expected a filename after \"%s\"\n", command[i]);
				return -1; // or free what you allocated and return
			}
			args[out][0] = command[i + 1];
			i++;
			*flags = O_WRONLY | O_CREAT | O_TRUNC;
			continue;

		} else if (strcmp(command[i], ">>") == 0) {

			if (command[i + 1] == NULL) {
				fprintf(stderr, "dish: expected a filename after \"%s\"\n", command[i]);
				return -1; // or free what you allocated and return
			}
			args[out][0] = command[i + 1];
			i++;
			*flags = O_WRONLY | O_CREAT | O_APPEND;
			continue;
		}

		args[com][second_dimension_pointer] = command[i];
		second_dimension_pointer++;
	}

	return 0;
}

int apply_redirects(char*** commands, int* flags) {

	int fd[2];

	char** out_file = commands[2];

	char** in_file = commands[1];

	int descriptor_in = -1;
	int descriptor_out = -1;

	if (commands[0][0] == NULL) {
		fprintf(stderr, "dish: missing command\n");
		return 1;
	}

	if (in_file[0] != NULL) {
		descriptor_in = open(in_file[0], O_RDONLY);
		if (descriptor_in == -1) {
			perror(in_file[0]);
			return 1;
		}
	}

	if (out_file[0] != NULL) {
		descriptor_out = open(out_file[0], *flags, 0644);
		if (descriptor_out == -1) {
			perror(out_file[0]);
			if (descriptor_in != -1)
				close(descriptor_in);
			return 1;
		}
	}

	pid_t pid;
	pid = fork();
	if (pid == 0) {

		if (descriptor_in != -1) {
			dup2(descriptor_in, STDIN_FILENO);
			close(descriptor_in);
		}
		if (descriptor_out != -1) {
			dup2(descriptor_out, STDOUT_FILENO);
			close(descriptor_out);
		}

		execvp(commands[0][0], commands[0]);
		perror("dish");
		exit(EXIT_FAILURE);

	} else {
		waitpid(pid, NULL, 0);
		if (descriptor_in != -1)
			close(descriptor_in);
		if (descriptor_out != -1)
			close(descriptor_out);
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

	for (int i = 0; i < first_size; i++) {

		if (pipe(fd) != 0) {
			perror("dish");
		}

		// Check for redirect
		char*** args = calloc(3, sizeof(char**));
		int flags;

		if (extract_redirects(commands[i], args, &flags) == 1) {
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
			perror("dish");
			exit(EXIT_FAILURE);
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
		while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR)
			;
	}

	return 0;
}

char** parse(char* input, int* size) {
	int tok_bufsize = TOK_BUFSIZE;
	char** args = calloc(TOK_BUFSIZE, sizeof(char*));
	char* token = strtok(input, TOK_DELIM);
	int index = 0;

	while (token != NULL) {
		if ((index >= tok_bufsize - 1)) {
			tok_bufsize += tok_bufsize;
			args = realloc(args, tok_bufsize * sizeof(char*));
		}
		args[index] = token;
		token = strtok(NULL, TOK_DELIM);
		index++;
	}

	args[index] = NULL;
	*size = index;

	return args;
}

char* readline() {

	int ch;
	int i = 0;
	int buffer_size = READ_SIZE;
	char* buffer = calloc(buffer_size, (sizeof(char)));

	printf(" > ");

	while (1) {

		errno = 0;
		ch = getchar();

		if (ch == EOF && errno == EINTR) {
			clearerr(stdin);
			i = 0;
			break;
		}
		if (ch == EOF && i == 0) {
			free(buffer);
			return NULL;
		}
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

int dish() {

	char* input;
	char** args;
	int args_num;

	input = readline();

	if (input == NULL) {
		putchar('\n');
		return 0;
	}

	args = parse(input, &args_num);
	if (args_num > 0) {
		if (run(args, &args_num) == 0) {
			return 0;
		}
	}

	free(input);
	free(args);

	return 1;
}

void on_sigint(int sig) {
	(void)sig;
	write(STDOUT_FILENO, "\n", 1);
}

int main(int argc, char* argv[]) {

	struct sigaction sa;
	sa.sa_handler = on_sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sa.sa_handler = on_sigint;

	while (dish()) {
	}

	return EXIT_SUCCESS;
}
