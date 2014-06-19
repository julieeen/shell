/*
 * parser.c
 *
 * This is a straight-forward, recursive-descent parser (including a limited
 * scanner) that understands a simple shell language. This software should
 * be considered as an alpha version that certainly includes bugs. If you
 * find one please do not hesitate to file a bug report.
 *
 *
 *  Created on: 05.05.2011
 *      Author: Helge Parzyjegla <helge.parzyjegla(at)uni-rostock.de>
 *     Version: 0.1
 */

//#include <malloc.h>   /* dynamic memory management                       */
#include <setjmp.h>   /* longjumps to simplify error handling            */
#include <stdbool.h>  /* constants                                       */
#include <stdio.h>    /* I/O                                             */
#include <stdlib.h>   /* standard c functions                            */
#include <string.h>   /* string manipulations                            */

#include "Parser.h"

#define MAX_LINE_LENGTH (2048)  /* maximum length of a command with args */
#define NEW_ARGV_LENGTH (10)    /* initial size of the arg vector        */


/* parser status ------------------------------------------------------- */
/* --------------------------------------------------------------------- */

char *messages[] =                 /* human readable error messages      */
{
	"Ok.",
	"Overflow of internal buffer.",
	"Insufficient memory.",
	"Invalid parser state.",
	"Bad variable substitution.",
	"Missing command.",
	"Unexpected end of input stream.",
	"Illegal combination of internal and external commands.",
	"Illegal input/output redirection in pipe.",
	"Missing argument for builtin command.",
	"Illegal argument for builtin command.",
	"Missing file for input or output redirection."
};

enum parser_errors parser_status;  /* parser status                      */
char* parser_message;              /* human readable error message       */
int error_line;                    /* line number of error               */
int error_column;                  /* column number of error             */


/* scanner structures -------------------------------------------------- */
/* --------------------------------------------------------------------- */

/* scanned tokens for parser                                             */
enum token_kind
{
	UNKNOWN,                       /* should never be the result         */
	END,                           /* EOF discovered                     */
	AMP,                           /* &-token                            */
	IN,                            /* <-token                            */
	OUT,                           /* >-token                            */
	STROKE,                        /* |-token                            */
	SEP,                           /* ;-token                            */
	IDE,                           /* token for identifiers (commands)   */
	REM                            /* #-token                            */
};

/* token representation                                                  */
typedef struct token
{
	enum token_kind kind;
	char arg[MAX_LINE_LENGTH];
} token;

/* variable buffer                                                       */
typedef struct buffer
{
	char var[MAX_LINE_LENGTH];
} buffer;


/* internal global state ----------------------------------------------- */
/* --------------------------------------------------------------------- */

static cmds* root;       /* root pointer of command list                 */
static int line;         /* current line scanned and parsed              */
static int col;          /* current column scanned and parsed            */

static char* stream;     /* the stream to parse                          */
static token lookahead;  /* last parsed token                            */
static buffer varbuf;    /* buffer used for variable substitutions       */
static int arg_pos;      /* position in argument buffer                  */
static int var_pos;      /* position in variable buffer                  */


/* freeing memory ------------------------------------------------------ */
/* --------------------------------------------------------------------- */

static void argv_free(prog_args* prog)
{
	int i;
	for (i=0; i<prog->argc; i++)
	{
		free(prog->argv[i]);
	}
	free(prog->argv);
	prog->argv=NULL;
	prog->argc=0;
}

static void prog_free(prog_args* prog)
{
	/* start freeing at end of the list                                  */
	if (prog->next != NULL)
	{
		prog_free(prog->next);
	}
	/* free input, output files and itself                               */
	argv_free(prog);
	free(prog->input);
	free(prog->output);
	free(prog);
}

/* Frees a list of commands.                                             */
void parser_free(cmds* cmd)
{
	/* start freeing at end of list                                      */
	if (cmd==NULL)
	{
		return;
	}
	else
	{
		parser_free(cmd->next);
	}
	/* free each type separately                                         */
	switch (cmd->kind)
	{
	case EXIT :
	case JOB :
		break;
	case CD :
		free(cmd->cd.path);
		break;
	case ENV :
		free(cmd->env.name);
		free(cmd->env.value);
		break;
	case PROG : case PIPE :
		if (cmd->prog.next != NULL)
		{
			prog_free(cmd->prog.next);
			argv_free(&(cmd->prog));
			free(cmd->prog.input);
			free(cmd->prog.output);
		}
		break;
	}
	/* finally free command item                                         */
	free(cmd);
}


