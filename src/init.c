/* See LICENSE file for copyright and license details. */

#include <linux/reboot.h>

#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "array.h"


typedef void (*tTermination)(void);

static void poweroff(void);
static void sigreap(void);
static void terminate(tTermination termination);

static char* initcmd[] = { "/sbin/init", NULL };

// By default, when there are no processes shutdown the machine
static tTermination termination = &poweroff;


char** getCommand(int argc, char* argv[])
{
	// Use init given by arguments, or the default one
	if(argc > 1)
	{
		array_free((void**)initcmd, 1);

		array_pop((void**)argv, argc);
	}
	else
	{
		array_free((void**)argv, argc);

		// Set argv to the default command
		argv = initcmd;
	}

	return argv;
}

static void poweroff(void)
{
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
}

static void restart(void)
{
	reboot(LINUX_REBOOT_CMD_POWER_OFF);
}

void listenSignals(void)
{
	// Sanitize signals
	sigset_t set;
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, NULL);

	// Disable `ctrl-alt-del` so we can process it
	reboot(LINUX_REBOOT_CMD_CAD_OFF);

	// Wait for signals
	while(1)
	{
		int sig;
		sigwait(&set, &sig);

		switch(sig)
		{
			case SIGCHLD: sigreap()           ; break;
			case SIGINT : terminate(&restart) ; break;  // ctrl-alt-del
			case SIGTERM: terminate(&poweroff); break;
		}
	}
}

static void sigreap(void)
{
	while(1)
		switch(waitpid(WAIT_ANY, NULL, WNOHANG))
		{
			// No more pending terminated process, go back to listen more signals
			case 0: return;

			case -1:
				// No more child processes, shutdown the machine
				if(errno == ECHILD)
				{
					sync();
					(*termination)();
				}
		}
}

static void spawn(char* const argv[])
{
	switch(fork())
	{
		case -1:  // Error
			perror("fork");
		break;

		case 0:  // Child process
		{
			setsid();

			execvp(argv[0], argv);

			perror("execvp");
			_exit(1);
		}
	}
}

void terminate(tTermination cmd)
{
	termination = cmd;

	// Send `SIGTERM` to all the system processes
	if(kill(-1, SIGTERM) == -1)
		perror("kill");
}


int main(int argc, char* argv[])
{
	// Not `PID 1`? Return error
	if(getpid() != 1) return 1;

	// Exec init command
	spawn(getCommand(argc, argv));

	// Start listening and processing signals
	listenSignals();

	/* not reachable */
	return 0;
}
