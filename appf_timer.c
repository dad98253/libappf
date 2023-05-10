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
//#include <sys/timerfd.h>
#include <sys/sysinfo.h>
#include <time.h>

void af_timer_now( struct timespec *now )
{
	clock_gettime( CLOCK_MONOTONIC, now );
}

static time_t boottime;

time_t *af_timer_curtime( struct timespec *then )
{
	struct sysinfo info;
	static time_t  curtime;

	if ( !boottime )
	{
		sysinfo(&info);
		boottime = time(NULL) - info.uptime;
	}

	curtime = boottime+then->tv_sec;

	return &curtime;
}

void af_timer_check( )
{
	af_timer_t        *tm;
	struct timespec    now;
	af_timer_t        *expire_tail;

	af_timer_now( &now );

	tm = _af_daemon->timers.head;

	_af_daemon->timers.expired = expire_tail = NULL;

	while ( tm != NULL && timediff( tm->timeout, now ) <= 0 )
	{
		// remove it from the list
		_af_daemon->timers.head = tm->next;

		tm->next = NULL;

		// Add to expired list
		if ( _af_daemon->timers.expired == NULL )
		{
			_af_daemon->timers.expired = tm;
			expire_tail = tm;
		}
		else
		{
			expire_tail->next = tm;
			expire_tail = tm;
		}

		tm = _af_daemon->timers.head;
	}

	while( _af_daemon->timers.expired )
	{
		// get next timer
		tm = _af_daemon->timers.expired;

		// remove if from the list
		_af_daemon->timers.expired = tm->next;

		tm->next = NULL;
		tm->running = FALSE;

		// run the call back
		(*tm->callback)( tm );
	}
}

void _af_timer_handle_event( af_poll_t *ap );

void af_timer_reset_fd( void )
{
/* jck	this routine and its associated timer functions do not appear to be called by the
//      libappf library and I don't use them in com2net. The timerfd_... family of routines
//      are gnulib specific and will not appear on other unix flavors. In addition, they
//	were added to gnulib well after Redhat Linux 7.0 (the target system in my ham shack)
//	was released. So, commenting this code out has no effect on me...

	af_timer_t         *timer;
	struct timespec     now;
	struct itimerspec   tm;
	long                sec, nsec;
	uint64_t            exp_cnt = 0;

	memset( &tm, 0, sizeof(tm) );

	if ( _af_daemon->timers.fd < 0 )
	{
		// Initialize the timerfd.
		_af_daemon->timers.timeout.tv_sec = 0;
		_af_daemon->timers.fd = timerfd_create( CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK );
		if ( _af_daemon->timers.fd < 0 )
		{
			af_log_print( LOG_ERR, "Failed timerfd_create() errno %d (%s)", errno, strerror(errno) );
			return;
		}
		// clear off the expire count from the fd.
		while ( read( _af_daemon->timers.fd, (void *)&exp_cnt, sizeof(exp_cnt) ) > 0 );

		af_poll_add( _af_daemon->timers.fd, POLLIN, _af_timer_handle_event, (void *)NULL );
	}

	// Start the timer if there are any on the list.
	if ( _af_daemon->timers.head )
	{
		timer = _af_daemon->timers.head;

		// Restart the timerfd if it's stopped or there is a shorter timer
		if ( (_af_daemon->timers.timeout.tv_sec == 0) || 
			 (timediff(_af_daemon->timers.timeout, timer->timeout) > 0) )
		{
			// Stop the timer if it is running.
			af_log_print( APPF_MASK_TIMER+LOG_DEBUG, "Timer FD stopped for fd %d timeout %ld.%09ld", _af_daemon->timers.fd, _af_daemon->timers.timeout.tv_sec, _af_daemon->timers.timeout.tv_nsec );
			timerfd_settime( _af_daemon->timers.fd, 0, &tm, NULL );

			_af_daemon->timers.timeout = timer->timeout;
	
			// compute the interval
			af_timer_now( &now );
			sec = timer->timeout.tv_sec - now.tv_sec;
			nsec = timer->timeout.tv_nsec - now.tv_nsec;
			if ( nsec < 0 )
			{
				sec--;
				nsec += 1000000000;
			}
	
			// Don't allow a 0 timer, just set it for 1 usec.
			if ( (sec < 0) || ((sec == 0) && (nsec <= 0)) )
			{
				sec = 0;
				nsec = 1000;
			}
	
			tm.it_value.tv_sec = sec;
			tm.it_value.tv_nsec = nsec;
	
			// start a relative time out
			if ( timerfd_settime( _af_daemon->timers.fd, 0, &tm, NULL ) < 0 )
			{
				af_log_print( LOG_ERR, "Failed timerfd_settime() errno %d (%s)", errno, strerror(errno) );
				return;
			}
			af_log_print( APPF_MASK_TIMER+LOG_DEBUG, "Timer fd %d started %ld.%09ld at %ld.%09ld timeout %ld.%09ld", _af_daemon->timers.fd, tm.it_value.tv_sec, tm.it_value.tv_nsec, now.tv_sec, now.tv_nsec, timer->timeout.tv_sec, timer->timeout.tv_nsec );
		}
	}
	else
	{
		// Stop the timer no timers are in the list.
		af_log_print( APPF_MASK_TIMER+LOG_DEBUG, "Timer FD stopped for fd %d timeout %ld.%09ld", _af_daemon->timers.fd, _af_daemon->timers.timeout.tv_sec, _af_daemon->timers.timeout.tv_nsec );
		timerfd_settime( _af_daemon->timers.fd, 0, &tm, NULL );
		_af_daemon->timers.timeout.tv_sec = 0;
	}
jck */
}

