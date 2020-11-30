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
	if (cmd->input)
	{
		int fd;
		if ((fd = open(cmd->input, O_DSYNC | O_RDONLY)) == -1)
			errmsg("Could not open input file");
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
	if (cmd->output)
	{
		int fd;
		if ((fd = open(cmd->output, O_DSYNC | O_CREAT | O_WRONLY | O_TRUNC)) == -1)
			errmsg("Could not open/create output file");
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
	if (cmd->error)
	{
		int fd;
		if ((fd = open(cmd->error, O_DSYNC | O_CREAT | O_WRONLY | O_TRUNC)) == -1)
			errmsg("Could not open/create error file");
		dup2(fd, STDERR_FILENO);
		close(fd);
	}
	if (cmd->append)
	{
		int fd;
		if ((fd = open(cmd->append, O_DSYNC | O_CREAT | O_WRONLY | O_APPEND)) == -1)
			errmsg("Could not open/create output file");
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
}

// exec_pipe take a command of type C_PIPE as input
// and redirect the output of the left command to the input of the right command
// If the first command fails it returns its code else it returns the coe of the right command
int exec_pipe(struct cmd *cmd)
{
	int p[2];
	if (pipe(p) == -1)
		perror("Pipe creation failed");
	pid_t idleft = fork();
	if (idleft == 0)
	{
		dup2(p[1], STDOUT_FILENO);
		close(p[0]);
		close(p[1]);
		exit(execute(cmd->left));
	}
	pid_t idright = fork();
	if (idright == 0)
	{
		dup2(p[0], STDIN_FILENO);
		close(p[0]);
		close(p[1]);
		exit(execute(cmd->right));
	}
	close(p[0]);
	close(p[1]);
	int codeleft, coderight;
	waitpid(idleft, &codeleft, 0);
	if (codeleft)
		kill(idright, SIGTERM);
	waitpid(idright, &coderight, 0);
	return (coderight);
}

// The function execute() takes a command parsed at the command line.
// The structure of the command is explained in output.c.
// Returns the exit code of the command in question.

int execute(struct cmd *cmd)
{
	pid_t id;
	id = fork();
	if (id)
	{
		int code;
		waitpid(id, &code, 0);
		return code;
	}
	else
	{
		apply_redirects(cmd);
		int ret_code;
		signal(SIGINT, SIG_DFL);
		switch (cmd->type)
		{
		case C_PLAIN:
			exit(execvp(cmd->args[0], cmd->args));
		case C_SEQ:
			execute(cmd->left);
			exit(execute(cmd->right));
		case C_AND:
			ret_code = execute(cmd->left);
			if (!ret_code)
				exit(execute(cmd->right));
			exit(ret_code);
		case C_OR:
			ret_code = execute(cmd->left);
			if (ret_code)
				exit(execute(cmd->right));
			exit(ret_code);
		case C_PIPE:
			exit(exec_pipe(cmd));
		case C_VOID:
			exit(execute(cmd->left));
		}
	}

	// Just to satisfy the compiler
	errmsg("This cannot happen!");
	return -1;
}

int main(int argc, char **argv)
{
	signal(SIGINT, SIG_IGN);
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
