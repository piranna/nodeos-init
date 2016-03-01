/* See LICENSE file for copyright and license details. */

#include <linux/reboot.h>

#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>


// musl don't have defined that constant
#ifndef WAIT_ANY
#  define WAIT_ANY (-1)
#endif


typedef int (*tTermination)(void);

static int poweroff(void);
static bool sigreap(void);

static void terminate(tTermination termination);

static char* initcmd[] = { "/sbin/init", NULL };

// By default, when there are no more processes running, shutdown the machine
static tTermination termination = &poweroff;


char** getCommand(int argc, char* argv[])
{
	// No arguments, use default command
	if(argc == 1) return initcmd;

	// Use init given by arguments
	return &argv[1];
}

static int poweroff(void)
{
	return reboot(LINUX_REBOOT_CMD_POWER_OFF);
}

static int restart(void)
{
	return reboot(LINUX_REBOOT_CMD_POWER_OFF);
}

// Wait for signals
void listenSignals(sigset_t set)
{
	bool loop = true;

	while(loop)
	{
		int sig;
		sigwait(&set, &sig);

		switch(sig)
		{
			case SIGCHLD: loop = sigreap()    ; break;
			case SIGINT : terminate(&restart) ; break;  // ctrl-alt-del
			case SIGTERM: terminate(&poweroff); break;
			// Ignore other signals
		}
	}
}

sigset_t prepareSignals(void)
{
	// Block all signals
	sigset_t set;
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, NULL);

	// Disable `ctrl-alt-del` so we can process it
	reboot(LINUX_REBOOT_CMD_CAD_OFF);

	// Return the blocked signals mask to be used
	return set;
}

static bool sigreap(void)
{
	while(true)
		switch(waitpid(WAIT_ANY, NULL, WNOHANG))
		{
			// No more pending terminated process, go back to listen more signals
			case 0: return true;

			case -1:
				// No more child processes, shutdown the machine
				if(errno == ECHILD)
				{
					sync();

					if((*termination)() == -1)
					{
						// System shutdown failed probably due to lack of permissions, just
						// show an error message and exit the program
						perror("termination");

						return false;
					}
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

			// There was an error executing child process, show error message and exit
			perror("execvp");
			_exit(1);
		}
	}
}

void terminate(tTermination cmd)
{
	termination = cmd;

	// Send `SIGTERM` to all (system) child processes
	if(kill(-1, SIGTERM) == -1)
		perror("kill");
}


int main(int argc, char* argv[])
{
	// Prepare signals
	sigset_t set = prepareSignals();

	// Mount `devtmpfs` filesystem in `/dev`. This is mandatory for Node.js on
	// NodeOS, but it's fairly common so it doesn't hurts (too much...)
	if(mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) == -1)
		perror("mount");

	// Exec init command
	spawn(getCommand(argc, argv));

	// Start listening and processing signals
	listenSignals(set);

	// Exit program failfully since we were not able to shutdown the system,
	// probably due to lack of permissions
	return 1;
}
