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

#define MAXDECODE	250

typedef struct _comport {
	int              fd;
	char            *dev;
	int              speed;
	char            *logfile;
	FILE            *logfh;
	int              tcpport;
	af_server_t      comserver;
	af_server_cnx_t *cnx;
	int              telnet_state;
	int		 		 inout;
	char			*remote;
	af_client_t      comclient;
	char			*prompt;
	char			*commands;
	char			*password;
	int				numprompts;
	unsigned int 	connect_timo;	/* connect timeout */
	unsigned int 	cmd_timo;		/* command timeout */
	char			decoded[MAXDECODE];
} comport;


int _af_client_connect_timeout( af_client_t *client, int timeout_msec )
{
	int                 flags, error, c, ret = 0;
	socklen_t           len = sizeof( error );
	struct pollfd       pfds[1];
	struct sockaddr_in  addr;

	// Save the socket flags
	if ( (flags = fcntl(client->sock, F_GETFL, 0)) < 0 )
	{
		af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "%s: sock %d fcntl GETFL FAILED (%d) %s", __func__, client->sock, errno, strerror(errno) );
		return -1;
	}

	// Set the socket for non-blocking
	if ( fcntl(client->sock, F_SETFL, flags | O_NONBLOCK) < 0 )
	{
		af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "%s: sock %d fcntl SETFL FAILED (%d) %s", __func__, client->sock, errno, strerror(errno) );
		return -1;
	}

	//initiate non-blocking connect
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl( client->ip );
	addr.sin_port = htons( client->port );

	ret = connect( client->sock, (struct sockaddr *)&addr, (socklen_t)sizeof(addr) );

	if ( ret == 0 )	   //then connect succeeded right away
		goto done;

	if ( ret < 0 )
	{
		// Check for any error except would block
		if ( errno != EINPROGRESS )
		{
			af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "%s: connect() sock %d FAILED (%d) %s", __func__, client->sock, errno, strerror(errno) );
			goto done;
		}
	}

	ret = 0;
	// Lets block for the timeout
	pfds[0].fd = client->sock;
	pfds[0].events = POLLOUT | POLLIN | POLLERR;
	pfds[0].revents = 0;

	c = poll( pfds, 1, timeout_msec );

	if ( c < 0 )
	{
		// Poll failed
		ret = -1;
		af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "%s: poll sock %d failed (%d) %s", __func__, client->sock, errno, strerror(errno) );
	}
	else if ( c == 0 )
	{
		//we had a timeout
		errno = ETIMEDOUT;
		ret = -1;
		af_log_print(APPF_MASK_CLIENT+LOG_INFO, "%s: poll sock %d timed out", __func__, client->sock );
	}
	else
	{
		// Got something on the socket check for an error
		if ( getsockopt(client->sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 )
		{
			af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "%s: getsockopt() sock %d failed (%d) %s", __func__, client->sock, errno, strerror(errno) );
			ret = -1;
		}
		else if ( error )
		{
			af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "%s: getsockopt() sock %d returned (%d) %s", __func__, client->sock, errno, strerror(errno) );
			errno = error;
			ret = -1;
		}
		// Else no error, it must have suceeded
	}

	done:
	//put socket back in blocking mode (if set)
	if ( fcntl(client->sock, F_SETFL, flags ) < 0 )
	{
		af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "%s: sock %d fcntl SETFL FAILED (%d) %s", __func__, client->sock, errno, strerror(errno) );
		ret = -1;
	}

	return ret;
}

void af_client_disconnect( af_client_t *client )
{
	if (client->sock >= 0 )
	{
		close( client->sock );
		client->sock = -1;
	}
}

int af_client_connect( af_client_t *client )
{
	if ( client->sock < 0 )
	{
		client->sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if ( client->sock < 0 )
		{
			return -1;
		}
	}
	return _af_client_connect_timeout( client, 1000 );
}

void af_client_delete( af_client_t *client )
{
	if ( !client )
	{
		return;
	}

	if ( client->service )
	{
		free( client->service );
		client->service = NULL;
	}

	if ( client->sock >= 0 )
	{
		close( client->sock );
	}

	free( client );
	client = NULL;
}

