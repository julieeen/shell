/*
 * Execute.c
 *
 *  Created on: 26.06.2013
 *      Author: julieeen
 *
 *  Modul um gegebene Befehlsliste ab zu arbeiten
 *  - exit 	: 	Shell beenden
 *  - cd 	:	cwd aendern
 *  - env	:	Variablen anlegen, loeschen
 *  - job	:	Jobmngt
 *  - prog	:	Programm ausfuehren (fg,bg)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

#include "Parser.h"
#include "Tools.h"
#include "Execute.h"

/*
 * Hilfsfunktion zum Ausfuehren eines externen Programms
 * Informationen dazu in der Doku
 */
int executeProg(prog_args* prog) {

	char* path = whereIs(prog->argv[0]);		// Programm suchen
	if (!path) {
		perror("Programm nicht gefunden");
		return -1;
	}

	pid = fork();					// Prozesse trennen

	if (pid > 0) {					// Vaterprozess
		if (!prog->background) {
			waitpid(pid, 0, 0);		// Warten auf Kindprozess falls fg
		}
		return 0;

	} else if (pid == 0) { 			//Kindprozess

		if (prog->input != NULL) {			// redirect von Stdin auf file
			if (debug)
				printf("Input von %s\n", prog->input);
			freopen(prog->input, "r", stdin);
		}
		if (prog->output != NULL) {			// redirect von Stdout auf file
			if (debug)
				printf("Output in %s\n", prog->output);
			freopen(prog->output, "w", stdout);

		}

		if (debug)
			printf("exec(%s)\n", path);

		execv(path, prog->argv);			// Programm ausf�hren

		// Fallls exec nicht klappt, muss der Kindprozess beendet werden
		perror("exec fail:\n");
		exit(0);

	} else
		perror("fork() error!\n");

	free(path);
	return 0;
}

/*
 * Fuehrt Befehlsliste aus
 * [-1,0,1] == [Fehler, OK, exit]
 */
int doThis(cmds* liste) {
	while (liste != NULL) {
		cmds* currentCmd = liste;
		liste = liste->next;

		/*
		 * Bei exit die Shell beenden
		 */
		if (currentCmd->kind == EXIT) {
			return 1;
		}

		/*
		 * CD bringt einen neuen Pfad, der mittels chdir() veraendert wird.
		 */
		if (currentCmd->kind == CD) {
			chdir(currentCmd->cd.path);
			continue;
		}

		/*
		 * unsetenv hat keinen value,
		 * setenv hat Value und ueberschreibt vorhandene ENVs
		 */
		if (currentCmd->kind == ENV) {
			if (currentCmd->env.value == NULL) {
				unsetenv(currentCmd->env.name);
			} else
				setenv(currentCmd->env.name, currentCmd->env.value, 1);
			// 1 um evtl gesetzten Wert einfach zu ueberschreiben, 0 um alten Wert zu nutzen
			continue;
		}

		/*
		 *	Jobcontol
		 */
		if (currentCmd->kind == JOB) {
			printf("noch nicht implementiert\n");
			break;
		}

		/*
		 * Piping
		 * Ueber gegebene PIPE iterieren
		 * Dabei die Ausgabe des aktuellen Programms cachen,
		 * naechstes Programm mit cache fuettern und starten
		 * Erstes und letztes Programm bekommten I/O wieder auf tty
		 */
		if (currentCmd->kind == PIPE) {
			int num = 0; //Zaehlt die Programme in der Pipe
			char cache[265], cache2[265];

			prog_args* iteratePipe = &currentCmd->prog;

			while (iteratePipe->next != NULL) {
				// alterniert PipeCacheFiles fuer pipes mit Laenge > 2
				if (num % 2 == 0) {
					strcpy(cache, PIPE1);
					iteratePipe->output = cache;		// redirect von Stdout auf tempfile
				} else {
					strcpy(cache2, PIPE2);
					iteratePipe->output = cache2;	// redirect von Stdout auf tempfile
				}

				if (executeProg(iteratePipe) < 0) {
					perror("Fehler bei der Programmausfuehrung!\n");
					break;
				}

				if (num % 2 == 0) {
					iteratePipe->next->input = cache;	// input des folgenden Programms auf tempfile
				} else {
					iteratePipe->next->input = cache2;	// input des folgenden Programms auf tempfile
				}

				iteratePipe = iteratePipe->next;

				num++;
			}

			executeProg(iteratePipe);		//letztes PipeProgramm ausfuehren
		}

		/*
		 * Programmausfuehrung
		 * Methode absrahiert um diese fuer Piping zu nutzen
		 */
		if (currentCmd->kind == PROG) {
			executeProg(&currentCmd->prog);
			continue;

		}
	}
	return 0;
}