/* error handling ------------------------------------------------------ */
/* --------------------------------------------------------------------- */

jmp_buf error_env;                  /* environment buffer for long jumps */

/* Sets error code and message. Returns from the deepest recursion using */
/* a long jump.                                                          */
static void raise_error(enum parser_errors code)
{
	parser_status = code;
	parser_message = messages[code];
	if (code>=PARSER_INVALID_STATE)
	{
		error_line=line;
		error_column=col;
	}
	/* cleanup before returning                                          */
	parser_free(root);
	longjmp(error_env,1);
}


/* scanner ------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

/* read the next token from input stream                                */
static void read()
{
	/* initialization                                                   */
	int quote = false;      /* set when scanning a quotation 'quote'    */
	int backspace = false;  /* set when scanning an escaped char \e     */
	int comment = false;    /* set when scanning a comment #comment     */
	int ide = false;        /* set when scanning an identifier          */
	int variable = false;   /* set when scanning a variable $name       */
	int bracket = false;    /* set when scanning a variable ${name}     */
    /* argument and variable buffers                                    */
	char* arg = lookahead.arg;
	char* var = varbuf.var;
	char* value = NULL;
	arg_pos = 0;
	var_pos = 0;
	lookahead.kind=UNKNOWN;

	/* read chars from stream                                           */
	for (; ; stream++, col++)
	{
		/* check for overflow                                           */
		if (arg_pos>=MAX_LINE_LENGTH-1 || var_pos>=MAX_LINE_LENGTH-1)
		{
			raise_error(PARSER_OVERFLOW);
		}
		/* gobble comments                                              */
		if (comment)
		{
			if(*stream=='\n' || *stream=='\0')
			{
				*arg = '\0';
				return;
			}
			*arg++=*stream;
			arg_pos++;
			continue;
		}
		/* gobble quotations                                            */
		if (quote)
		{	/* check for EOF                                            */
			if (*stream=='\0')
			{
				raise_error(PARSER_UNEXPECTED_EOF);
			}
			/* check for end of quotation                               */
			if (*stream=='\'')
			{
				quote = false;
			}
			/* still gobbling quotation                                 */
			else
			{
				*arg++=*stream;
				arg_pos++;
				if(*stream=='\n')
				{
					col=0;
					line++;
				}
			}
			continue;
		}
		/* gobble escaped char                                          */
		if (backspace)
		{	/* check for EOF                                            */
			if (*stream=='\0')
			{
				raise_error(PARSER_UNEXPECTED_EOF);
			}
			*arg++=*stream;
			arg_pos++;
			backspace = false;
			if (*stream=='\n')
			{
				col=0;
				line++;
			}
			continue;
		}
		/* gobble variables                                             */
		if (variable)
		{
			/* next char of variable name                               */
			if (*stream!='\0' && strchr(" \t&><|\n;#\'\\${}",*stream)==NULL)
			{
				*var++=*stream;
				var_pos++;
				continue;
			}
			/* stop gobbling                                            */
			if (bracket)
			{
				/* check for closing bracket                            */
				if (*stream!='}')
				{
					raise_error(PARSER_BAD_SUBSTITUTION);
				}
				stream++;
				col++;
			}
			bracket=false;
			variable=false;
			*var='\0';
			var=varbuf.var;
			var_pos=0;
			/* and start substitution                                   */
			value = getenv(var);
			if (value!=NULL)
			{
				for (;*value!='\0';value++,arg++,arg_pos++)
				{
					if (arg_pos>=MAX_LINE_LENGTH-1)
					{
						raise_error(PARSER_OVERFLOW);

					}
					*arg=*value;
				}
			}
		}
		/* gobble identifier                                            */
		if (ide)
		{
			/* stop gobbling                                            */
			if (*stream=='\0' || strchr(" \t&><|\n;#",*stream)!=NULL)
			{
				*arg='\0';
				return;
			}
			/* next char of identifier                                  */
			if (strchr("\'\\$",*stream)==NULL)
			{
				*arg++=*stream;
				arg_pos++;
				continue;
			}
			/* fall through for quotes, escapes, and variables          */
		}

		/* analyze next char in stream                                  */
		switch (*stream)
		{
		/* EOF reached                                                  */
		case '\0' :
			*arg='\0';
			lookahead.kind=END;
			return;
		/* white spaces are skipped at the beginning                                     */
		case ' ' : 	case '\t':
			continue;
		/* ampersand token                                              */
		case '&':
			*arg='\0';
			lookahead.kind=AMP;
			stream++;
			col++;
			return;
		/* redirections                                                 */
		case '>':
			*arg='\0';
			lookahead.kind=OUT;
			stream++;
			col++;
			return;
		case '<':
			*arg='\0';
			lookahead.kind=IN;
			stream++;
			col++;
			return;
		/* pipe token                                                   */
		case '|':
			*arg='\0';
			lookahead.kind=STROKE;
			stream++;
			col++;
			return;
		/* new line or separator token                                  */
		case '\n':
			line++;
			col=0;
			*arg='\0';
			lookahead.kind=SEP;
			stream++;
			return;
	    /* separator token                                              */
		case ';':
			*arg='\0';
			lookahead.kind=SEP;
			stream++;
			col++;
			return;
		/* comment token                                                */
		case '#':
			comment=true;
			lookahead.kind=REM;
			continue;
		/* quotations                                                   */
		case '\'':
			quote = true;
			lookahead.kind=IDE;
			ide = true;
			continue;
		/* escaped chars                                                */
		case '\\':
			backspace=true;
			lookahead.kind=IDE;
			ide=true;
			continue;
		/* variables                                                    */
		case '$':
			variable=true;
			lookahead.kind=IDE;
			ide = true;
			if ( *(stream+1) == '{')
			{
				bracket=true;
				stream++;
				col++;
			}
			continue;
		/* finally a regular identifier                                 */
		default:
			lookahead.kind=IDE;
			ide = true;
			*arg++=*stream;
			arg_pos++;
		}
	}
}

