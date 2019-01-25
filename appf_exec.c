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
#include <sysexits.h>

int af_exec_is_running(const char *pid_file_path, const char *process_name)
{
	struct stat pid_stat;
	FILE        *pid_file, *proc_file;
	int         pid     = 0;
	int     is_running  = 0;

	/* get the current pid for dhcpd */
	if ( stat( pid_file_path, &pid_stat ) == 0 )
	{
		if ( S_ISREG( pid_stat.st_mode ))
		{
			pid_file = fopen( pid_file_path, "r" );
			if ( pid_file != NULL )
			{
				char    pid_str[16];

				rewind( pid_file );
				if ( fgets( pid_str, sizeof(pid_str)-1, pid_file ) == NULL )
				{
					fclose( pid_file );
					return 0;
				}
				fclose( pid_file );

				if (( pid = strtol( pid_str, NULL, 0 )) == 0 )
					af_log_print( APPF_MASK_MAIN+LOG_INFO, "%s: PID file contains invalid data", __func__);
			}
		}
	}

	/* check to make sure the pid is actually running and owned by process_name */
	if ( pid != 0 )
	{
		char    proc_loc[16+sizeof(int)];

		snprintf( proc_loc, sizeof(proc_loc)-1, "/proc/%d/status", pid );

		proc_file = fopen( proc_loc, "r" );

		if ( proc_file != NULL )
		{
			char        line[128];
			char        *argv[32];
			int          argc;

			rewind( proc_file );
			/* get the first line which should be Name: <process> */
			if ( fgets( line, sizeof(line)-1, proc_file ) == NULL )
			{
				fclose( proc_file );
				return 0;
			}

			argc = af_argv( line, argv );

			if ( argc > 1)
			{
				if ( strcmp( argv[1], process_name ) == 0 )
				{
					is_running = 1;

					/* get the second line which should be State: <state char> <State Name> */
					/* R, S, D, T, Z, X */
					if ( fgets( line, sizeof(line)-1, proc_file ) == NULL )
					{
						fclose( proc_file );
						return 0;
					}

					argc = af_argv( line, argv );

					if ( argc > 1)
					{
						// Check for zombie
						if ( strcmp( argv[1], "Z" ) == 0 )
						{
							is_running = 0;
						}
						// Check for dead
						if ( strcmp( argv[1], "X" ) == 0 )
						{
							is_running = 0;
						}
					}
				}
			}

			fclose( proc_file );
		}
	}
	return( is_running );
}

