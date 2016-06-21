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

#include "queue.h"


// musl don't have defined that constant
#ifndef WAIT_ANY
#  define WAIT_ANY (-1)
#endif


typedef int (*tTermination)(void);

static void nuke();
static int poweroff(void);
static bool sigreap(void);

static void terminate(tTermination termination);

const char* initcmd = "/sbin/init";
const unsigned int countdown = 5;

// By default, when there are no more processes running, shutdown the machine
static tTermination termination = &poweroff;
static bool exiting = false;


char** getCommand(int argc, char* argv[])
{
	// Use init given by arguments
	if(argc > 1 && argv[1][0] == '/') return &argv[1];

	// No arguments or not given a command, use default one
	argv[0] = initcmd;
	return argv;
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
			case SIGALRM: nuke()              ; break;
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

void umount_filesystems()
{
	// Create a LIFO strcture to host the mounted filesystems
	SLIST_HEAD(slisthead, entry) head = SLIST_HEAD_INITIALIZER(head);

  struct entry
	{
		char path[80];
    SLIST_ENTRY(entry) entries;
  } *item;

  SLIST_INIT(&head);


	// Get mounted filesystems and fill the LIFO structure
	FILE*	pFile = fopen("/proc/mounts", "r");
	while(!feof(pFile))
	{
		item = malloc(sizeof(struct entry));

		if(fscanf(pFile, "%*s %s %*s %*s 0 0", item->path) == EOF)
		{
			free(item);
			break;
		};

    SLIST_INSERT_HEAD(&head, item, entries);
	};
	fclose(pFile);


	// Unmount filesystems in reverse order how they were mounted
	while(!SLIST_EMPTY(&head))
	{
		item = SLIST_FIRST(&head);
	  SLIST_REMOVE_HEAD(&head, entries);

		if(umount(item->path) == -1) perror(item->path);

	  free(item);
	}
}

static bool sigreap(void)
{
	// Stop the nuke timer
	if(exiting) alarm(0);

	while(true)
		switch(waitpid(WAIT_ANY, NULL, WNOHANG))
		{
			// No more pending terminated process, go back to listen more signals
			case 0:
			{
				// Re-start the nuke timer
				if(exiting) alarm(countdown);

				return true;
			}

			// Error
			case -1:
				// No more child processes, shutdown the machine
				if(errno == ECHILD)
				{
					sync();
					umount_filesystems();

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
	if(kill(-1, SIGTERM) == -1) perror("kill SIGTERM");

	// Start countdown to send `SIGKILL` to the remaining processes and nuke them
	exiting = true;
	alarm(countdown);
}

void nuke()
{
	// Ignore the `SIGALARM` signal if we are not exiting
	if(!exiting) return;

	// Send `SIGKILL` to all (system) child processes
	if(kill(-1, SIGKILL) == -1) perror("kill SIGKILL");
}


int main(int argc, char* argv[])
{
	// Prepare signals
	sigset_t set = prepareSignals();

	// Mount common kernel filesystems
	if(mount("procfs", "/proc", "procfs", 0, NULL) == -1)
		perror("mount procfs");

	if(mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) == -1)
		perror("mount devtmpfs");

	// Exec init command
	spawn(getCommand(argc, argv));

	// Start listening and processing signals
	listenSignals(set);

	// Exit program failfully since we were not able to shutdown the system,
	// probably due to lack of permissions
	return 1;
}
