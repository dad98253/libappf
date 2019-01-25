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

void _af_server_handle_new_connection( af_poll_t *ap );

int af_server_get_port( const char *service )
{
	struct servent    *servEnt = NULL;

	if ( service == NULL )
	{
		return -EINVAL;
	}

	if ( ( servEnt = getservbyname( service, "tcp") ) == NULL )
	{
		af_log_print( LOG_ERR, "%s: Couldn't find the TCP port id for service %s in /etc/services. error(%d)",
				     __func__, service, errno);
		return -1;
	}

	return( ntohs( servEnt->s_port ) );
}

char *af_server_get_prompt( const char *service )
{
	int                i = 0;
	char              *alias = NULL;
	static char        prompt[64] = { 0 };
	struct servent    *servEnt = NULL;

	if( service == NULL )
	{
		return NULL;
	}

	if ( ( servEnt = getservbyname( service, "tcp" ) ) == NULL )
	{
		/* doh!!! */
		af_log_print(LOG_ERR, "%s: Couldn't find the TCP port id for service %s in /etc/services. error(%d)",
				 __func__, service, errno);
		return NULL;
	}

	while ( ( alias = servEnt->s_aliases[i++] ) != NULL )
	{
		if ( alias[strlen(alias)-1] == '>' )
		{
			strncpy( prompt, alias, sizeof(prompt)-1 );
			break;
		}
	}

	if ( prompt[0] == '\0' )
	{
		snprintf( prompt, sizeof(prompt), "%s>", servEnt->s_name );
	}

	return( prompt );
}

static int af_server_set_sockopts( int s, int server_sock )
{
	int              val;
	struct linger    ling;

	/* set closexec so children don't close our sockets */
	if ( fcntl( s, F_SETFD, FD_CLOEXEC ) != 0 )
	{
		/* complain but don't fail */
		af_log_print( LOG_WARNING, "%s: fcntl(F_SETFD, FD_CLOEXEC) failed for fd=%d errno=%d (%s)",
			__func__, s, errno, strerror(errno) );
	}

	/* set nodelay to not buffer and send immediately. */
	val = 1;
	if ( setsockopt( s, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val) ) < 0 )
	{
        close( s );

		af_log_print( LOG_ERR, "%s: setsockopt(TCP_NODELAY) failed for fd=%d errno=%d (%s)",
			__func__, s, errno, strerror(errno) );

		return -1;
	}

	if ( server_sock )
	{
		/* set reuse on server socket so we can exit and restart */
		val = 1;
		if ( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val) ) < 0 ) 
		{
			close( s );

			af_log_print( LOG_ERR, "%s: setsockopt(SO_REUSEADDR) failed for fd=%d errno=%d (%s)",
				__func__, s, errno, strerror(errno) );

			return -1;
		}
	}
	else
	{
		/* set to non-blocking so close doesn't linger. */
		ling.l_onoff = 1;
		ling.l_linger = 2;
		if ( setsockopt( s, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling) ) != 0 )
		{
			af_log_print( LOG_ERR, "%s: setsockopt(SO_LINGER) failed for fd=%d errno=%d (%s)",
					__func__, s, errno, strerror(errno) );

			return -1;
		}
	}

	return 0;
}

