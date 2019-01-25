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

#define		MAX_FDS      256

int af_poll_run( int timeout )
{
	int           ret;
	int           numfds, idx;
	struct pollfd pfds[MAX_FDS];
	af_poll_t     apfds[MAX_FDS];
	af_poll_t    *ppfd;

	if ( _af_daemon->poll_head == NULL )
	{
		return 0;
	}

	numfds = 0;
	ppfd = _af_daemon->poll_head;
	while ( ppfd && (numfds < MAX_FDS) )
	{
		pfds[numfds].fd = ppfd->fd;
		pfds[numfds].events = ppfd->events;
		apfds[numfds] = *ppfd;

		numfds++;
		ppfd = ppfd->next;
	}

	// Main poll
	ret = poll( pfds, numfds, timeout );

	if ( ret > 0 )
	{
		for ( idx = 0; (idx < numfds); idx++ )
		{
			// check for events
			if ( pfds[idx].revents )
			{
				// Callback
				apfds[idx].revents = pfds[idx].revents;
				apfds[idx].callback( &apfds[idx] );
			}
		}
	}
	if ( ret < 0 )
	{
		// check for signal
		if ( errno != EINTR )
		{
			// poll failed.
		}
		else
		{
			// Signal caught. keep going.
			ret = 0;
		}
	}

	return ret;
}

int af_poll_add( int fd, int events, void (*callback)(af_poll_t *), void *ctx )
{
	int        cnt = 0;
	af_poll_t *pap;

	pap = _af_daemon->poll_head;
	while ( pap )
	{
		if ( pap->fd == fd )
		{
			af_log_print( APPF_MASK_MAIN+LOG_INFO, "Add poll fd %d, Already on the list", 
						  fd );
			return -2;
		}
		cnt++;
		pap = pap->next;
	}

	if ( cnt >= MAX_FDS )
	{
		af_log_print( LOG_WARNING, "Add poll fd %d, Failed. Too MANY fds %d.", 
					  fd, MAX_FDS );
		return -1;
	}

	// Make a new one and add it to the list
	pap = malloc( sizeof( af_poll_t ) );

	pap->fd = fd;
	pap->events = events;
	pap->callback = callback;
	pap->context = ctx;

	// Add it to the head of the list
	pap->next = _af_daemon->poll_head;
    _af_daemon->poll_head = pap;

	return 0;
}

void af_poll_rem( int fd )
{
	af_poll_t    *pap, *nap;

	pap = _af_daemon->poll_head;

	// list is not empty
	if ( pap )
    {
    	if ( _af_daemon->poll_head->fd == fd )
    	{
    		_af_daemon->poll_head = _af_daemon->poll_head->next;
    		free( pap );
    	}
    	else
    	{
    		while( pap )
    		{
    			nap = pap->next;
    
    			if ( nap && (nap->fd == fd) )
    			{
    				pap->next = nap->next;
    				free( nap );
    				break;
    			}
    			pap = nap;
    		}
    	}
    }
}


