/*
 * parser.h
 *
 * This header file describes the interface of a simple recursive-descent
 * parser that understands a very limited shell language to execute typed
 * in commands.
 *
 * The parser supports:
 * -lists of commands with their arguments separated by semicolons (;)
 * -commands to be executed as foreground or background (&) jobs
 * -input (<) and output (>) redirections from/to a file
 * -commands assembled to pipes (|)
 * -the builtin commands exit, cd [path], jobs, fg [id], bg [id], and
 *  [un]setenv variable [value]
 * -comments (#) that are ignored until end of line
 * -variable substitutions with $variable or ${variable}
 * -quotations with single quotation marks (') protecting enclosed content
 * -the backslash (\) that only protects the following character
 *
 *
 *  Created on: 07.05.2011
 *      Author: Helge Parzyjegla <helge.parzyjegla(at)uni-rostock.de>
 *     Version: 0.1
 */

/**
 * Parser status and global variables.
 */

enum parser_errors
{
	PARSER_OK,                   /* Everything is fine :-).               */
	PARSER_OVERFLOW,             /* Internal buffer overflow.             */
	PARSER_MALLOC,               /* Memory allocation gone wrong.         */
	PARSER_INVALID_STATE,        /* A bug!!! Please file a bug report.    */
	PARSER_BAD_SUBSTITUTION,     /* Illegal variable identifier.          */
	PARSER_MISSING_COMMAND,      /* Well, a command was expected.         */
	PARSER_UNEXPECTED_EOF,       /* EOF while parsing a quotation.        */
	PARSER_ILLEGAL_COMBINATION,  /* Use of builtin commands in pipes.     */
	PARSER_ILLEGAL_REDIRECTION,  /* Illegal redirection in pipes.         */
	PARSER_MISSING_ARGUMENT,     /* Missing argument for builtin command. */
	PARSER_ILLEGAL_ARGUMENT,     /* Illegal argument for builtin command. */
	PARSER_MISSING_FILE          /* Missing file for redirection.         */
};

extern enum  parser_errors parser_status; /* parser status                */
extern char* parser_message;              /* human readable error message */
extern int   error_line;                  /* line number of error         */
extern int   error_column;                /* column number of error       */


/**
 * Data structures representing a parsed command line.
 */

typedef struct cd_args  /* arguments for builtin command 'cd'             */
{
	char* path;         /* directory path (might be NULL if not given)    */
} cd_args;

typedef struct env_args /* arguments of builtin environment commands      */
{                       /* [un]set variable [value]                       */
	char* name;         /* environment variable to set/delete (not NULL)  */
	char* value;        /* value to set (is NULL in unset command)        */
} env_args;

enum job_kind           /* types of job control commands                  */
{
	INFO,               /* just info about a job                          */
	BG,                 /* continue (last) suspended job in background    */
	FG                  /* execute (last) job in foreground               */
};

typedef struct job_args /* arguments of builtin job control               */
{                       /* jobs [id] and bg [id] and fg [id]              */
	enum job_kind kind; /* kind of control request (info/bg/fg)           */
	int id;             /* job id (is -1 if no id was supplied)           */
	env_args *foo;
} job_args;

typedef struct prog_args    /* arguments of an external command           */
{                           /* program arg1 arg2 ...                      */
	char* input;            /* input redirection from file (might be NULL)*/
	char* output;           /* output redirection to file (might be NULL) */
	int background;         /* execute in background when true            */
	int argc;               /* elements in argument vector (>=1)          */
	char** argv;            /* argument vector                            */
	struct prog_args* next; /* next command when in pipe (NULL otherwise) */
} prog_args;

enum cmd_kind  /* type of command (internal, external, or in a pipe)      */
{
	EXIT,      /* builtin 'exit'                                          */
	CD,        /* builtin 'cd [path]'                                     */
	ENV,       /* builtin '[un]set variable [value]'                      */
	JOB,       /* builtin 'jobs [id]', 'bg [id], and fg [id]'             */
	PROG,      /* external command/program                                */
	PIPE       /* external commands in a pipe                             */
};

typedef struct cmds     /* structure representing a list of parsed        */
{                       /* commands                                       */
	enum cmd_kind kind; /* type of command (see above)                    */
	union               /* with following arguments:                      */
	{                   /* exit needs no further arguments                */
		cd_args  cd;    /* path for cd                                    */
		env_args env;   /* variable name and value                        */
		job_args job;   /* job id and request type                        */
		prog_args prog; /* program and its arguments (for PROG and PIPE)  */
	};
	struct cmds *next;  /* next command in list                           */
} cmds;


/**
 * Parser functions.
 */

/*
 * Parses a line of input and returns a handle to the list of parsed
 * commands. See data structure above for details on the command list.
 * The function may return NULL in two cases: the input line contained no
 * command or an error occurred while parsing the command. Use variable
 * parser_status to distinguish both cases. If input was empty the parser
 * is fine (status: PARSER_OK), otherwise parser_status carries an error
 * number and parser_message a short error description for diagnostics.
 */
extern cmds* parser_parse(char* input);

/*
 * Frees a parsed command list if it is not longer needed by the shell. If
 * handle is NULL nothing happens.
 */
extern void parser_free(cmds* handle);

/*
 * Visualizes/prints a parsed command list supplied by handle.
 */
extern void parser_print(cmds* handle);

/*
 * Tests the parser using input and prints the results. Please use the
 * output of this function to file a bug report.
 */
extern void parser_test(char* input);
