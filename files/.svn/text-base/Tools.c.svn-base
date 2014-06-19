/*
 * Tools.c
 *
 *  Created on: 16.06.2013
 *      Author: julieeen
 *
 *	nuetzliche Funktionen
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "Tools.h"
#include <errno.h>

#define SIGNAL_PATH "signals"

/*
 * Textfarbe:
 * \033[x;ym
 * mit x und y aus:
 * 	0 to restore default color
 1 for brighter colors
 4 for underlined text
 5 for flashing text
 30 for black foreground
 31 for red foreground
 32 for green foreground
 33 for yellow (or brown) foreground
 34 for blue foreground
 35 for purple foreground
 36 for cyan foreground
 37 for white (or gray) foreground
 40 for black background
 41 for red background
 42 for green background
 43 for yellow (or brown) background
 44 for blue background
 45 for purple background
 46 for cyan background
 47 for white (or gray) background
 */

/*
 * Erstelle PipeDump-Pfade
 * Zwischenspeicher fuer die einzelnen Pipeprogramme
 */
void createCacheFiles() {
	FILE* createFile;

	char* home = getenv("HOME");
	strcpy(PIPE1, strcat(home, "/.PipeDump1"));

	createFile = fopen(PIPE1, "w");
	if(createFile == NULL){
		perror("1 Fehlende Schreibrechte für Cachefiles.");
	}
	fclose(createFile);

	if (debug) {
		printf("cachfile @%s\n", PIPE1);
	}

	home[strlen(home) - 11] = '\0';
	strcpy(PIPE2, strcat(home, "/.PipeDump2"));

	createFile = fopen(PIPE2, "w");
	if(createFile == NULL){
		perror("Fehlende Schreibrechte für Cachefiles.");
	}
	fclose(createFile);

	if (debug) {
		printf("cachfile @%s\n", PIPE2);
	}
}

/*
 * Gibt des Pfad zur gesuchten Datei zurÔøΩck
 * Dabei wird nur in den Ordnern gesucht, die unter lookup abgelegt sind.
 * (Idee: Umgebungsvariablen in Datei ablegen)
 */
char result[256];
DIR *dirHandle;
struct dirent * dirEntry;

/*
 * Durchsucht alle Ordner aus lookup nach file mit $filename
 * Gibt Pfad dahin zurück.
 */
char * whereIs(char* filename) {
	result[0] = '\0';
	char * cwd_tmp = getcwd(NULL, 1024);
	strcat(cwd_tmp, "/");
	char * lookup[] = { "/bin/", "/usr/bin/", "/sbin/", cwd_tmp };

	int iterateLookup = 0;
	while (iterateLookup < sizeof(lookup) / sizeof(char*)) {		// iteriert Ordner
		int iterateDir = 1;
		char* currentPath = lookup[iterateLookup];

		dirHandle = opendir(currentPath);

		if (debug) {
			printf("Durchsuche : %s\n", currentPath);
		}

		if (dirHandle == NULL) {
			perror("opendir() error");
		}

		if (dirHandle) {
			while (0 != (dirEntry = readdir(dirHandle))) {		// iteriert Files
				if (dirEntry == NULL) {
					perror("readdir() error");
				}
				if (!strcmp(dirEntry->d_name, filename)) {
					strcat(result, currentPath);
					strcat(result, filename);
					return result;
				}
				iterateDir++;
			}
		}
		closedir(dirHandle);
		iterateLookup++;
	}
	return NULL;
}

/*
 * Signaltable from:
 * http://people.cs.pitt.edu/~alanjawi/cs449/code/shell/UnixSignals.htm
 * SignalName x = signalLookup[4*(x-1)]
 * Signalbescheibung y = signalLookup[4*(x-1)+3]
 */
char * signalLookup[] = { "SIGHUP", "1", "Exit", "Hangup", "SIGINT", "2",
		"Exit", "Interrupt", "SIGQUIT", "3", "Core", "Quit", "SIGILL", "4",
		"Core", "Illegal Instruction", "SIGTRAP", "5", "Core",
		"Trace/Breakpoint Trap", "SIGABRT", "6", "Core", "Abort", "SIGEMT", "7",
		"Core", "Emulation Trap", "SIGFPE", "8", "Core", "Arithmetic Exception",
		"SIGKILL", "9", "Exit", "Killed", "SIGBUS", "10", "Core", "Bus Error",
		"SIGSEGV", "11", "Core", "Segmentation Fault", "SIGSYS", "12", "Core",
		"Bad System Call", "SIGPIPE", "13", "Exit", "Broken Pipe", "SIGALRM",
		"14", "Exit", "Alarm Clock", "SIGTERM", "15", "Exit", "Terminated",
		"SIGUSR1", "16", "Exit", "User Signal 1", "SIGUSR2", "17", "Exit",
		"User Signal 2", "SIGCHLD", "18", "Ignore", "Child Status", "SIGPWR",
		"19", "Ignore", "Power Fail/Restart", "SIGWINCH", "20", "Ignore",
		"Window Size Change", "SIGURG", "21", "Ignore",
		"Urgent Socket Condition", "SIGPOLL", "22", "Ignore",
		"Socket I/O Possible", "SIGSTOP", "23", "Stop", "Stopped (signal)",
		"SIGTSTP", "24", "Stop", "Stopped (user)", "SIGCONT", "25", "Ignore",
		"Continued", "SIGTTIN", "26", "Stop", "Stopped (tty input)", "SIGTTOU",
		"27", "Stop", "Stopped (tty output)", "SIGVTALRM", "28", "Exit",
		"Virtual Timer Expired", "SIGPROF", "29", "Exit",
		"Profiling Timer Expired", "SIGXCPU", "30", "Core",
		"CPU time limit exceeded", "SIGXFSZ", "31", "Core",
		"File size limit exceeded", "SIGWAITING", "32", "Ignore",
		"All LWPs blocked", "SIGLWP", "33", "Ignore",
		"Virtual Interprocessor Interrupt for Threads Library", "SIGAIO", "34",
		"Ignore", "Asynchronous I/O" };

char * getSignalText(int signo) {
	char* antwort = (char*) malloc(128);
	sprintf(antwort, "%s <%s>", signalLookup[4 * (signo - 1)],
			signalLookup[4 * (signo - 1) + 3]);
	return antwort;
}