void _af_timer_handle_event( af_poll_t *ap )
{
	struct timespec  now;
	long             diff;
	uint64_t         exp_cnt = 0;

	af_timer_now( &now );

	if ( ap->revents & POLLIN )
	{
		// clear off the expire count from the fd.
		while ( read( _af_daemon->timers.fd, (void *)&exp_cnt, sizeof(exp_cnt) ) > 0 )
		{
			if ( exp_cnt != 1 )
			{
				af_log_print( APPF_MASK_TIMER+LOG_NOTICE, "Timer callback fd %d, expired count = %"PRIu64"", ap->fd, exp_cnt );
			}
		}
	}
	else if ( ap->revents )
	{
		// socket error
		af_log_print( LOG_ERR, "poll error, event %d error (%d) %s on timer fd %d TIMER STOPPED!", ap->revents, errno, strerror(errno), _af_daemon->timers.fd );
		af_timer_reset_fd( );
        return;
	}

	// Looks like the fd can have POLLIN set without any expiration on the fd.
	if ( exp_cnt != 1 )
	{
		af_log_print( APPF_MASK_TIMER+LOG_NOTICE, "Timer callback revents %d fd %d, expired count = %"PRIu64" ", ap->revents, ap->fd, exp_cnt );
		return;
	}

	// Warn the user if the time out is off by more than .1 seconds
	diff = (now.tv_sec - _af_daemon->timers.timeout.tv_sec) * 1000;
	diff += (now.tv_nsec - _af_daemon->timers.timeout.tv_nsec) / 1000000;
	if ( diff > 100 || diff < -100 )
	{
		af_log_print( APPF_MASK_TIMER+LOG_NOTICE, "Timeout off by %ld msec,  now %ld:%ld , timeout should be %ld:%ld",
					  diff,
					  now.tv_sec,now.tv_nsec,
					  _af_daemon->timers.timeout.tv_sec, _af_daemon->timers.timeout.tv_nsec );
	}

	af_log_print( APPF_MASK_TIMER+LOG_DEBUG, "Timer event fd %d, expired %"PRIu64" at %ld.%09ld timeout %ld.%09ld", ap->fd, 
			exp_cnt, now.tv_sec, now.tv_nsec, _af_daemon->timers.timeout.tv_sec, _af_daemon->timers.timeout.tv_nsec );

	// Check for any expired timers.
	af_timer_check();

	// Mark the fd as stopped.
	_af_daemon->timers.timeout.tv_sec = 0;

	// Restart the fd.
	af_timer_reset_fd( );
}

