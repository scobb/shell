#include <stdlib.h>
#include <stdio.h>
void shell_loop();
char* shell_read_line();

int main(int argc, char **argv) {

	shell_loop();

	return 0;
}

void shell_loop() {
	char* line;
	char** args;
	int status;
	do {
		printf("> ");
		line = shell_read_line();
		printf("%s\n", line);
		/*args = shell_split_line(line);
		status = shell_execute(args);
		free(line);
		free(args);*/

	} while (status);
}

#define SHELL_RL_BUFSIZE 1024
char* shell_read_line() {
	int bufsize = SHELL_RL_BUFSIZE;
	int position = 0;
	char* buf = malloc(sizeof(char) * bufsize);
	int c;

	if (!buf) {
		fprintf(stderr, "Shell: allocation error\n");
		exit(1);
	}

	while (1) {
		c = getchar();

		if (c == EOF || c == '\n') {
			buf[position] = 0;
			return buf;
		}
		else {
			buf[position++] = c;
		}

		if (position >= bufsize) {
			bufsize += SHELL_RL_BUFSIZE;
			buf = realloc(buf, bufsize);
			if (!buf) {
				fprintf(stderr, "Shell: allocation error\n");
				exit(1);
			}
		}
	}
}