af_client_t *af_client_new( char *service, unsigned int ip, int port, const char *prompt )
{
	const char     *server_prompt = NULL;
	af_client_t    *client = calloc( 1, sizeof(af_client_t) );

	if( client != NULL )
	{
		client->sock = -1;
		client->ip = ip;
		client->port = port;

		if ( prompt )
		{
			strncpy( client->prompt, prompt, MAX_PROMPT );
		}

		if ( service )
		{
			client->service = strdup(service);

			if ( !port )
			{
				client->port = af_server_get_port( service );
			}

			if ( !prompt )
			{
				server_prompt = af_server_get_prompt( service );
				if ( !server_prompt )
				{
					free( client );
					client = NULL;
					return ( NULL );
				}

				strncpy( client->prompt, server_prompt, MAX_PROMPT );
			}
		}

		client->prompt_len = strlen( client->prompt );
	}

	return( client );
}


int _af_client_prompt_detect( af_client_t *det, char *buf, int *len )
{
	int   i;
	int   prompt_idx;
	char *ptr;

	if ( *len >= det->prompt_len )
	{
		// If we get at least prompt length bytes
		// just copy the last prompt bytes to our buffer
		ptr = &buf[*len - det->prompt_len];
		det->saved_len = det->prompt_len;
	}
	else
	{
		// We got less than prompt bytes
		// Copy what we got
		ptr = buf;
		det->saved_len = *len;
	}
	// Copy to our saved buffer
	for ( i=0; i<det->saved_len; i++ )
	{
		det->saved[i] = *ptr++;
	}

	af_log_print(APPF_MASK_CLIENT+LOG_INFO, "Prompt detection start--> buffer len %d (det->saved=\"%s\", det->saved_len=%d)", *len, det->saved, det->saved_len );

	// Look for the prompt beginning at each saved character
	for ( prompt_idx = 0; prompt_idx<det->saved_len; prompt_idx++ )
	{
		af_log_print(APPF_MASK_CLIENT+LOG_INFO, "Try prompt_idx=%d, saved=[%c]", prompt_idx, det->saved[prompt_idx] );

		for ( i=0; i<det->saved_len-prompt_idx; i++ )
		{
			if (det->saved[i+prompt_idx] == det->prompt[i])
			{
				// Current saved == current prompt
				af_log_print(APPF_MASK_CLIENT+LOG_INFO, "Matched prompt[%d] saved=[%c]", i, det->saved[i+prompt_idx]);
			}
			else
			{
				af_log_print(APPF_MASK_CLIENT+LOG_INFO, "No match prompt[%d] saved=[%c] != [%c]", i, det->saved[i+prompt_idx], det->prompt[i]);
				break;
			}
		}
		if ( i == det->saved_len-prompt_idx )
		{
			// we matched from prompt_idx on to the beginning of the prompt
			af_log_print(APPF_MASK_CLIENT+LOG_INFO, "Match Complete %d characters", det->saved_len-prompt_idx);
			break;
		}
	}

	// prompt_idx is the first matching char of the matching string.
	if ( (prompt_idx == 0) && (det->saved_len == det->prompt_len) )
	{
		// We have the whole prompt
		// clear our saved data counter
		det->saved_len = 0;
		// remove the prompt from the data buffer
		*len -= det->prompt_len;
		af_log_print(APPF_MASK_CLIENT+LOG_INFO, "Prompt Matched. Return %d characters", *len );
		return 1;
	}
	//else
	// We have a partial match or no match
	// Copy any match to the beginning of the saved buffer
	for ( i=0; i<(det->saved_len-prompt_idx); i++ )
	{
		det->saved[i] = det->saved[i+prompt_idx];
	}
	// Update the saved data count we need to cache
	det->saved_len = det->saved_len-prompt_idx;

	// remove the saved data from the data buffer
	*len -= det->saved_len;

	af_log_print(APPF_MASK_CLIENT+LOG_INFO, "Prompt NO Match. Return %d characters saved %d", *len, det->saved_len );
	return 0;
}


