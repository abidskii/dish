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

	handle_delim(args, size, "|");

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

	exec_pip(commands, splits_found + 1);

	for (int k = 0; k < *size; k++) {
		free(commands[k]);
	}
	free(commands);
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
		args[k] = calloc((size + 1), sizeof(char*));
		if (!args[k]) {
			for (int j = 0; j < k; j++) {
				free(args[j]);
				args[j] = NULL;
			}
			return -1;
		}
	}
	int second_dimension_pointer = 0;

	for (int i = 0; i < size; i++) {

		if (strcmp(command[i], "<") == 0) {

			if (command[i + 1] == NULL) {
				fprintf(stderr, "dish: expected a filename after \"%s\"\n", command[i]);
				goto fail;
			}
			args[in][0] = command[i + 1];
			i++;
			continue;

		} else if (strcmp(command[i], ">") == 0) {

			if (command[i + 1] == NULL) {
				fprintf(stderr, "dish: expected a filename after \"%s\"\n", command[i]);
				goto fail;
			}
			args[out][0] = command[i + 1];
			i++;
			*flags = O_WRONLY | O_CREAT | O_TRUNC;
			continue;

		} else if (strcmp(command[i], ">>") == 0) {

			if (command[i + 1] == NULL) {
				fprintf(stderr, "dish: expected a filename after \"%s\"\n", command[i]);
				goto fail;
			}
			args[out][0] = command[i + 1];
			i++;
			*flags = O_WRONLY | O_CREAT | O_APPEND;
			continue;
		}

		args[com][second_dimension_pointer] = command[i];
		second_dimension_pointer++;
	}
	args[com][second_dimension_pointer] = NULL;
	return 0;
fail:
	for (int k = 0; k < 3; k++) {
		free(args[k]);
		args[k] = NULL;
	}
	return -1;
}

void redirect_in_child(char*** rows, int out_flags) {

	if (rows[1][0] != NULL) {
		int f = open(rows[1][0], O_RDONLY);
		if (f == -1) {
			perror(rows[1][0]);
			exit(EXIT_FAILURE);
		}
		dup2(f, STDIN_FILENO);
		close(f);
	}

	if (rows[2][0] != NULL) {
		int f = open(rows[2][0], out_flags, 0644);
		if (f == -1) {
			perror(rows[2][0]);
			exit(EXIT_FAILURE);
		}
		dup2(f, STDOUT_FILENO);
		close(f);
	}
}

int exec_pip(char*** commands, int first_size) {

	int prev_read = -1;

	int started = 0;

	pid_t pids[first_size];

	char*** rows[first_size];
	int flags[first_size];

	for (int i = 0; i < first_size; i++) {
		rows[i] = calloc(3, sizeof(char**));
		flags[i] = 0;
		if (extract_redirects(commands[i], rows[i], &flags[i]) == -1) {
			for (int j = 0; j <= i; j++) {
				if (rows[j]) {
					free(rows[j][0]);
					free(rows[j][1]);
					free(rows[j][2]);
					free(rows[j]);
				}
			}
			return 1;
		}
		if (rows[i][0][0] == NULL) {
			fprintf(stderr, "dish: missing command\n");
			for (int j = 0; j <= i; j++) {
				free(rows[j][0]);
				free(rows[j][1]);
				free(rows[j][2]);
				free(rows[j]);
			}
			return 1;
		}
	}

	for (int i = 0; i < first_size; i++) {
		int last = (i == first_size - 1);
		int fd[2];

		if (!last && pipe(fd) != 0) {
			perror("dish");
			break;
		}

		pid_t pid = fork();

		if (pid == 0) {
			if (prev_read != -1) {
				dup2(prev_read, STDIN_FILENO);
				close(prev_read);
			}
			if (!last) {
				dup2(fd[1], STDOUT_FILENO);
				close(fd[0]);
				close(fd[1]);
			}

			redirect_in_child(rows[i], flags[i]);

			execvp(rows[i][0][0], rows[i][0]);
			perror("dish");
			exit(EXIT_FAILURE);
		}

		pids[started++] = pid;

		if (prev_read != -1)
			close(prev_read);
		if (!last) {
			close(fd[1]);
			prev_read = fd[0];
		}
	}
	for (int i = 0; i < started; i++) {
		while (waitpid(pids[i], NULL, 0) == -1 && errno == EINTR)
			;
	}

	for (int i = 0; i < first_size; i++) {
		free(rows[i][0]); // command row
		free(rows[i][1]); // input file row
		free(rows[i][2]); // output file row
		free(rows[i]); // the array holding the three row pointers
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
	fflush(stdout);

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
	int keep_going = 1;

	if (args_num > 0) {
		keep_going = run(args, &args_num);
	}

	free(input);
	free(args);

	return keep_going;
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