/* scan the next token from input stream.                               */
static void scan()
{
	/* simply ignore comments                                           */
	do
	{
		read();
	}
	while (lookahead.kind==REM);
}


/* allocating memory --------------------------------------------------- */
/* --------------------------------------------------------------------- */

static int argv_size;                 /* size of current argument vector */

static void argv_new(prog_args* prog)
{
	char** argv = NULL;
	/* initialize prog                                                   */
	prog->input=prog->output=NULL;
	prog->next = NULL;
	prog->background = false;
	prog->argc = 0;
	prog->argv = NULL;
	/* create argument vector                                            */
	argv_size = NEW_ARGV_LENGTH;
	argv = malloc(NEW_ARGV_LENGTH*sizeof(char *));
	if (argv==NULL) raise_error(PARSER_MALLOC);
	argv[0]=NULL;
	prog->argv=argv;
	return;
}

static void argv_add(prog_args* prog, char* arg)
{
	char** argv = prog->argv;
	int argc = prog->argc;
	/* increase vector size                                              */
	if (argc+1>=argv_size)
	{
		argv_size+=NEW_ARGV_LENGTH;
		argv = realloc(argv, argv_size*sizeof(char *));
		if (argv==NULL) raise_error(PARSER_MALLOC);
	}
	/* add argument                                                      */
	argv[argc++]=arg;
	argv[argc]=NULL;
	prog->argv=argv;
	prog->argc=argc;
}

static prog_args* prog_new()
{
	prog_args* prog = (prog_args*)malloc(sizeof(prog_args));
	if (prog==NULL) raise_error(PARSER_MALLOC);
	argv_new(prog);
	return prog;
}

static cmds* cmd_new()
{
	cmds* cmd = (cmds*)malloc(sizeof(cmds));
	if (cmd==NULL) raise_error(PARSER_MALLOC);
	cmd->kind=PROG;
	cmd->next=NULL;
	argv_new(&(cmd->prog));
	return cmd;
}


