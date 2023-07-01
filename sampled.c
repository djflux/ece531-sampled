/* Compile x86: gcc -Wall -o sampled sampled.c 
 *
 * Cross-compile for ARM on x86 with buildroot:
 *
 * ${BUILDROOT}/output/host/usr/bin//arm-linux-gcc --sysroot=${BUILDROOT}/output/staging -o sampled sampled.c 
 *
 * Where ${BUILDROOT} is the root of the buildroot environment.
 *
 * To show debugging output, compile as:
 *
 * x86: gcc -Wall -DDEBUG_OUT=1 -o sampled sampled.c 
 * ARM: ${BUILDROOT}/output/host/usr/bin//arm-linux-gcc --sysroot=${BUILDROOT}/output/staging -DDEBUG_OUT=1 -o sampled sampled.c 
 *
 * Cross compiling assumes that the buildroot environment for the ARM board
 * in question has been configured and successfully compiled.
 *
 * sampled.c is a simple Linux daemon that writes the system time in one
 * second intervals to the syslog logging system.
 *
 * UNM ECE531 - Internet of Things, Summer 2023
 *
 * Andrew Rechenberg
 * arechenberg at unm.edu
 * andrew at rechenberg.net
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static char *app_name = NULL;

// Define some error codes
//
#define OK		0
#define ERR_FORK	3
#define ERR_SETSID	4
#define ERR_CHDIR	5
#define ERR_WTF		187

// Error string
//
#define ERROR_FORMAT	"An error occurred. The error is: %s"

// Sleep time between log writes
//
#define LOG_INTERVAL	1

#ifndef DEBUG_OUT
#define DEBUG_OUT	0
#endif

// Setup a DEBUG function to print out various items during program
// execution.
//
#if defined(DEBUG_OUT) && DEBUG_OUT > 0
#define DEBUG(fmt, args...) printf("DEBUG: %s:%d:%s(): " fmt "\n", \
		__FILE__, __LINE__, __func__, ##args)
#else
#define DEBUG(fmt, args...) /* Don't do anything in release builds */
#endif

static void _signal_handler(const int signal) {
	switch(signal) {
		case SIGHUP: 
			break;
		case SIGTERM:
			syslog(LOG_INFO, "received SIGTERM - exiting.");
			closelog();
			exit(OK);
			break;
		default:
			syslog(LOG_INFO, "received unhandled signal.");
	}
}

static void _log_time(void) {
	while (1) {
		time_t rawtime;
		struct tm * timeinfo;

		time ( &rawtime );
		timeinfo = localtime ( &rawtime );

		syslog(LOG_INFO, "Current system time is: %s", asctime (timeinfo));
		sleep(LOG_INTERVAL);
	}
}


int main(int argc, char *argv[])
{

	// Get the executable name for logging
	// 
	app_name = strrchr(argv[0], '/');
	if (app_name) {
		// skip past the last /
		++app_name;   
	} else {
		app_name = argv[0];
	}

	openlog(app_name, LOG_PID | LOG_NDELAY | LOG_NOWAIT, LOG_DAEMON);
	syslog(LOG_INFO, "Starting %s", app_name);

	// Fork for daemon
	//
	pid_t pid = fork();

	// Log any issues forking
	//
	if ( pid < 0 ) {
		syslog(LOG_ERR, ERROR_FORMAT, strerror(errno));
		return ERR_FORK;
	}

	// Log any session errors
	//
	if (setsid() < -1) {
		syslog(LOG_ERR, ERROR_FORMAT, strerror(errno));
		return ERR_SETSID;
	}

	// Let parent process exit
	//
	if ( pid > 0 ) {
		return OK;
	}

	// Close standard file descriptors
	//
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	// Set file umask: 644 
	//
	umask(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );

	// Set working directory to root
	//
	if ( chdir("/") ) {
		syslog(LOG_ERR, ERROR_FORMAT, strerror(errno));
		return ERR_CHDIR;
	}

	// Handle signals
	//
	signal(SIGHUP, _signal_handler);
	signal(SIGTERM, _signal_handler);

	// Do what we're supposed to do - log system time to syslog
	// every second.
	_log_time();

	// We should never get here
	//
	return ERR_WTF;
}
