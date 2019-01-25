/*****************************************************************************/
/*               _____                      _  ______ _____                  */
/*              /  ___|                    | | | ___ \  __ \                 */
/*              \ `--. _ __ ___   __ _ _ __| |_| |_/ / |  \/                 */
/*               `--. \ '_ ` _ \ / _` | '__| __|    /| | __                  */
/*              /\__/ / | | | | | (_| | |  | |_| |\ \| |_\ \                 */
/*              \____/|_| |_| |_|\__,_|_|   \__\_| \_|\____/ Inc.            */
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
/* Purpose: Application Framework Library for building daemons               */
/*                                                                           */
/*****************************************************************************/

#include <appf.h>

af_daemon_t _af_default_daemon = {
};

af_daemon_t *_af_daemon = &_af_default_daemon;

af_daemon_t *af_daemon_set( af_daemon_t *ctx )
{
	af_daemon_t *ret = _af_daemon;
	_af_daemon = ctx;
	return ret;
}

int af_log_signal_open()
{
	int  fd;
	char fn[128];

	strcpy( fn, "/var/log/" );
	strcat( fn, _af_daemon->appname );
	strcat( fn, ".log" );

	fd = open( fn, O_RDWR | O_APPEND | O_CREAT, 0644 );
	
	return fd;
}
void af_log_signal( int fd, char *data )
{
	if ( write( fd, data, strlen( data ) ) != strlen(data) )
	{
		// an error happened.
	}
}

char *af_itoa( int num, char *str, int base )
{
	int i = 0;
	int isNegative = 0;

	/* Handle 0 explicitely, otherwise empty string is printed for 0 */
	if ( num == 0 )
	{
		str[i++] = '0';
		str[i] = '\0';
		return str;
	}

	// In standard itoa(), negative numbers are handled only with
	// base 10. Otherwise numbers are considered unsigned.
	if ( num < 0 && base == 10 )
	{
		isNegative = 1;
		num = -num;
	}

	// Process individual digits
	while ( num != 0 )
	{
		int rem = num % base;
		str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
		num = num/base;
	}

	// If number is negative, append '-'
	if ( isNegative )
		str[i++] = '-';

	str[i] = '\0'; // Append string terminator

	// Reverse the string
	//reverse(str, i);
	//void reverse(char str[], int length)
	{
		int start = 0;
		int end = i -1;
		while (start < end)
		{
//			swap(*(str+start), *(str+end));
			{
				char x = *(str+start);
				*(str+start) = *(str+end);
				*(str+end) = x;
			}
			start++;
			end--;
		}
	}
	return str;
}

void af_seg_fault_handler (int signo, siginfo_t* p_siginfo, void* dummy3)
{
//	af_log_print( LOG_ERR, "===  Received %s", af_signal_to_str(signo));
	int fd;

	/* We will traverse this handler twice, once for the 
	 * error signal (SIGSEGV, SIGBUS, SIGFPE, etc), and 
	 * a second time with a SIGABRT due to the call to
	 * abort() below.
	 */
	if (signo == SIGABRT) {
		// This has to be a 'return'
		// A call to exit() will not produce a core file
		return;
	}

	fd = af_log_signal_open();
	if ( fd > 0 ) 
	{
		af_log_signal( fd, "[af_seg_fault_handler]\n" );

#ifdef BACKTRACE_AVAILABLE
		{
			int size, i;
			void *array[MAX_FRAMES];
			char buf[32];
//			char **strings;

			size = backtrace (array, MAX_FRAMES);
//			strings = backtrace_symbols (array, size);

			af_log_signal( fd, "Backtrace.....\n" );
			for (i = 0; i < size; i++)
			{
//				af_log_print( ILOG_ERR, "%s", strings[i] );
//				af_signal_log( fd, strings[i] );
				af_log_signal( fd, af_itoa( (int)(ptrdiff_t)array[i], buf, 16 ) );
				af_log_signal( fd, "\n" );
			}
		}
#endif
		close(fd);
	}

	closelog();


	/* generate a core file */
//	af_log_print( ILOG_ERR, "Generating core file and exiting");
	abort();

	exit(1);
}