/* Assumes the POLL has returned POLLIN */
/* if len == NULL && pptr == NULL just use internal buffer and toss results away. */
/* *prlen is the buffer len and returns count of read data */
int af_client_read_socket( af_client_t *cl, int *len, char **pptr, int *prlen )
{
	int   i, rt, rtlen;
	char  rbuf[10240];
	char *ptr;
	int   rlen;

	comport *coms = (comport*)cl->extra_data;

	if ( *len )
	{
		// If we get a buffer then use the pointer and
		ptr  = *pptr;
		rlen = *prlen - 1 - cl->prompt_len;
	}
	else
	{
		ptr  = rbuf;
		rlen = sizeof(rbuf) - 1 - cl->prompt_len;
	}

	// Just in case they passed in a buffer smaller than prompt size
	if ( rlen <= 0 )
	{
		af_log_print(APPF_MASK_CLIENT+LOG_INFO, "do_read buffer too small %d bytes, len %d", rlen, len?*len:0 );
		return AF_BUFFER;
	}

	do
	{
		// Restore the cached partial prompt to the buffer
		af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "restoring cached prompt %d bytes to ptr %p", cl->saved_len, (void *)ptr );
		for ( i=0; i<cl->saved_len; i++ )
		{
			ptr[i] = cl->saved[i];
		}

		// Read new data with cached data already in the buffer
		rt = recv( cl->sock, &ptr[cl->saved_len], rlen-cl->saved_len, MSG_DONTWAIT );
		af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "recv returned %d, for read max %d", rt, rlen-cl->saved_len );
#ifdef TODO
		if (rt == 0 ) break;
		if (strncmp ( (char *)"telnet", cl->service, 6 ) == 0){
			// filter telnet data out
			rt = com_filter_telnet( cl->extra_data,  (unsigned char *)&ptr[cl->saved_len], rt );   ///////////////////////  we need to figure out how to do all of this back in com2net
		}
#endif	// TODO
		if ( rt < 0 )
		{
			if ( errno == EAGAIN || errno == EWOULDBLOCK )
				break;
			else
			{
				// An error occurred.
				return AF_ERRNO;
			}
		}
		else if ( rt == 0 )
		{
			// peer performed an order shutdown
			af_log_print(APPF_MASK_CLIENT+LOG_INFO, "peer %s performed an order shutdown",coms->remote);
			// jck			af_log_print(APPF_MASK_CLIENT+LOG_INFO, "%s has no bytes to process - ignoringn",coms->remote);
			return AF_SOCKET;
			break;
		}
		else // rt > 0
		{
			// We got something
			// free the cache
			rt += cl->saved_len;
			cl->saved_len = 0;

			ptr[rt] = 0;	// NULL terminate

			af_log_print(APPF_MASK_CLIENT+LOG_INFO, "client read bytes %d ptr (%s)", rt, ptr );
			// write to comm
			if ( coms->fd > 0 && strlen(ptr) ) {
				write( coms->fd, ptr, strlen(ptr) );
			}
			rtlen = rt;
			if ( coms->numprompts ) {
				af_log_print(APPF_MASK_CLIENT+LOG_INFO, " -- now check for prompt" );
				if ( _af_client_prompt_detect( cl, ptr, &rtlen ) )
				{
					// found prompt
					if ( len )
					{
						// update length
						*len += rtlen;
						// remove prompt from data.
						ptr[rtlen] = 0;
					}
					(coms->numprompts)--;
					// everything is good
					return AF_OK;
				}
				else // No prompt or partial prompt, keep going.
				{
					if ( len )
					{
						/* update len with new chars read or re-injected from cache */
						*len += rtlen;
					}
				}
			}
			// If we have less than a prompt left, we are full.
			if ( rt >= rlen - 1 - cl->prompt_len )
			{
				if ( len )
					return AF_BUFFER;

				// No buf, just our internal buffer filled. return to re-POLL
				break;
			}
			else // try to get some more..
			{
				// buffer is smaller
				rlen -= rtlen;
				ptr += rtlen;
			}
		} // rt >0

		// Didn't get the prompt or fill the buffer. (might be some data in there.
		*prlen = rlen;
		*pptr = ptr;

	} while ( 0 );

	// Got to return a NOT done code.
	return AF_TIMEOUT;
}