/* parser -------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

/* returns a copy of the argument of the last parsed identifier          */
static char* get_ide()
{
	char* ide = NULL;
	if ( !(lookahead.kind==IDE) ) raise_error(PARSER_INVALID_STATE);
	ide = strdup(lookahead.arg);
	if (ide==NULL) raise_error(PARSER_MALLOC);
	return ide;
}

/* parses a redirection (PIPE token)                                     */
static void parse_redirection(prog_args* prog)
{
	if (lookahead.kind==IN)
	{
		scan(); /* skip '<' and scan identifier                          */
		if (lookahead.kind!=IDE) raise_error(PARSER_MISSING_FILE);
		prog->input = get_ide();
	}
	else
	{
		scan(); /* skip '>' and scan identifier                          */
		if (lookahead.kind!=IDE) raise_error(PARSER_MISSING_FILE);
		prog->output = get_ide();
	}
}

/* parse program arguments (builtins are also parsed this way first)      */
static void parse_prog(cmds* cmd, prog_args* prog)
{
	switch(lookahead.kind)
	{
	/* end of program arguments?                                         */
	case AMP:
		prog->background=true;
	case SEP:
	case END:
	case STROKE:
		return;
	/* redirections?                                                     */
	case OUT:
	case IN:
		parse_redirection(prog);
		break;
	/* argument?                                                         */
	case IDE:
		argv_add(prog,get_ide());
		break;
	default:
		raise_error(PARSER_INVALID_STATE);
	}
	/* parse next program argument                                       */
	scan();
	parse_prog(cmd,prog);
}

static int get_int(char* number)
{
	int value;
	if(sscanf(number,"%i",&value)!=1 || value<0)
	{
		raise_error(PARSER_ILLEGAL_ARGUMENT);
	}
	return value;
}

/* distinguish builtin commands from parsed program arguments            */
static void parse_cmd(cmds* cmd, prog_args* prog)
{
	char* path = NULL;  /* path for cd                                   */
	char* name = NULL;  /* name of environment variable                  */
	char* value = NULL; /* value of environment variable                 */
	int id = -1;        /* job id                                        */

	parse_prog(cmd, prog);
	/* any command supplied?                                             */
	if (prog->argc==0) raise_error(PARSER_MISSING_COMMAND);
    /*  exit command?                                                    */
	if (!strcmp(prog->argv[0],"exit"))
	{
		/* used in pipe?                                                 */
		if (cmd->kind!=PROG) raise_error(PARSER_ILLEGAL_COMBINATION);
		/* make exit command                                             */
		argv_free(prog);
		cmd->kind=EXIT;
		return;
	}
	/* cd command                                                        */
	if (!strcmp(prog->argv[0],"cd"))
	{
		/* used in pipe?                                                 */
		if (cmd->kind==PIPE) raise_error(PARSER_ILLEGAL_COMBINATION);
		/* make cd command                                               */
		if (prog->argc>=2)
		{
			path = strdup(prog->argv[1]);
			if (path==NULL) raise_error(PARSER_MALLOC);
		}
		argv_free(prog);
		cmd->kind=CD;
		cmd->cd.path=path;
		return;
	}
	/* unset an environment variable                                     */
	if (!strcmp(prog->argv[0],"unsetenv"))
	{
		/* used in pipe?                                                 */
		if (cmd->kind==PIPE) raise_error(PARSER_ILLEGAL_COMBINATION);
		/* enough args?                                                  */
		if (prog->argc<2) raise_error(PARSER_MISSING_ARGUMENT);
		/* make env command                                              */
		name = strdup(prog->argv[1]);
		if (name==NULL) raise_error(PARSER_MALLOC);
		argv_free(prog);
		cmd->kind=ENV;
		cmd->env.name=name;
		cmd->env.value=NULL;
		return;
	}
	/* set an environment variable                                       */
	if (!strcmp(prog->argv[0],"setenv"))
	{
		/* used in pipe?                                                 */
		if (cmd->kind==PIPE) raise_error(PARSER_ILLEGAL_COMBINATION);
		/* enough args?                                                  */
		if (prog->argc<3) raise_error(PARSER_MISSING_ARGUMENT);
		/* make env command                                              */
		name = strdup(prog->argv[1]);
		if (name==NULL) raise_error(PARSER_MALLOC);
		value = strdup(prog->argv[2]);
		if (value==NULL)
		{
			free(name);
			raise_error(PARSER_MALLOC);
		}
		argv_free(prog);
		cmd->kind=ENV;
		cmd->env.name=name;
		cmd->env.value=value;
		return;
	}
	/* request job infos                                                 */
	if (!strcmp(prog->argv[0],"jobs"))
	{
		/* used in pipe?                                                 */
		if (cmd->kind==PIPE) raise_error(PARSER_ILLEGAL_COMBINATION);
		/* make job command                                              */
		if (prog->argc>=2) id=get_int(prog->argv[1]);
		argv_free(prog);
		cmd->kind=JOB;
		cmd->job.kind=INFO;
		cmd->job.id=id;
		return;
	}
	/* continue a stopped job in background                              */
	if (!strcmp(prog->argv[0],"bg"))
	{
		/* used in pipe?                                                 */
		if (cmd->kind==PIPE) raise_error(PARSER_ILLEGAL_COMBINATION);
		/* make job command                                              */
		if (prog->argc>=2) id=get_int(prog->argv[1]);
		argv_free(prog);
		cmd->kind=JOB;
		cmd->job.kind=BG;
		cmd->job.id=id;
		return;
	}
	/* continue a job in foreground                                      */
	if (!strcmp(prog->argv[0],"fg"))
	{
		/* used in pipe?                                                 */
		if (cmd->kind==PIPE) raise_error(PARSER_ILLEGAL_COMBINATION);
		/* make job command                                              */
		if (prog->argc>=2) id=get_int(prog->argv[1]);
		argv_free(prog);
		cmd->kind=JOB;
		cmd->job.kind=FG;
		cmd->job.id=id;
		return;
	}
	/* check input for input redirection in pipe                         */
	if (cmd->kind==PIPE && prog->input != NULL)
	{
		raise_error(PARSER_ILLEGAL_REDIRECTION);
	}
}