af_server_cnx_t *_af_server_add_connection( af_server_t *server )
{
	int                 s, fd_dup;
	af_server_cnx_t    *cnx = NULL;
	struct sockaddr_in  raddr;
    socklen_t           rlen;

	s = accept( server->fd, NULL, NULL );

	if ( s < 0 ) 
	{
		af_log_print(LOG_ERR, "%s: accept() failed (%d) %s",\
			__func__, errno, strerror(errno) );
		return NULL;
	}

	/* quick check to deny before scanning list */
	if ( server->num_cnx >= server->max_cnx )
	{
		close( s );
		af_log_print(LOG_ERR, "%s: rejecting new client connection: max number of sessions (%d) already open",\
			__func__, server->max_cnx );

		return NULL;
	}

	rlen=sizeof(raddr);
	if ( getpeername( s, (struct sockaddr *)&raddr, &rlen ) )
	{
		close( s );
		af_log_print(LOG_ERR, "%s: rejecting new client, getpeername() failed (%d) %s",\
			__func__, errno, strerror(errno) );

		return NULL;
	}

	cnx = (af_server_cnx_t *)calloc( 1, sizeof(af_server_cnx_t) );
	if ( cnx == NULL )
	{
		close(s);
		af_log_print( LOG_ERR, "Failed to allocate memory for new connection");
		return NULL;
	}

	/**
	 * Client socket
	 */
	if ( af_server_set_sockopts( s, 0 ) != 0 )
	{
		close(s);
		free(cnx);
		return NULL;
	}

	/**
	 * Dup client socket for file handle
	 */
	if ( ( fd_dup = dup( s ) ) < 0 )
	{
		close( s );
		free(cnx);
		af_log_print(LOG_ERR, "%s: dup() failed on fd=%d, errno=%d (%s)", __func__, s, errno, strerror(errno) );

		return NULL;
	}

	if ( af_server_set_sockopts( fd_dup, 0 ) != 0 )
	{
		close( s );
		free(cnx);
		return NULL;
	}

	if ( ( cnx->fh = fdopen( fd_dup, "w+"  ) ) == NULL )
	{
		close( s );
		close( fd_dup );
		free(cnx);

		af_log_print( LOG_CRIT, "%s: fdopen() failed for fd=%d", __func__, fd_dup );

		return NULL;
	}

	/* set handle to line buffered mode */
	setlinebuf( cnx->fh );

	cnx->fd = s;
	cnx->raddr = raddr;
	cnx->server = server;

	// Add to the server list
	cnx->next = server->cnx;
	server->cnx = cnx;
	server->num_cnx++;

	return cnx;
}


void af_server_prompt( af_server_cnx_t *cnx )
{
	if ( cnx && (cnx->fd >= 0) && cnx->server )
	{
		// Make sure the stream is flushed. (Not sure this is necessary.)
		if ( cnx->fh )
			fflush( cnx->fh );

		send( cnx->fd, cnx->server->prompt, strlen(cnx->server->prompt), 0 );

		af_log_print(APPF_MASK_SERVER+LOG_DEBUG, "dcli prompt %s sent", cnx->server->prompt );
	}
}

void _af_server_rem_instance( af_server_cnx_t *cnx )
{
	af_server_cnx_t *pcnx;

	if ( cnx == NULL )
		return;

	if ( cnx->fh )
		fclose( cnx->fh );

	close( cnx->fd );

	// remove the connection from the server.
	pcnx = cnx->server->cnx;
	if ( cnx == pcnx )
	{
		cnx->server->cnx = cnx->next;
		cnx->server->num_cnx--;
	}
	else
	{
		// Find it in the server list and remove
		while( pcnx )
		{
			if ( pcnx->next == cnx )
			{
				pcnx->next = cnx->next;
				cnx->server->num_cnx--;
				break;
			}
			pcnx = pcnx->next;
		}
	}

	free(cnx);
}

void af_server_disconnect( af_server_cnx_t *cnx )
{
	if ( cnx == NULL )
		return;

	af_poll_rem( cnx->fd );

	// Call users disconnection callback
	if ( cnx->disconnect_callback )
	{
		cnx->disconnect_callback( cnx );
	}

	// remove the client from our client list
	af_log_print(APPF_MASK_SERVER+LOG_DEBUG, "dcli client disconnected fd %d", cnx->fd);

	// close the client socket
	_af_server_rem_instance(cnx);
}

void _af_server_add_service( char *service, int port, char *prompt )
{
	FILE *fh;

	fh = fopen( "/etc/services", "a" );

	if ( fh )
	{
		fprintf( fh, "%-15s %d/tcp       %s\n", service, port, prompt );
		fclose( fh );
	}
}