void _af_setup_signals( void )
{
	struct sigaction act;

	// Set signals to proper defaults
	act.sa_sigaction = af_seg_fault_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGSEGV, &act, (struct sigaction *)NULL) < 0)
	{
		   af_fatal("sigaction() failed, errno=%d (%s)", errno, strerror(errno) );
	}
	if (sigaction(SIGABRT, &act, (struct sigaction *)NULL) < 0)
	{
		   af_fatal("sigaction() failed, errno=%d (%s)", errno, strerror(errno) );
	}
	if (sigaction(SIGILL, &act, (struct sigaction *)NULL) < 0)
	{
		   af_fatal("sigaction() failed, errno=%d (%s)", errno, strerror(errno) );
	}
	if (sigaction(SIGFPE, &act, (struct sigaction *)NULL) < 0)
	{
		   af_fatal("sigaction() failed, errno=%d (%s)", errno, strerror(errno) );
	}
	if (sigaction(SIGBUS, &act, (struct sigaction *)NULL) < 0)
	{
		   af_fatal("sigaction() failed, errno=%d (%s)", errno, strerror(errno) );
	}
	if (sigaction(SIGSYS, &act, (struct sigaction *)NULL) < 0)
	{
		   af_fatal("sigaction() failed, errno=%d (%s)", errno, strerror(errno) );
	}

	signal(SIGINT, _af_daemon->sig_handler);
	signal(SIGTERM, _af_daemon->sig_handler);
	signal(SIGQUIT, _af_daemon->sig_handler);
	signal(SIGHUP, _af_daemon->sig_handler);

	/* always ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN );
}

int af_enable_core_dump( rlim_t limit )
{
	int   rc;
	struct rlimit rlim;

	if ( limit == 0 )
		limit = CORE_LIMIT;

	rlim.rlim_cur = limit;
	rlim.rlim_max = limit;
	rc = setrlimit(RLIMIT_CORE, &rlim);
	if (rc != 0) {
		af_log_print( LOG_ERR, "ERROR: setrlimit() returned %d", rc);
	}

	return rc;
}


void _af_daemonize( void )
{
	int cnt, fd; 
	pid_t pid, pgid; 

	if ((pid = fork()) < 0) 
	{ 
		af_fatal("Daemonizing failed: Can not spawn child "
			  "process, exiting...");
	} 
	else if (pid == 0) 
	{	/* child */ 
	
		/* create session and set process group ID */ 
		if ((pgid = setsid()) < 0)
			af_fatal("Daemonizing failed: Can not setsid, exiting...");	

		/* get rid of our controlling terminal */ 
		if ((pid = fork()) < 0)
		{ 
			af_fatal("Daemonizing failed: Can not respawn "
				  "child process, exiting...");	
		} 
		else if (pid == 0)
		{ 
			/* child */ 
			if (chdir("/") != 0)
			{ 
				af_fatal("Daemonizing failed: Can not change "
					  "working directory, exiting...");	
			} 

			(void)umask(077); 

			/* close all open files */ 
			for (cnt = 0; cnt < sysconf(_SC_OPEN_MAX); cnt++) 
				close(cnt); 

			/* open stdin, stdout, and stderr */ 
			if ((fd = open("/dev/null", O_RDWR, 0)) == 0) { 
				for (cnt = 1; cnt < 3; cnt++)  
					if (dup(fd) != cnt) 
					{ 
						af_fatal("Can not open file "
						"descriptor %d, exiting...", fd);	
					} 
			}
			else
				af_fatal("Can not open file descriptor %d, exiting...", cnt);	

		}
		else
		{
			/* parent (controlling terminal) */ 
			exit(0); 
		} 
	 
	}
	else
	{
		/* parent */ 
		exit(0); 
	} 

	/* demonized */ 
	af_enable_core_dump(CORE_LIMIT);

}

int _af_write_pid( void )
{
	FILE *fh;

	/* make check that pid_file has been set */
	if ( _af_daemon->pid_file == NULL)
	{
		return 0;
	}

	if ( af_exec_is_running( _af_daemon->pid_file, _af_daemon->appname  ) )
	{
		af_log_print( LOG_EMERG, "another instance of %s is already running, exiting", _af_daemon->appname );
		return -1;
	}
	else
	{
		struct stat	pid_stat;

		/* delete the old PID file if it exists */
		if ( stat( _af_daemon->pid_file, &pid_stat ) == 0 )
		{
			if ( S_ISREG( pid_stat.st_mode ))
			{
				af_log_print( APPF_MASK_MAIN+LOG_INFO, "PID file found, but process dead, removing");
				unlink( _af_daemon->pid_file );
			}
		}
	}

	/* create the pidfile */
	if ( (fh = fopen( _af_daemon->pid_file, "w"))==NULL)
	{
		af_log_print( LOG_ERR, "Failed to create pid file %s", _af_daemon->pid_file );
		return -1;
	}

	fprintf(fh, "%d", getpid());
	fclose(fh);
	chmod( _af_daemon->pid_file, 0644);
    return 0;
}

extern void _af_log_init( void );

int af_daemon_start( void )
{
	// Check daemon context is set.
	if ( _af_daemon == NULL )
	{
		printf("Fatal error af_set_daemon() not called");
		exit(-1);
	}

	// setup signals
	_af_setup_signals( );

	// daemonize
	if ( _af_daemon->daemonize )
	{
		_af_daemonize( );
	}

	_af_daemon->timers.fd = -1;

	// setup logging
	_af_log_init( );

	// Create PID file
	return _af_write_pid( );
}

char *_af_get_next_arg( char *string, int *len )
{
	char *argv;
	char  quote = 0;
	char  iquote = 0;

	/* find the begining of a word */
	while ( *string && isspace(*string) )
	{
		string++;
	}

	if ( *string == '"' || *string == '\'' )
	{
		quote = *string;
		string++;
	}

	*len = 0;

	if ( *string == '\0' )
	{
		/* There is no argument. */
		return NULL;
	}


	argv = string;

	/* find the end of the word */
	while ( *string )
	{
		if ( !quote && !iquote && isspace(*string) )
			break;

		if ( *string == quote )
		{
			break;
		}
		// Check for internal quotes and keep them
		if ( *string == '"' || *string == '\'' )
		{
			if ( iquote )
			{
				if ( iquote == *string )
				{
					// terminate it
					iquote = 0;
				}
				// else it will just pass as a quote inside a quote.
				// multiple nesting is not supported.
			}
			else
			{
				// begin quoted section, don't exit prematurely
				iquote = *string;
			}
		}

		string++;
		(*len)++;
	}
	/* null terminate the arg */
	*string = 0;

	return argv;
}

int af_parse_argv( char *buffer, char **argv, int max )
{
	int   len, cnt;
	char *ptr, *end;

	if ( !buffer )
		return 0;

	cnt = 0;
	ptr = buffer;
	end = buffer+strlen(buffer);

	do
	{
		ptr = _af_get_next_arg( ptr, &len );

		if ( ptr == NULL )
		{
			// we have no arg
			break;
		}

		argv[cnt++] = ptr;

		ptr += (len + 1); //skip past the null termination


	} while ( (ptr < end) && (cnt < max-1) );

	// Last argv needs to be null.
	argv[cnt] = NULL;

	return cnt;
}


