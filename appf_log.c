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

void af_open_logfile( )
{
	FILE *fh;

	if ( _af_daemon->log_fh == NULL && _af_daemon->log_filename != NULL )
	{
		if ( (fh=fopen(_af_daemon->log_filename, "a")) == NULL )
		{
			// Can't log since we are probably not initialized.
//			af_log_print( ILOG_WARN, "Failed to open logfile %s", _af_daemon->log_filename );
			return;
		}

		_af_daemon->log_fh = fh;
	}
}

void af_close_logfile(void)
{
	if ( _af_daemon->log_fh )
		fclose( _af_daemon->log_fh );

	_af_daemon->log_fh = NULL;
}

void _af_log_init( void )
{
	if ( _af_daemon->use_syslog )
	{
		// open syslog
		openlog( _af_daemon->log_name, LOG_PID | LOG_NDELAY, LOG_LOCAL7 );
	}
	else
	{
		closelog();
	}

	if ( _af_daemon->log_filename )
	{
		af_open_logfile();
	}
	else
	{
		af_close_logfile();
	}
}

void af_vlog_print( 
	unsigned int msg_mask, 
	const char *format, 
	va_list ap )
{
	FILE *fh;
	unsigned int log_level = msg_mask & LOG_PRIMASK;
	unsigned int log_mask = msg_mask & ~LOG_PRIMASK;

	// check group != 0 and mask = 0 don't log
	if ( log_mask && ((log_mask & _af_daemon->log_mask) == 0) )
	{
		// group mask doesnt match
		return;
	}
	// check level
	if ( log_level > _af_daemon->log_level )
	{
		// doesn't meet mask or mask level
		return;
	}

	if ( _af_daemon->use_syslog )
	{
		va_list nap;
		va_copy(nap,ap);
		vsyslog( log_level, format, nap);
		va_end(nap);
	}

	fh = _af_daemon->log_fh;

	if ( !fh && !_af_daemon->daemonize )
		fh = stdout;

	/* print timestamp when not using syslog */
	if ( fh )
	{
        struct timeval tv;
        time_t         curtime;
		va_list        nap;
        char           time_buf[64];

        gettimeofday(&tv, NULL); 
        curtime=tv.tv_sec;

        strftime(time_buf, sizeof(time_buf), 
                 "%m-%d-%Y %T.",
                 localtime(&curtime));

        fprintf(fh, "%s%06d %s: ",
                time_buf, (int)tv.tv_usec,
                _af_daemon->log_name );

		va_copy(nap,ap);
		vfprintf(fh, format, nap);
		va_end(nap);

		fprintf(fh, "\n");
		fflush( fh );
	}

	return;
}

void af_log_print( unsigned int msg_mask, const char *format , ...)
{
	va_list ap;
	va_start(ap, format); 
	af_vlog_print( msg_mask, format, ap );
	va_end(ap);
}

void af_fatal( const char *fmt, ...)
{
	va_list pvar;

	va_start(pvar, fmt);
	af_vlog_print( LOG_EMERG, fmt, pvar );
	va_end(pvar);

	if ( _af_daemon->pid_file )
		unlink( _af_daemon->pid_file );
	closelog();
	exit(-1);
}