int af_client_read_timeout( af_client_t *cl, char *buf, int *len, int timeout )
{
	int rt, pin;
	int rlen;
	char *ptr;
	struct timespec now, then;
	int tdiff = 0, to;
	struct pollfd pfds[1];

//	af_log_print(APPF_MASK_CLIENT+LOG_INFO, "af_client_read_timeout buf %p len %d timeout %d", (void *)buf, len?*len:0, timeout );
	// Check if the app passed in a buffer
	if ( buf && len )
	{
		ptr = buf;
		rlen = *len - 1 - cl->prompt_len; /* allow data shift up to prompt sz bytes */;
		*len = 0;	///////////////////   WHY?
	}
	else
	{
		ptr = NULL;
		rlen = 0;
	}


	af_timer_now( &then );
	to = timeout;

	do
	{
		pfds[0].fd = cl->sock;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;

		// Can't use <0 timeout or poll goes infinite
		if ( to <= 0 )
			to = 1;

		pin = poll( pfds, 1, to );
		if ( pin > 0 )
		{
			if ( pfds[0].revents & POLLIN )
			{
//				af_log_print(APPF_MASK_CLIENT+LOG_INFO, "do_read buf %p len %d rlen %d", (void *)ptr, len?*len:0, rlen );
				rt = af_client_read_socket( cl, len, &ptr, &rlen );
				if ( rt != AF_TIMEOUT )
					return rt;
			}
			else // revents has no POLLIN
			{
				// Probably kind of socket failure
				return AF_SOCKET;
			}
		}
		else if ( pin < 0 )
		{
			if ( errno != EAGAIN )
				return AF_ERRNO;

		}
		else // pin == 0
		{
			// timed out
		}

		af_timer_now( &now );

		tdiff = timediff(now, then);
		// Check for clock change
		if ( tdiff <= 0 )
		{
			af_timer_now( &then );
			tdiff = 0;
		}

		to = timeout - tdiff;

	} while ( tdiff < timeout );

	// If we had a timeout, we timed out before getting a prompt
	if ( timeout )
	{
		return AF_TIMEOUT;
	}

	return AF_OK;
}



int af_client_send( af_client_t *cl, char *cmd )
{
	int rt;
	size_t	cmd_len, slen;

	slen = cmd_len = strlen(cmd);
	// Check to see if they included the newline and add it if not.
	if ( cmd[cmd_len-1] != '\n' )
	{
		// Should be room where the null is.
		cmd[cmd_len++] = '\n';
	}
	rt = send( cl->sock, cmd, cmd_len, MSG_DONTWAIT );
	// restore the null if we added the \n
	if ( cmd[slen] != 0 )
		cmd[slen] = 0;

	if ( rt < 0 )
	{
		if ( errno == EAGAIN || errno == EWOULDBLOCK )
			return AF_TIMEOUT;
		else
			return AF_ERRNO;

	}
	if ( rt != cmd_len )
	{
		return AF_SOCKET;
	}

	return AF_OK;
}

int af_client_send_raw( af_client_t *cl, unsigned char *cmd, size_t	cmd_len )
{
	int rt;

	rt = send( cl->sock, cmd, cmd_len, MSG_DONTWAIT );

	if ( rt < 0 )
	{
		if ( errno == EAGAIN || errno == EWOULDBLOCK )
			return AF_TIMEOUT;
		else
			return AF_ERRNO;

	}
	if ( rt != cmd_len )
	{
		return AF_SOCKET;
	}

	return AF_OK;
}