void af_timer_start( af_timer_t *timer )
{
	af_timer_t      *tm; 
	af_timer_t      *ntm;
	struct timespec  now;
	long             sec;

	/* make sure it's not in the list first */
	if ( timer->running )
	{
		af_timer_stop( timer );
	}

	/* set timeout */
	af_timer_now( &now );

	sec = timer->sec;
	timer->timeout.tv_nsec = now.tv_nsec+timer->nsec;
	if ( timer->timeout.tv_nsec > 1000000000 )
	{
		sec++;
		timer->timeout.tv_nsec -= 1000000000;
	}
	timer->timeout.tv_sec = now.tv_sec + sec;

	timer->next = NULL;
	timer->running = TRUE;

	/* insert this timer in timeout order */
	if ( _af_daemon->timers.head == NULL || timediff(timer->timeout, _af_daemon->timers.head->timeout) <= 0 )
	{
		/* its the only or less than the head */
		timer->next = _af_daemon->timers.head;
		_af_daemon->timers.head = timer;
		// Fire up the timerfd if we have a new head.
		af_timer_reset_fd( );
	}
	else
	{
		/* search for time order insertioni */
		tm = _af_daemon->timers.head;
		ntm = tm->next;
		while( ntm != NULL )
		{
			if ( timediff( timer->timeout, ntm->timeout ) <= 0 )
			{
				/* this timer is less than the nexti */
				break;
			}
			tm = ntm;
			ntm = tm->next;
		}
		/* insert the timer here */
		timer->next = ntm;
		tm->next = timer;
	}
	af_log_print( APPF_MASK_TIMER+LOG_DEBUG, "Timer started at %ld.%09ld timeout %ld.%09ld", now.tv_sec, now.tv_nsec, timer->timeout.tv_sec, timer->timeout.tv_nsec );

}

void af_timer_stop( af_timer_t *timer )
{
	af_timer_t           *tm, *ntm;

	if ( timer->running == FALSE )
	{
		return;
	}

	// list empty ?
	if ( _af_daemon->timers.head != NULL )
	{
		// just take it out of the list
		if ( timer == _af_daemon->timers.head )
		{
			_af_daemon->timers.head = timer->next;
			timer->next = NULL;
			timer->running = FALSE;
		}
		else
		{
			tm = _af_daemon->timers.head;
			ntm = tm->next;
			while ( ntm != NULL )
			{
				if ( timer == ntm )
				{
					// found it, remove it
					tm->next = ntm->next;
					timer->next = NULL;
					timer->running = FALSE;
					break;
				}
				tm = ntm;
				ntm = tm->next;
			}
		}
	}

	/* are we running from a callback ? */
	if ( _af_daemon->timers.expired != NULL )
	{
		/* clear timer from expired list if it's waiting to be ran */ 
		if ( timer == _af_daemon->timers.expired )
		{
			_af_daemon->timers.expired = _af_daemon->timers.expired->next;
			timer->next = NULL;
			timer->running = FALSE;
		}
		else
		{
			tm = _af_daemon->timers.expired;
			ntm = tm->next;
			while ( ntm != NULL )
			{
				if ( timer == ntm )
				{
					/* found it, remove it from expired list */
					tm->next = ntm->next;
					timer->next = NULL;
					timer->running = FALSE;
					break;
				}
				tm = ntm;
				ntm = tm->next;
			}
		}
	}
	af_log_print( APPF_MASK_TIMER+LOG_DEBUG, "Timer stopped timeout %ld.%09ld", timer->timeout.tv_sec, timer->timeout.tv_nsec );
}


