#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "global.h"

// This is the file that you should work on.

// declaration
int execute(struct cmd *cmd);

// name of the program, to be printed in several places
#define NAME "myshell"

// Some helpful functions

void errmsg(char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
}

// apply_redirects() should modify the file descriptors for standard
// input/output/error (0/1/2) of the current process to the files
// whose names are given in cmd->input/output/error.
// append is like output but the file should be extended rather
// than overwritten.

void apply_redirects(struct cmd *cmd)
{
	if (cmd->input || cmd->output || cmd->append || cmd->error)
	{
		errmsg("I do not know how to redirect, please help me!");
		exit(-1);
	}
}

// The function execute() takes a command parsed at the command line.
// The structure of the command is explained in output.c.
// Returns the exit code of the command in question.

int execute(struct cmd *cmd)
{
	pid_t id;
	int ret_code;
	switch (cmd->type)
	{
	case C_PLAIN:
		id = fork();
		if (id)
		{
			int code;
			waitpid(id, &code, 0);
			return code;
		}
		else
			exit(execvp(cmd->args[0], cmd->args));
	case C_SEQ:
		execute(cmd->left);
		return execute(cmd->right);
	case C_AND:
		ret_code = execute(cmd->left);
		if (!ret_code)
			return execute(cmd->right);
		return ret_code;
	case C_OR:
		ret_code = execute(cmd->left);
		if (ret_code)
			return execute(cmd->right);
		return ret_code;
	case C_PIPE:
		errmsg("pipe");
		return -1;
	case C_VOID:
		id = fork();
		if (id)
		{
			int code;
			waitpid(id, &code, 0);
			return code;
		}
		else
			exit(execute(cmd->left));
	}

	// Just to satisfy the compiler
	errmsg("This cannot happen!");
	return -1;
}

int main(int argc, char **argv)
{
	char *prompt = malloc(strlen(NAME) + 3);
	printf("welcome to %s!\n", NAME);
	sprintf(prompt, "%s> ", NAME);

	while (1)
	{
		char *line = readline(prompt);
		if (!line)
			break; // user pressed Ctrl+D; quit shell
		if (!*line)
			continue; // empty line

		add_history(line); // add line to history

		struct cmd *cmd = parser(line);
		if (!cmd)
			continue;	// some parse error occurred; ignore
		output(cmd, 0); // activate this for debugging
		execute(cmd);
	}

	printf("goodbye!\n");
	return 0;
}