int af_client_read_socket_raw( af_client_t *cl, int *len, char **pptr, int *prlen )
{
	int   rt, rtlen;
	char  rbuf[10240];
	char *ptr;
	int   rlen;

	comport *coms = (comport*)cl->extra_data;

	if ( *len )
	{
		// If we get a buffer then use the pointer and
		ptr  = *pptr;
		rlen = *prlen;
	}
	else
	{
		ptr  = rbuf;
		rlen = sizeof(rbuf);
	}

	// Just in case they passed in a buffer smaller than prompt size
	if ( rlen <= 0 )
	{
		af_log_print( LOG_ERR, "do_read buffer too small %d bytes, len %d", rlen, len?*len:0 );
		return AF_BUFFER;
	}

	do
	{
		// Read new data
		rt = recv( cl->sock, ptr, rlen, MSG_DONTWAIT );
		af_log_print(APPF_MASK_CLIENT+LOG_DEBUG, "recv returned %d, for read max %d", rt, rlen );

		if ( rt < 0 )
		{
			if ( errno == EAGAIN || errno == EWOULDBLOCK )
				break;
			else
			{
				// An error occurred.
				return AF_ERRNO;
			}
		}
		else if ( rt == 0 )
		{
			// peer performed an order shutdown
			af_log_print(APPF_MASK_CLIENT+LOG_INFO, "peer %s performed an order shutdown",coms->remote);
			// jck			af_log_print(APPF_MASK_CLIENT+LOG_INFO, "%s has no bytes to process - ignoringn",coms->remote);
			return AF_SOCKET;
			break;
		}
		else // rt > 0
		{
			// We got something
			// free the cache
			rt += cl->saved_len;
			cl->saved_len = 0;

			af_log_print(APPF_MASK_CLIENT+LOG_INFO, "client read bytes %d", rt );
			// write to comm
//			if ( coms->fd > 0 && strlen(ptr) ) {
//				write( coms->fd, ptr, strlen(ptr) );
//			}
			*len = rtlen = rt;


		} // rt >0

		// Didn't get the prompt or fill the buffer. (might be some data in there.
		*prlen = rlen;
		*pptr = ptr;

	} while ( 0 );

	// Got to return a NOT done code.
	return AF_TIMEOUT;
}


int af_client_read_raw_timeout( af_client_t *cl, char *buf, int *len, int timeout )
{
	int rt, pin;
	int rlen;
	char *ptr;
	struct timespec now, then;
	int tdiff = 0, to;
	struct pollfd pfds[1];

//	af_log_print(APPF_MASK_CLIENT+LOG_INFO, "af_client_read_timeout buf %p len %d timeout %d", (void *)buf, len?*len:0, timeout );
	// Check if the app passed in a buffer
	if ( buf && len )
	{
		ptr = buf;
		rlen = *len; /* allow data shift up to prompt sz bytes */;
//		*len = 0;	///////////////////   WHY?
	}
	else
	{
		ptr = NULL;
		rlen = 0;
	}


	af_timer_now( &then );
	to = timeout;

	do
	{
		pfds[0].fd = cl->sock;
		pfds[0].events = POLLIN;
		pfds[0].revents = 0;

		// Can't use <0 timeout or poll goes infinite
		if ( to <= 0 )
			to = 1;

		pin = poll( pfds, 1, to );
		if ( pin > 0 )
		{
			if ( pfds[0].revents & POLLIN )
			{
//				af_log_print(APPF_MASK_CLIENT+LOG_INFO, "do_read buf %p len %d rlen %d", (void *)ptr, len?*len:0, rlen );
				rt = af_client_read_socket_raw( cl, len, &ptr, &rlen );
				if ( rt != AF_TIMEOUT )
					return rt;
			}
			else // revents has no POLLIN
			{
				// Probably kind of socket failure
				return AF_SOCKET;
			}
		}
		else if ( pin < 0 )
		{
			if ( errno != EAGAIN )
				return AF_ERRNO;

		}
		else // pin == 0
		{
			// timed out
		}

		af_timer_now( &now );

		tdiff = timediff(now, then);
		// Check for clock change
		if ( tdiff <= 0 )
		{
			af_timer_now( &then );
			tdiff = 0;
		}

		to = timeout - tdiff;

	} while ( tdiff < timeout );

	// If we had a timeout, we timed out before getting a prompt
	if ( timeout )
	{
		return AF_TIMEOUT;
	}

	return AF_OK;
}