static void parse_pipe(cmds* cmd, prog_args* prog)
{
	prog_args* elem = NULL;
	/* parse a command                                                   */
	parse_cmd(cmd, prog);
    /* not in pipe?                                                      */
	if (lookahead.kind!=STROKE)
	{
		return;
	}
	/* builtin and starting pipe?                                        */
	if( !(cmd->kind==PROG || cmd->kind==PIPE) )
	{
		raise_error(PARSER_ILLEGAL_COMBINATION);
	}
	/* in pipe                                                           */
	cmd->kind=PIPE;
	/* output redirection?                                               */
	if (prog->output != NULL)
	{
		raise_error(PARSER_ILLEGAL_REDIRECTION);
	}
	/* skip pipe symbol and parse next command                           */
	scan();
	elem = prog_new();
	prog->next=elem;
	parse_pipe(cmd,elem);
}

/* start parsing the input line                                          */
static void parse_input(cmds* cmd)
{
	cmds* elem;
	/* examine first token                                               */
	scan();
	if (lookahead.kind==END)
	{
		return;
	}

	/* create new command element, add it to the list, and parse it      */
	elem = cmd_new();
	if (cmd==NULL)
	{
		root=cmd=elem;
	}
	else
	{
		cmd->next = elem;
	}
	parse_pipe(elem, &elem->prog);

	/* parse next command from input                                     */
	parse_input(elem);
}

cmds* parser_parse(char *input)
{
	/* initialize scanner and parser                                     */
	stream = input;
	line = col = error_line = error_column = 0;
    root=NULL;
    parser_status = PARSER_OK;
    parser_message = messages[PARSER_OK];

    /* save environment to return here in case of an error               */
    if (setjmp(error_env))
    {
    	/* executed if error was raised                                  */
    	return NULL;
    }

	/* parse input                                                       */
	parse_input(root);
	return root;
}


/* visualization ------------------------------------------------------- */
/* --------------------------------------------------------------------- */

