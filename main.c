// Written by Thomas MICHEL
// Exercice/DM d'architecture et système

// New functions and builtin functions are at the end
// (I did not create a new file because I didn't know if we were allowed to change the structure of the project)

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>

#include "global.h"

// declaration
int execute(struct cmd *cmd);
int exec_pipe(struct cmd *cmd);
int ls(char *argv[]);
int cat(char *argv[]);
int cd(char *argv[]);

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
		if ((fd = open(cmd->input, O_DSYNC | O_RDONLY)) == -1) // Open the file
			errmsg("Could not open input file");
		dup2(fd, STDIN_FILENO); // Overwrite stdin
		close(fd);
	}
	if (cmd->output)
	{
		int fd;
		if ((fd = open(cmd->output, O_DSYNC | O_CREAT | O_WRONLY | O_TRUNC)) == -1) // Open the file
			errmsg("Could not open/create output file");
		dup2(fd, STDOUT_FILENO); // Overwrite stdout
		close(fd);
	}
	if (cmd->error)
	{
		int fd;
		if ((fd = open(cmd->error, O_DSYNC | O_CREAT | O_WRONLY | O_TRUNC)) == -1) // Open the file
			errmsg("Could not open/create error file");
		dup2(fd, STDERR_FILENO); // Overwrite stderr
		close(fd);
	}
	if (cmd->append)
	{
		int fd;
		if ((fd = open(cmd->append, O_DSYNC | O_CREAT | O_WRONLY | O_APPEND)) == -1) // Open the file in 'append' mode
			errmsg("Could not open/create output file");
		dup2(fd, STDOUT_FILENO); // Overwrite stdout
		close(fd);
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
		if (strcmp(cmd->args[0], "cd") == 0) // Need to change the directory of the parent process -> executes outside the fork
			return (cd(cmd->args));
		id = fork(); // Create child process which input and output can be overwrited
		if (id)
		{
			int code;
			waitpid(id, &code, 0);
			return code;
		}
		else
		{
			signal(SIGINT, SIG_DFL); //Restore signals for child process
			signal(SIGQUIT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);
			signal(SIGTTIN, SIG_DFL);
			signal(SIGTTOU, SIG_DFL);
			apply_redirects(cmd);				  // Redirection
			if (strcmp(cmd->args[0], "cat") == 0) // built in functions
				exit(cat(cmd->args));
			if (strcmp(cmd->args[0], "ls") == 0)
				exit(ls(cmd->args));
			exit(execvp(cmd->args[0], cmd->args));
		}
	case C_SEQ:
		execute(cmd->left);
		return (execute(cmd->right));
	case C_AND:
		ret_code = execute(cmd->left);
		if (!ret_code)
			return (execute(cmd->right));
		return (ret_code);
	case C_OR:
		ret_code = execute(cmd->left);
		if (ret_code)
			return (execute(cmd->right));
		return (ret_code);
	case C_PIPE:
		return (exec_pipe(cmd));
	case C_VOID:
		id = fork();
		if (id)
		{
			int code;
			waitpid(id, &code, 0);
			return code;
		}
		else
		{
			exit(execute(cmd->left)); // Subshell in a child process so functions such as "cd" can't affect the main shell
		}
	}

	// Just to satisfy the compiler
	errmsg("This cannot happen!");
	return -1;
}

int main(int argc, char **argv)
{
	signal(SIGINT, SIG_IGN); // Neutralize signals
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
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

// exec_pipe take a command of type C_PIPE as input
// and redirect the output of the left command to the input of the right command
// If the first command fails it returns its code else it returns the coe of the right command
int exec_pipe(struct cmd *cmd)
{
	int p[2];
	if (pipe(p) == -1)
		perror("Pipe creation failed");
	pid_t idleft = fork();
	if (idleft == 0) // Execute left command
	{
		dup2(p[1], STDOUT_FILENO); // Redirection of left output to right input
		close(p[0]);
		close(p[1]);
		exit(execute(cmd->left));
	}
	pid_t idright = fork();
	if (idright == 0) // Execute left command
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
	if (codeleft) // If left command failed
		kill(idright, SIGTERM);
	waitpid(idright, &coderight, 0);
	return (coderight);
}

// List directory content
int ls(char *argv[])
{
	struct dirent **namelist;
	int n;
	if (argv[0] == NULL) // invalid command
	{
		return -1;
	}
	else if (argv[1] == NULL) // ls without arguments
	{
		n = scandir(".", &namelist, NULL, alphasort);
	}
	else
	{
		n = scandir(argv[1], &namelist, NULL, alphasort);
	}
	// printf("%d\n", n);  // Number of files and directories
	if (n < 0)
	{
		return -1;
	}
	else
	{
		for (int i = 0; i < n; i++)
		{
			printf("%s\n", namelist[i]->d_name);
			free(namelist[i]);
		}
		free(namelist);
	}
	fflush(stdout);
	return 0;
}

// concatenate files and print on the standard output
int cat(char *argv[])
{
	int i, c;
	i = 1;
	while (argv[i] != NULL)
	{
		FILE *f;
		f = fopen(argv[i], "r");
		while ((c = fgetc(f)) != EOF)
			fputc(c, stdout);
		fclose(f);
		i++;
	}
	fputc('\n', stdout); // Just to print the prompt on a new line
	fflush(stdout);
	return 0;
}

// change the working directory
int cd(char *argv[])
{
	if (argv[1] != NULL)
	{
		if (chdir(argv[1]) != 0)
		{
			errmsg("Could not change the current directory");
			return -1;
		}
	}
	return 0;
}
