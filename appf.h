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

/*
 * libappf API
 *   Main sections:
 *      daemonizing     API
 *      logging         API
 *      poll dispatch   API
 *
 *   Add ons:				   usage
 *      timers          API	   poll / logging
 *      telnet server   API    poll / logging
 *      telnet client   API    poll / logging
 *      child process   API    poll / logging
 *      
 *
 *
 *
 *
 */

#ifndef APPF_H_INCLUDED
#define APPF_H_INCLUDED 1

#define __STDC_FORMAT_MACROS    1
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <poll.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef CJSON
#include <cJSON.h>
#endif	// CJSON

#ifdef __cplusplus
	extern "C" {
#endif
/*****************************************************
 *
 * defines
 *
 ******************************************************/

#define CORE_LIMIT  20000000    /* 20 Mb */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#define AF_OK           0
#define AF_TIMEOUT     -1
#define AF_ERRNO       -2
#define AF_SOCKET      -3
#define AF_BUFFER      -4

#define MAX_PROMPT		32

#define APPF_MASK_MAIN   0x80000000
#define APPF_MASK_TIMER  0x40000000
#define APPF_MASK_SERVER 0x20000000
#define APPF_MASK_CLIENT 0x10000000


/* struct timespec difference in msec. */
#define timediff( x, y )	((x.tv_sec<y.tv_sec)?0:( (x.tv_sec-y.tv_sec>100000000)?0:( (x.tv_sec-y.tv_sec)*1000+(x.tv_nsec-y.tv_nsec)/1000000) ) )

/*****************************************************
 *
 * typedefs
 *
 ******************************************************/
struct _af_timer_control_s;
struct _af_daemon_s;
typedef struct _af_timer_s {
	struct _af_timer_s        *next;
	// User data
	long                       sec;
	long                       nsec;
    void                     (*callback)( struct _af_timer_s * );
	void                      *context;
	// Internal data
	int                        running;      // Is this timer running.
	struct timespec            timeout;      // When this timer should timeout.

//	struct _af_daemon_s       *daemon;
} af_timer_t;

typedef struct _af_timer_control_s
{
	af_timer_t       *head;
	af_timer_t       *expired;

	int               fd;          // timerfd
	struct timespec   timeout;     // when we expect a callback for timerfd 

} af_timer_control_t;

typedef struct _af_poll_s {
	struct _af_poll_s *next;

	int                fd;
	int                events;
	int                revents;
	void              *context;
	void             (*callback)( struct _af_poll_s * );

} af_poll_t;

typedef struct _af_daemon_s {
	// daemon stuff
	char                 *appname;
	int                   daemonize;
	char                 *pid_file;
	void                (*sig_handler)(int);

	// Log stuff
	char                 *log_name;
	int                   use_syslog;
	int                   log_level;
	int                   log_mask;

	FILE                 *log_fh;
	char                 *log_filename;

	// Timer stuff
	af_timer_control_t    timers;

	// Poll dispatch stuff
	af_poll_t            *poll_head;

} af_daemon_t;

typedef struct _af_server_s af_server_t;
typedef struct _af_client_s af_client_t;
typedef struct _af_server_cnx_s af_server_cnx_t;

struct _af_server_cnx_s {
	struct _af_server_cnx_s *next;
	// User data
	void                    *user_data;           /* Opaque pointer to user data */
	void                   (*disconnect_callback)( af_server_cnx_t *cnx );
	struct sockaddr_in       raddr;               /* Remote socket address */

	// Internal Data
	af_server_t             *server;              /* Pointer back to server struct */
	int                      fd;                  /* Connection file descriptor */
	FILE                    *fh;                  /* Connection file handle */
	int                      inout;               // flag to indicate if it is a cnx as a client or server
	af_client_t             *client;              // Pointer back to client struct

};

struct _af_server_s {
	// User set data
	char            *service;  // Specifiy /etc/services name
	char            *prompt;   // Or set prompt and port*/
	int              port;
	int              local;    // set try to bind to INADDR_LOOPBACK
	int              max_cnx;  // maximum number of connections
	// Callback for new commands
	void           (*command_handler)( char *command, af_server_cnx_t *cnx );      
	// Callback for new connections
	void           (*new_connection_callback)( af_server_cnx_t *cnx, void *ctx );
	void            *new_connection_context;

	// Internal data
	int              fd;
	int              num_cnx;
	af_server_cnx_t *cnx;	    // Connections

};


struct _af_client_s
{
	char                *service;  // Specifiy /etc/services name
	int                  port;     // TCP port
	unsigned int         ip;       // Remote IP

	int                  sock;     // Connection

	// Prompt detection
	char                 prompt[MAX_PROMPT];
	int                  prompt_len;
	char                 saved[MAX_PROMPT];
	int                  saved_len;
	void				*extra_data;
	int					 filter_telnet;

	struct _af_client_s *next;

};

typedef struct _af_client_cmd_s {
	struct _af_client_s *next;

} af_client_cmd_t;

typedef struct _af_child_s {
	const char         *command;
	pid_t               pid;
	int                 timeout;
	char               *result;
} af_child_t;

typedef struct _af_cfg_file_s {
	struct _af_cfg_file_s *next;

} af_cfg_file_t;

/*****************************************************
 *
 * Main section: daemonizing, logging, poll
 *
 ******************************************************/
extern af_daemon_t *_af_daemon;

af_daemon_t *af_daemon_set( af_daemon_t *ctx );
int af_daemon_start( void );

void af_fatal( const char *fmt, ... ) __attribute__((format(printf, 1,2)));
void af_log_print( unsigned int mask, const char *fmt, ... ) __attribute__((format(printf, 2,3)));

int af_poll_run( int timeout );
int af_poll_add( int fd, int events, void (*callback)(af_poll_t *), void *ctx );
void af_poll_rem( int fd );

void af_open_logfile(void);
void af_close_logfile(void);


/*****************************************************
 *
 * Add ons section: timers, telnet server, telnet client
 *
 ******************************************************/

// Timer
void af_timer_now( struct timespec *now );
time_t *af_timer_curtime( struct timespec *then );
void af_timer_start( af_timer_t *timer );
void af_timer_stop( af_timer_t *timer );

// TCLI server
int af_server_get_port( const char *service );
char *af_server_get_prompt( const char *service );
int af_server_start( af_server_t *server );
void af_server_stop( af_server_t *server );
void af_server_disconnect_all( af_server_t *server );

void af_server_disconnect( af_server_cnx_t *cnx );
void af_server_prompt( af_server_cnx_t *cnx );

// TCLI client
af_client_t *af_client_new( char *service, unsigned int ip, int port, const char *prompt );
void af_client_delete( af_client_t *client );
int af_client_connect( af_client_t *client );
void af_client_disconnect( af_client_t *client );
int af_client_read_socket( af_client_t *cl, int *len, char **pptr, int *prlen );
int af_client_read_timeout( af_client_t *cl, char *buf, int *len, int timeout );
int af_client_send( af_client_t *cl, char *cmd );
int af_client_send_raw( af_client_t *cl, unsigned char *cmd, size_t	cmd_len );
int af_client_read_raw_timeout( af_client_t *cl, char *buf, int *len, int timeout );

#define af_client_get_prompt( x, y ) af_client_read_timeout( x, NULL, NULL, y )

// fork, exec and child
int af_exec_fork( void );
int af_exec_child( af_child_t *child );
int af_exec_fork_child( af_child_t *child );
int af_exec_is_running( const char *pid_path, const char *name );

void af_exec_to_buf(char *buf, int size, int timeout, const char *cmd);
void af_exec_to_fd(int sock, int timeout, const char *cmd);

// argc, argv
#define af_argv( x, y ) af_parse_argv( x, y, sizeof(y)/sizeof(char*) )
int af_parse_argv( char *buf, char **argv, int max );
int af_parse_cfg_file( af_cfg_file_t *file );
int af_parse_free( af_cfg_file_t *file );


#ifdef __cplusplus
}
#endif
#endif // APPF_H_INCLUDED