static void print_prog(prog_args* prog)
{
	int i;
	if (prog->input!=NULL)
	{
		printf("<%s ", prog->input);
	}
	for (i=0; i<prog->argc; i++)
	{
		printf("%s ",prog->argv[i]);
	}
	if (prog->output!=NULL)
	{
		printf(">%s ", prog->output);
	}
	if (prog->background)
	{
		printf("& ");
	}
}

static void print_pipe(cmds* cmd)
{
	prog_args* prog;
	for (prog=&cmd->prog; prog!=NULL; prog=prog->next)
	{
		print_prog(prog);
		if (prog->next!=NULL)
		{
			printf("| ");
		}
	}
}

/* print/dump/visualize a command list                                   */
void parser_print(cmds* cmd)
{
	if (cmd==NULL)
	{
		printf("NULL\n");
		return;
	}
	switch (cmd->kind)
	{
	case EXIT:
		printf("EXIT ");
		break;
	case CD:
		printf("CD %s ",cmd->cd.path);
		break;
	case PROG:
		print_prog(&cmd->prog);
		break;
	case PIPE:
		print_pipe(cmd);
		break;
	case ENV:
		if (cmd->env.value != NULL)
		{
			printf("SET %s=%s ",cmd->env.name,cmd->env.value);
		}
		else
		{
			printf("UNSET %s ",cmd->env.name);
		}
		break;
	case JOB:
		switch (cmd->job.kind)
		{
		case INFO: printf("JOBS "); break;
		case BG: printf("BG "); break;
		case FG: printf("FG ");
		}
		if (cmd->job.id!=-1) printf("%d ",cmd->job.id);
		break;
	}
	if (cmd->next!=NULL)
	{
		printf(";\n");
		parser_print(cmd->next);
	}
	else
	{
		printf("\n");
	}
}

/* test the parser and visualize the results                             */
void parser_test(char* input)
{
	cmds* cmd = NULL;
	printf("input:  %s\n",input);
	cmd = parser_parse(input);
	printf("result: %s @ %d:%d\n",parser_message,error_line,error_column);
	printf("---\n");
	parser_print(cmd);
	printf("---\n \n");
	parser_free(cmd);
}

#ifdef PARSER_DEBUG
/* main function for debug issuing a number of tests                     */
int main()
{
	setenv("a","var1",true);
	setenv("b","var2",true);
	setenv("c","var3",true);

	parser_test("");
	parser_test("exit");
	parser_test("cd ..");
	parser_test("cd exit");
	parser_test("cd foo next arguments are ignored");
	parser_test("ls -l");
	parser_test("cat; ls foo");
	parser_test("cat; ; ls foo");
	parser_test("ls -l -a -r   \n   cd \n echo foobar");
	parser_test(">file ls");
	parser_test("> out <in cat & echo foo");
	parser_test("missing file >& cd ..");
	parser_test("missing file <; exit");
	parser_test("ls <in -lisa | grep '.pdf' | sort >out");
	parser_test("ls > foo | grep .pdf    | sort");
	parser_test("ls | <foo grep .pdf |   sort");
	parser_test("ls | grep .pdf >bar |   sort");
	parser_test("cd .. | ls");
	parser_test("ls | exit | sort");
	parser_test("ls | sort | cd /bar/foo");
	parser_test("# this is a comment");
	parser_test("> out echo bla # this is a comment echo; $foo; 'open quote \n ls -l");
	parser_test("echo '; >foo <bar exit cd & \n $a | &' & ls -lisa" );
	parser_test("echo escaping \\\\ \\a \\< foo \\> bar \\; \\| \\$a \\& hello");
	parser_test("open 'quotation\n is still open");
	parser_test("open escaped char \\");
	parser_test("setenv foo 'bar and barfoo' this is ignored up to here; exit");
	parser_test("setenv foo");
	parser_test("unsetenv foo and this is ignored up to here; exit");
	parser_test("unsetenv");
	parser_test("$a> $b& cd ..; exit");
	parser_test("${here <foobar");
	parser_test("echo alloneide'bla'blub\\#\\;$a${b}$c'yeah'");
	parser_test("jobs 13 ignored; bg 1 ignored; fg 3 igno red; bg; fg");
	parser_test("jobs foobar");
	parser_test("bg -42");
	parser_test("fg dunno");

	return EXIT_SUCCESS;
}
#endif
