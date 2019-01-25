/*****************************************************************************/
/*               _____                      _  ______ _____                  */
/*              /  ___|                    | | | ___ \  __ \                 */
/*              \ `--. _ __ ___   __ _ _ __| |_| |_/ / |  \/                 */
/*               `--. \ '_ ` _ \ / _` | '__| __|    /| | __                  */
/*              /\__/ / | | | | | (_| | |  | |_| |\ \| |_\ \                 */
/*             \____/|_| |_| |_|\__,_|_|   \__\_| \_|\____/ Inc.             */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/*                       copyright 2016 by SmartRG, Inc.                     */
/*                              Santa Barbara, CA                            */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/* Author: Colin Whittaker                                                   */
/*                                                                           */
/* Purpose: daemonize an application                                         */
/*                                                                           */
/*****************************************************************************/


#include <appf.h>

void usage(void)
{
	fprintf(stderr, "\nUSAGE: daemonize <opts> <application> [<params> ...] ");
	fprintf(stderr, "         -v          Display version and exit\n");
	fprintf(stderr, "         -h          Display this message\n");
	fprintf(stderr, "         -s          log to syslog instead of stdout\n");
	fprintf(stderr, "         -l <level>  Specify logger level (1-7)\n");
	fprintf(stderr, "         -m <mask>   Specify logger mask (0-0xfffffff0)\n");
	exit(1);
}

/*
 *  sig_handler():  catch a kill signal.  set bail_out flag to we exit from poll loop
 */
void sig_handler(int sigtype)
{
}

/*
 *  main():  read cmd line, connect to server, drop into command loop
 */

af_daemon_t mydaemon;

int main(int argc, char *argv[])
{
	int  ch = 0;

	// Make sure we're given something
	if (argc == 1) {
		usage();
	}

	mydaemon.appname = argv[0];
	mydaemon.daemonize = 0;
	mydaemon.log_level = LOG_WARNING;
	mydaemon.sig_handler = sig_handler;
	mydaemon.log_name = argv[0];
	mydaemon.use_syslog = 1;

	// Only process options if our first argument is an option
	optind = 1;
	if (argv[1][0] == '-')
	{
		/*  read command line args  */
		while ( (ch = getopt(argc, argv, "t:p:T:D:c:i:l:m:svxhdn")) != -1 )
		{
			switch ( ch )
			{
			case 'l':
				mydaemon.log_level = atoi(optarg);
				break;

			case 'm':
				if ( (strlen(optarg) > 2) && (optarg[1] == 'x') )
					sscanf(optarg,"%x",&mydaemon.log_mask);
				else
					mydaemon.log_mask = atoi(optarg);
				break;

			case 's':
				mydaemon.use_syslog = TRUE;
				break;

			case 'v':
				printf("daemonize v%s\n", "1.0" );
				exit(0);
				break;

			case 'h':
			default:
				usage();
				/* NOTREACHED */
			}
		}

		if ( optind == argc )
		{
			usage();
		}
	}

	af_daemon_set( &mydaemon );
	af_daemon_start();

	argv[argc] = NULL;

	if ( af_exec_fork() == 0 )
	{
		// Child
		ch = execv( argv[optind], &argv[optind] );
		af_daemon_start();
		af_log_print( LOG_ERR, "Child error %d (%s)", errno, strerror(errno) );
	}

	return 0;
}

