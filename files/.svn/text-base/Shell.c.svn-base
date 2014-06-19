/*
 ============================================================================
 Name        : Projekt2.c
 Author      : julieeen
 Version     :
 Copyright   : TEAM 4
 Description : Tolle Shell
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/types.h>

#include <signal.h>

#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <sys/ioctl.h>
#include <errno.h>

#include "Parser.h"
#include "Execute.h"
#include "Tools.h"

int exitShell, signals;



/*
 * Signal Handler
 * Implementiert den Code der bei ensprechdem Signal ausgef�hrt wird
 */
void sig_handler(int signo) {
	if ((signo > 0) && (signals)) {
		char* signalbeschreibung;
		signalbeschreibung = getSignalText(signo);
		printf("\n\033[0;31mrecived %s\033[0;37m\n", signalbeschreibung);
		free(signalbeschreibung);

	}

	if (signo < 1)
		perror("Signal error.\n");

}

int main(int argc, char *argv[]) {
	/*
	 * Ueberpruefen ob shell argumente hat
	 * -s : Signalausgabe
	 * -d : Debugmodus + Signalausgabe
	 */


	argc--;
	if (!strcmp(argv[argc], "-s")) {
		signals++;
		printf("Ausgabe von Signalen aktiviert.\n");
	}
	if (!strcmp(argv[argc], "-d")) {
		signals++;
		debug++;
		printf("Debugmodus und Signalausgabe aktiviert\n");
	}

	createCacheFiles();

	shell_pgid = getpid();			// ProzessID der Shell

	/*
	 * Signale registrieren
	 * SIGKILL & SIGSTOP k�nnen dabei nie uebernommen werden!
	 * Mit raise() koennen Signale 'simuliert' werden.
	 *
	 * Bem: Einige Signale werden unterdrueckt, da diese stoerende Nachrichten erzeugen,
	 * weils sie nicht aktiviert werden k�nnen!
	 * 		--> SIGUSR2 (17), SIGKILL, Alle Signale > 30,
	 */
	int signalNumber;
	for (signalNumber = 1; signalNumber < 30; ++signalNumber) {
		if (signalNumber == (SIGKILL))
			continue;
		if (signalNumber == (17))
			continue;

		if (signal(signalNumber, sig_handler) == SIG_ERR)
			printf("\ncan't catch %s\n", getSignalText(signalNumber));
	}

	/*
	 * Hier beginnt die eigentliche Shell
	 * Create Prompt, read line & execute
	 */
	char* input, shell_prompt[1024];

	rl_bind_key('\t', rl_complete); 	// Autocomplete mit TAB

	while (!exitShell) {
		/*
		 * Anhand der Fenstergroesse wird entschieden
		 * ob ein kurzes oder langes Prompt genutzt wird
		 */
		struct winsize w;
		ioctl(0, TIOCGWINSZ, &w);
		int size = strlen(getcwd(NULL, 512)) * 3;

		if (w.ws_col > size) {								// Grosses Prompt
			snprintf(shell_prompt, sizeof(shell_prompt),
					"\033[0;33mPID(%i):\033[0;32m%s\033[0;0m@\033[0;36m%s \033[0;31m>>\033[0;0m  ",
					getpid(), getenv("USER"), getcwd(NULL, 512));

		} else
			snprintf(shell_prompt, sizeof(shell_prompt),
					" \033[0;31m>>\033[0;0m ");				// Kleines Prompt

		input = readline(shell_prompt);

		if (!input)
			break;

		add_history(input);						// Input in History speichern

		if (debug)
			parser_test(input);
		cmds* liste = parser_parse(input); 		// Input parsen

		exitShell = doThis(liste);				// Befehlsliste abarbeiten

	}

	free(input);		// fertige Eingabe loeschen
	remove(PIPE1);	// PipeDumps entfernen
	remove(PIPE2);

	return EXIT_SUCCESS;
}