int af_server_start( af_server_t *server )
{
	int                   s;
	int                   port;
	char                 *p;
	struct sockaddr_in    sin;

	if ( server->service != NULL )
	{
		port = af_server_get_port( server->service );
		p = af_server_get_prompt ( server->service );
		if ( port == 0 || p == NULL )
		{
			if ( server->port && server->prompt )
			{
				af_log_print( LOG_NOTICE, "%s not found in /etc/services, adding", server->service );
				_af_server_add_service( server->service, server->port, server->prompt );

			}
			else
			{
				af_log_print( LOG_ERR, "%s not found in /etc/services. TCLI Server NOT started", server->service );
				return -1;
			}
		}
		else
		{
			server->port = port;
			server->prompt = strdup(p);
		}
	}
	if ( (server->port == 0) || (server->prompt == NULL) )
	{
		af_log_print( LOG_ERR, "Server port or prompt not found. Server not started." );
		return -1;
	}

	/* Get socket */
	if ( ( s = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ) ) < 0 )
	{
		af_log_print( LOG_ERR, "%s: socket() failed errno=%d (%s)",
			__func__, errno, strerror(errno) );

		return -1;
	}

	if ( af_server_set_sockopts( s, 1 ) != 0 )
	{
		return -1;
	}
	
	memset( &sin, '\0', sizeof(sin) );

	sin.sin_family = AF_INET;
	if ( server->local )
	{
		sin.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
	}
	else
	{
		sin.sin_addr.s_addr = htonl( INADDR_ANY );
	}

	sin.sin_port = htons( server->port );

	if ( bind( s, (struct sockaddr*)&sin, sizeof(struct sockaddr) ) < 0 )
	{
		close( s );

		af_log_print( LOG_ERR, "bind() failed for port %d, fd=%d errno=%d (%s)",
					  server->port, s, errno, strerror(errno) );

		return -1;
	}

	/* Listen for incomming connections */
	if ( listen( s, server->max_cnx ) != 0 ) 
	{
		close( s );

		af_log_print( LOG_ERR, "%s: listen() failed for fd=%d errno=%d (%s)",
			__func__, s, errno, strerror(errno) );

		return -1;
	}

	server->fd = s;
	server->num_cnx = 0;
	server->cnx = NULL;

	// Add pollfd for new connections
	af_poll_add( server->fd, (POLLIN|POLLPRI), _af_server_handle_new_connection, (void*)server );

	return 0;

}

void af_server_disconnect_all( af_server_t *server )
{
	// close all connections
	while ( server->cnx )
	{
		af_server_disconnect( server->cnx );
	}
}

void af_server_stop( af_server_t *server )
{
	af_server_disconnect_all( server );

	af_poll_rem( server->fd );

	close( server->fd );

	server->fd = -1;
}


void _af_server_cnx_handle_event( af_poll_t *ap )
{
	int              len = 0;
	char             buf[2048];
	af_server_cnx_t *cnx = (af_server_cnx_t *)ap->context;

	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );

				af_server_disconnect(cnx);
			}
		}
		else
		{
			// terminate the read data
			buf[len] = 0;

			if ( cnx->server->command_handler )
			{
				af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "DCLI server command [%s]", buf );
				cnx->server->command_handler( buf, cnx );
			}
		}
	}
	else if ( ap->revents )
	{
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "dcli socket error, revents: %d", ap->revents );
		af_server_disconnect(cnx);
	}
}


/*******************************************************************************
 *
 *                                   handle_new_client_connection()
 *
 ***************************************************************************//**
 *
 * \brief
 * 	This function handles activity on the master DCLI socket.
 *
 *
 * \details
 * 	Accepts new connections and starts the client socket.
 *
 *
 ******************************************************************************/
void _af_server_handle_new_connection( af_poll_t *ap )
{
	af_server_t     *serv = (af_server_t*)ap->context;
	af_server_cnx_t *cnx;

	if ( ( cnx = _af_server_add_connection( serv ) ) != NULL )
	{
		af_log_print(APPF_MASK_SERVER+LOG_INFO, 
                     "accepted new client connection (fd=%d)", 
                     cnx->fd);

		/* add new client fd to the pollfd list */
		af_poll_add( cnx->fd, POLLIN, _af_server_cnx_handle_event, cnx );

		// Call the user's new connection callback
		if ( serv->new_connection_callback )
		{
			serv->new_connection_callback( cnx, serv->new_connection_context );
		}

		/* send dcli prompt to client */
		af_server_prompt( cnx );
	}
	else
	{
        /* Mask this - its legal to refuse connections at boot time */
        /* We end up filling processes logs with this if we leave   */
        /* without the mask                                         */
		af_log_print(LOG_ERR, 
                     "failed to accept new client connection");
	}
}