int af_exec_fork( void )
{
	int          cnt, fd;
	pid_t        chpid, pgid;

	/* fork */
	if ( ( chpid = fork() ) < 0 )
	{
		af_log_print(LOG_CRIT, "%s: fork() failed, cause: %d (%s)", __func__, errno, strerror(errno));
		return( chpid );
	}
	else if ( chpid == 0 ) /* child */
	{
		/* create session and set process group ID */
		if ( ( pgid = setsid() ) < 0 )
		{
			af_log_print(LOG_CRIT, "%s: setsid() failed, cause: %d (%s)", __func__, errno, strerror(errno));
			exit( EX_OSERR );
		}

		/* don't block signals to child */
		if ( signal( SIGINT, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGINT, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGTERM, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGTERM, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGQUIT, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGQUIT, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGABRT, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGABRT, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGFPE, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGFPE, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGHUP, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGHUP, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGCHLD, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGCHLD, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGUSR1, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGUSR1, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}
		if ( signal( SIGUSR2, SIG_DFL ) == SIG_ERR )
		{
			af_log_print(LOG_CRIT, "%s: failed to set SIG_DFL for SIGUSR2, cause: %d (%s)",
						 __func__, errno, strerror(errno));
		}

		/* close all open files */
		for ( cnt = 0; cnt < sysconf(_SC_OPEN_MAX); cnt++ )
		{
			close( cnt );
		}

		/* open stdin, stdout, and stderr */
		if ( ( fd = open( "/dev/null", O_RDWR, 0 ) ) == 0 )
		{
			for ( cnt = 1; cnt < 3; cnt++ )
			{
				if ( dup( fd ) != cnt )
				{
					af_log_print(LOG_CRIT, "dup() failed, cause: %d (%s)", errno, strerror(errno));
					exit( EX_OSERR );
				}
			}
		}
		else
		{
			af_log_print(LOG_CRIT, "open() failed, cause: %d (%s)", errno, strerror(errno));
			exit( EX_OSERR );
		}

		/* set umask to something reasonable */
		(void)umask( S_IWGRP | S_IWOTH );

		af_log_print(APPF_MASK_MAIN+LOG_INFO, "fork child...." );    
	}
	else /* parent */
	{
		af_log_print(APPF_MASK_MAIN+LOG_INFO, "fork parent child pid=%d", chpid);
	}


	return( chpid );
}

/**
 * Defines and macros
 */
#define	MAX_ARGV_SZ		20
#define EXIT_FAILURE            1
#define PIPE_CHILD              1
#define PIPE_PARENT             0

/**
 * Local function prototypes
 */

int _af_exec_open_pipe_run(const char *command, int *pid)
{
	int rc;
	int cnt;
	int pipefd[2];

	rc = pipe(pipefd);
	if (rc != -1)
	{
		*pid = fork();

		switch (*pid)
		{
		case -1: /* error */
			rc = -1;
			close(pipefd[0]);
			close(pipefd[1]);
			break;

		case 0:	/* child */
			/* close all open files except the child side pipe*/ 
			for (cnt = 1; cnt < sysconf(_SC_OPEN_MAX); cnt++) 
			{
				if ( cnt != pipefd[PIPE_CHILD] )
					close(cnt); 
			}
//			close(pipefd[0]);
//			close(STDOUT_FILENO);
//			close(STDERR_FILENO);

			dup2(pipefd[PIPE_CHILD], STDOUT_FILENO);
			dup2(pipefd[PIPE_CHILD], STDERR_FILENO);

			/* the system() call assumes that /bin/sh is always available, and so will we. */
			execl("/bin/sh", "sh", "-c", command, NULL);
			_exit(EXIT_FAILURE);
			break;

		default: /* parent */
			close(pipefd[PIPE_CHILD]);
			rc = pipefd[PIPE_PARENT];
			break;
		} /* switch */
	}
	return rc;
}


static void (*old_sigchild)(int) = NULL;
static int exec_got_sigchild = 0;
static void _af_exec_sigchild_handler( int sig )
{
	exec_got_sigchild = 1;
	if ( old_sigchild != NULL )
		old_sigchild( sig );
}

int _af_exec_poll_for_result(int fd, int timeout, char *buf, int buf_sz, int sock)
{
	int             c, bail;
	int     len;
	char            tmp[1024];
	struct pollfd   pfd[1];
	struct timespec tv;
	int     sigint = 0;
	int     result = 0;

	// Hook SIGCHLD so we don't exit on that signal
	old_sigchild = signal(SIGCHLD, _af_exec_sigchild_handler);

	if ( buf )
		buf[0] = '\0';

	pfd[0].fd = fd;
	pfd[0].events = (POLLIN|POLLPRI);
	pfd[0].revents = 0;

	af_timer_now( &tv );
	bail = tv.tv_sec + (unsigned long)timeout;

	while ( 1 )
	{
		c = poll(pfd, 1, 100);

		if ( c > 0 )
		{
			if ( pfd[0].revents & (POLLIN|POLLPRI) )
			{
				tmp[0] = '\0';
				len = read(pfd[0].fd, tmp, sizeof(tmp)-1);

				if ( len < 0 )
				{
					if ( errno != EAGAIN )
					{
						af_log_print(APPF_MASK_MAIN+LOG_INFO, "%s: failed to read fd=%d errno=%d (%s)",
									 __func__, pfd[0].fd, errno, strerror(errno));
					}
				}
				else
				{
					tmp[len] = '\0';

					if ( buf )
					{
						strncat( buf, tmp, (buf_sz - strlen(buf)) );
						if ( strlen(buf) >= (unsigned int)buf_sz )
							break;
					}
					else if ( sock != 0 )
					{
						send( sock, tmp, strlen(tmp), 0 );
					}
					else
					{
						printf("Here's you're data: %s\n", tmp);
					}
				}
			}
			else if ( pfd[0].revents & POLLHUP )
			{
				af_log_print(APPF_MASK_MAIN+LOG_DEBUG, "%s: received POLLHUP, bailing", __func__);
				break;
			}
		}
		else if ( c < 0 )
		{
			if ( errno == EINTR )
			{
				if ( exec_got_sigchild )
				{
					exec_got_sigchild = 0;
					af_log_print(APPF_MASK_MAIN+LOG_DEBUG, "%s: SIGCHILD",__func__);
				}
				else
				{
					af_log_print(APPF_MASK_MAIN+LOG_DEBUG, "%s: interrupted errno=%d (%s)",
								 __func__, errno, strerror(errno));
					/* run through the poll loop one more time to 
					 * grab any remaining data on the descriptor */
					sigint = 1;
				}
			}
			else
			{
				af_log_print(LOG_ERR, "%s: failed errno=%d (%s)",
							 __func__, errno, strerror(errno));
				result = -1;
				break;
			}
		}
		else if ( sigint == 1 )
		{
			af_log_print(APPF_MASK_MAIN+LOG_DEBUG, "%s: received interrupt signal and no more data, bailing", __func__);
			break;
		}

		af_timer_now( &tv );
		if ( tv.tv_sec > bail )
		{
			af_log_print(APPF_MASK_MAIN+LOG_INFO, "%s: %d second timeout reached, bailing", __func__, timeout);
			result = -1;
			break;
		}
	}
	signal(SIGCHLD, old_sigchild);
	return( result );
}

void af_exec_to_buf(char *buf, int size, int timeout, const char *cmd)
{
	int     fd, pid;

	af_log_print(APPF_MASK_MAIN+LOG_INFO, "%s: executing command=(%s) timeout=%d", __func__, cmd, timeout);

	/* fire off the process */
	fd = _af_exec_open_pipe_run( cmd, &pid );

	if ( fd < 0 ) 
	{
		af_log_print( LOG_ERR, "%s: _af_exec_open_pipe_run() failed, (%d) %s", __func__, errno, strerror(errno));
		return;
	}

	/* get data back */
	if ( _af_exec_poll_for_result( fd, timeout, buf, size-1, 0 ) == -1 )
	{
		close( fd );

		/* kill it */
		(void)kill( pid, SIGKILL );
	}
	else
	{
		close( fd );
	}
}

void af_exec_to_fd(int sock, int timeout, const char *cmd)
{
	int     fd, pid;

	af_log_print(APPF_MASK_MAIN+LOG_INFO, "%s: executing command=(%s) timeout=%d", __func__, cmd, timeout);

	/* fire off the process */
	fd = _af_exec_open_pipe_run( cmd, &pid );

	if ( fd < 0 ) 
	{
		af_log_print( LOG_ERR, "%s: _af_exec_open_pipe_run() failed, (%d) %s", __func__, errno, strerror(errno));
		return;
	}
	/* get data back */
	if ( _af_exec_poll_for_result( fd, timeout, NULL, 0, sock ) == -1 )
	{
		close( fd );

		/* kill it */
		(void)kill( pid, SIGKILL );
	}
	else
	{
		close( fd );
	}
}

int af_exec_child( af_child_t *child )
{
	return 0;
}

int af_exec_fork_child( af_child_t *child )
{
	return 0;
}

