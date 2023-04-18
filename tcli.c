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
/* Purpose: Telnet CLI transaction application                               */
/*                                                                           */
/*****************************************************************************/


#include <appf.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CMDS		2048
#define MAX_CMD_BUF		4096
#define HEX_PRINT_WIDTH		16
#define DEFAULT_CONNECT_TIMO	5
#define DEFAULT_CMD_TIMO	10
#define DEFAULT_PORT		23


typedef struct _connect
{
	af_client_t *client;	/* af_client_t struct */
	int busy;				/* server state: busy after command sent, idle once prompt detected */
	unsigned int connect_timo;	/* connect timeout */
	unsigned int cmd_timo;		/* command timeout */
} connect_t;


typedef struct _opts
{
    int debug_mode;
    int hide_prompt;
    int dump_hex;
	char delim_char[2];
	char filename[PATH_MAX];
} opt_t;


typedef struct _command
{
    char buf[MAX_CMD_BUF];
    char *part[MAX_CMDS];
    int num;
    int current;
} command_t;

typedef struct _tcli
{
	opt_t opt;
	connect_t conn;
	command_t cmd;
	int bailout;
} tcli_t;


char *version = "1.0";
//extern char *optarg;
//extern int optind, opterr, optopt;

// init defaults
tcli_t tcli = {
	opt:{
		debug_mode:0,
		hide_prompt:0,
		dump_hex:0,
		delim_char:",",
		filename:""
		},
	conn:{ 
		busy:0, 
		connect_timo:DEFAULT_CONNECT_TIMO, 
		cmd_timo:DEFAULT_CMD_TIMO 
		},
	cmd:{
		buf:"",
		part:{NULL}, 
		num:0,
		current:0
		}
};


void usage(void)
{
	fprintf(stderr, "\nUSAGE: tcli [options] <service> ");
	fprintf(stderr, "<command>[,<command>,...]\n\n");
	fprintf(stderr, "         -v          Display version and exit\n");
	fprintf(stderr, "         -h          Display this message\n");
	fprintf(stderr, "         -d          Debug mode\n");
	fprintf(stderr, "         -n          Do NOT display prompt detection\n");
	fprintf(stderr, "         -x          dump data as hex\n");
	fprintf(stderr, "         -s          log to syslog instead of stdout\n");
	fprintf(stderr, "         -l <level>  Specify logger level (1-7)\n");
	fprintf(stderr, "         -m <mask>   Specify logger mask (0-0xfffffff0)\n");
	fprintf(stderr, "         -i <file>   Specify filename to read commands from\n");
	fprintf(stderr, "         -T <timo>   Specify timeout for initial prompt detect in secs (default=%d)\n", DEFAULT_CONNECT_TIMO);
	fprintf(stderr, "         -c <timo>   Specify command timeout in seconds in secs (default=%d)\n", DEFAULT_CMD_TIMO);
	fprintf(stderr, "         -D <delim>  Specify delim char between commands (default is comma)\n");
	fprintf(stderr, "         -t <port>   Specify server port\n");
	fprintf(stderr, "         -u <port>   Specify server name\n");
	fprintf(stderr, "         -p <prompt> Specify server prompt\n\n");
	fprintf(stderr, "note: you must specify a server name or a port number and the tcli prompt string\n\n");
	exit(1);
}


/*
 *  sig_handler():  catch a kill signal.  set bail_out flag to we exit from poll loop
 */
void sig_handler(int sigtype)
{
	if ( tcli.bailout == TRUE )
	{
//		af_fatal( "Second signal %d caught. Die....", sigtype );
		abort();
	}

//	af_log_print(LOG_WARN, "caught kill signal %d.. ", sigtype );
	tcli.bailout = TRUE;
}


/* append one string to another without exceeding specifid max,
 * ret -1 if append cannot be done without exceeding max length. */
int addstr(char *dest, const char *add, const char *delim, int max_len)
{
	if ( (strlen(dest) + strlen(add) + strlen(delim)) < max_len-1)
	{
		if ( strlen(dest) )
		{
			strcat(dest,delim);
		}

		strcat(dest,add);
	}
	else
	{
		return(-1);
	}

	return(0);
}


/* split a string on some delim, each word points to a part,
 * return number of parts.  Original string is modified. */
int tokenize(char *line, const char *delim, char **word, int max_parts)
{
	char *location;
	int partcnt, i;
	location = line;
	partcnt = 1;

	/* Count the number of parts */
	while( (location = strstr(location,delim)))
	{
		location += strlen(delim);
		partcnt++;
		if (partcnt == max_parts+1)
			break;
	}
	location = line;
	word[partcnt] = (char *)0;
	i = 0;
	while(1)
	{
		word[i] = location;
		location = strstr(location,delim);
		if(!location)
			break;
		*location = 0;
		if(strlen(delim) > 1)
		{
			memmove(location + 1, location + strlen(delim),\
				strlen(location + strlen(delim)) + 1);
		}
		location++;
		i++;
	}
	return(partcnt);
}


/* print a line of data with hex dump */
void dump_hex_line( char *buf, int len )
{
	int j;

	// write line as ascii
	for (j=0; j<len; j++)
	{
		if ( (buf[j] < 32) || (buf[j] > 126) )
			printf(".");
		else
			fputc( buf[j], stdout );
	}

	// spacer
	for (j=0; j<(HEX_PRINT_WIDTH+4-len); j++)
	{
		printf(" ");
	}

	// hex
	for (j=0; j<len; j++)
	{
		printf("%02X ",buf[j] );
	}
	printf("\n");
}


/* dump socket data as ascii and hex */
void dump_as_hex(char *buf, int len)
{
	int done = FALSE;
	int i = 0;

	// format the data for hex code printing
	while (! done)
	{
		if (len > i+HEX_PRINT_WIDTH)
		{
			dump_hex_line(buf+i, HEX_PRINT_WIDTH);
			i += HEX_PRINT_WIDTH;
		}
		else
		{
			dump_hex_line(buf+i, len-i);
			done = TRUE;
		}
	}
}

void myexit(int status)
{
	if (tcli.conn.client)
	{
		// close connection to server
		af_client_delete( tcli.conn.client );
	}

	af_log_print(LOG_DEBUG, "exiting status %d", status);
	exit(status);
}

void handle_cmd_timeout(void *ignore)
{
	af_log_print(LOG_ERR, "command timeout (%d secs) expired", tcli.conn.cmd_timo);
	myexit(1);
}

#define MAX_SOCK_READ_BUF 4096

void handle_server_socket_event( af_poll_t *af )
{
	int status;
	char buf[MAX_SOCK_READ_BUF];
	int len = MAX_SOCK_READ_BUF;

	//af_log_print(LOG_DEBUG, "%s: revents %d", __func__, revents);

	// Read with 1 ms timeout since we are using the poll loop to get here.
	status = af_client_read_timeout( tcli.conn.client, buf, &len, 1 );

	//af_log_print( LOG_DEBUG, "%s: client_read() returned %d, len=%d", __func__, status, len );

	// Handle status
	switch ( status )
	{
	case AF_TIMEOUT:
		// Still going.. no PROMPT yet.
		break;
	case AF_BUFFER:
			// read buffer full, poll will fire us again immediately to read the rest
		break;
	case AF_SOCKET:
		//  DCLI server probably died...
		tcli.bailout = TRUE;
		break;
	case AF_OK:
		// tcli prompt was detected
		tcli.conn.busy = FALSE;
		tcli.cmd.current++;
		break;
	default:
		af_log_print(LOG_ERR, "%s: oops, unexpected status %d from client_read", __func__, status);
		break;
	}

	// Handle any data we read from the sock
	if ( len > 0 )
	{
		buf[len] = 0;
		af_log_print(LOG_DEBUG, "%s: server rx %d bytes", __func__, len);

		if (tcli.opt.dump_hex == TRUE)
		{
			dump_as_hex( buf, len );
			fflush(stdout);
		}
		else
		{
			printf("%s",buf);
			fflush(stdout);
		}
	}
}


int send_tcli_command(void)
{
	int status;
	int exitval;

	af_log_print(LOG_DEBUG, "%s: sending tcli command \"%s\"", __func__, tcli.cmd.part[tcli.cmd.current] );

	// show user the command we're sending unless suppressed
	if (tcli.opt.hide_prompt == FALSE)
	{
		printf("%s%s\n", tcli.conn.client->prompt, tcli.cmd.part[tcli.cmd.current] );
		fflush(stdout);
	}

	status = af_client_send( tcli.conn.client, tcli.cmd.part[tcli.cmd.current] );
	/*
	client_send can return:
		AF_TIMEOUT
		AF_ERRNO
		AF_SOCKET
		AF_OK
	*/

	switch ( status )
	{
	case AF_OK:
		// command was sent successfully
		tcli.conn.busy = TRUE;
		exitval = 0;
		break;
	default:
		//  send failed
		af_log_print(LOG_ERR, "%s: client_send returned %d (cmd=%s)", __func__, status, tcli.cmd.part[tcli.cmd.current] );
		exitval = 1;
		break;
	}

	return exitval;
}

int main_loop(void)
{
	while( 1 )
	{
		af_poll_run( 100 );

		if ( tcli.conn.busy == FALSE )
		{
			// Done with the previous command send the next
			if ( tcli.cmd.current == tcli.cmd.num )
			{
				break;
			}
			if ( send_tcli_command() )
				myexit(1);
		}

		// check bailout flag
		if ( tcli.bailout )
			myexit(1);
	}

	return 0;
}


/*
 *  main():  read cmd line, connect to server, drop into command loop
 */

af_daemon_t mydaemon;

int main(int argc, char *argv[])
{
	int   ch;
	int   port = 0;
	char *service = NULL;
	char *prompt = NULL;
	unsigned int ip = 0;
	char *default_server_name = (char*)"localhost";
	char *lpServerName = NULL;
	char *servername = NULL;
	unsigned int addr;
	struct hostent *hp;
	struct sockaddr_in server;
	unsigned short usport = DEFAULT_PORT;
	//int socket_type = DEFAULT_PROTO;
	//int socket_type = SOCK_DGRAM;
	//int socket_type = SOCK_STREAM;


	mydaemon.appname = argv[0];
	mydaemon.daemonize = 0;
	mydaemon.log_level = LOG_WARNING;
	mydaemon.sig_handler = sig_handler;
	mydaemon.log_name = argv[0];
	mydaemon.use_syslog = 0;

	/*  read command line args  */
	while ( (ch = getopt(argc, argv, "o:t:p:u:T:D:c:i:l:m:svxhdn")) != -1 )
		switch ( ch )
		{
		case 'T':
			tcli.conn.connect_timo = atoi(optarg);
			break;
		case 'D':
			strncpy(tcli.opt.delim_char, optarg, 1);
			break;
		case 'c':
			tcli.conn.cmd_timo = atoi(optarg);
			break;
		case 'l':
			mydaemon.log_level = atoi(optarg);
			break;
		case 'm':
			if ( (strlen(optarg) > 2) && (optarg[1] == 'x') )
				sscanf(optarg,"%x",&mydaemon.log_mask);
			else    
				mydaemon.log_mask = atoi(optarg);
			break;
		case 'i':
			strncpy( tcli.opt.filename, optarg, PATH_MAX-1);
			tcli.opt.filename[PATH_MAX-1] = 0;
			break;
		case 's':
			mydaemon.use_syslog = TRUE;
			break;
		case 'o':
			mydaemon.log_filename = optarg;
			break;
		case 'd':
			tcli.opt.debug_mode = 1;
			break;
		case 't':
			port = atoi(optarg);
			usport = port;
			break;
		case 'u':
			lpServerName = optarg;
			break;
		case 'p':
			prompt = optarg;
			break;
		case 'x':
			tcli.opt.dump_hex = TRUE;
			break;
		case 'n':
			tcli.opt.hide_prompt = TRUE;
			break;
		case 'v':
			printf("tcli v%s\n", version );
			exit(0);
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}

	if ( optind == argc )
	{
		usage();
	}

	af_daemon_set( &mydaemon );
	af_daemon_start();

	service = strdup( argv[optind++] );

	if ( ! (ip) ) {
		if ( lpServerName == NULL ) {
			servername = default_server_name;
		} else {
			servername = lpServerName;
		}
		//
		// Attempt to detect if we should call gethostbyname() or
		// gethostbyaddr()
		if (isalpha(servername[0])) {   /* server address is a name */
			hp = gethostbyname(servername);
		}
		else  { /* Convert nnn.nnn address to a usable one */
			addr = inet_addr(servername);
			hp = gethostbyaddr((char *)&addr,4,AF_INET);
		}
		memset(&server,0,sizeof(server));
		if (hp == NULL ) {
			int errsv = errno;
			int herrsv = h_errno;
			af_log_print(LOG_INFO,"Client: Cannot resolve address [%s]: Error %d, h_err = %i\n",
				servername,errsv,herrsv);
			af_log_print(LOG_INFO,"%s\n", hstrerror(h_errno));
			if (h_errno == HOST_NOT_FOUND) af_log_print(LOG_INFO,"Note: on linux, ip addresses must have valid RDNS entries\n");
			ip = server.sin_addr.s_addr = inet_addr(servername);
			server.sin_family = AF_INET;
		} else {
			memcpy(&(server.sin_addr),hp->h_addr,hp->h_length);
			server.sin_family = hp->h_addrtype;
		}
		server.sin_port = htons(usport);
	}

	tcli.conn.client = af_client_new( service, ip, port, prompt );

	af_log_print(LOG_INFO, "tcli server: %s, port %d, prompt %s", service, port, prompt );

	while ( optind < argc )
	{
		af_log_print(LOG_INFO, "ARG[%d]: \"%s\"", optind, argv[optind] );

		addstr(tcli.cmd.buf, argv[optind], " ", MAX_CMD_BUF  );
		optind++;
	}

	// send the help string if we didn't get a command
	if ( ! strlen(tcli.cmd.buf) && ! strlen(tcli.opt.filename) )
	{
		strcpy(tcli.cmd.buf, "?");
	}

	if ( strlen(tcli.cmd.buf) )
	{
	 	// split the command buffer on delim char
		tcli.cmd.num = tokenize(tcli.cmd.buf, tcli.opt.delim_char, tcli.cmd.part, MAX_CMDS);
	}
	else
	{
		tcli.cmd.num = 0;
	}

	for ( ch=0;ch<tcli.cmd.num;ch++)
	{
		af_log_print(LOG_DEBUG, "tcli cmd[%d]: %s", ch, tcli.cmd.part[ch] );
	}

	// connect to tcli server
	if ( af_client_connect( tcli.conn.client ) )
	{
		af_log_print(LOG_ERR, "failed to connect to server (port=%d, prompt=\"%s\")", tcli.conn.client->port, tcli.conn.client->prompt );
		myexit(1);
	}
	else
	{
		af_log_print(LOG_DEBUG, "connected to server (port=%d)", tcli.conn.client->port );
	}

	// detect the initial prompt
	if ( af_client_get_prompt( tcli.conn.client, tcli.conn.connect_timo*1000 ) )
	{
		af_log_print(LOG_ERR, "failed to connect to detect prompt (\"%s\") within timeout (%d secs)", tcli.conn.client->prompt, tcli.conn.connect_timo );
		myexit(1);
	}
	
	// Queue up the commands

	if ( strlen(tcli.opt.filename) )
	{
		FILE *fh;
		char buf[2048];
		char *ptr;

		if ( (fh=fopen( tcli.opt.filename, "r" )) == NULL )
		{
			af_log_print(LOG_ERR, "failed to open command file %s", tcli.opt.filename );
			myexit(1);
		}

		while ( !feof(fh) )
		{
			if ( fgets( buf, 2048, fh ) == NULL )
				break;

			// Remove new line.
			ptr = strchr( buf, '\n' );
			if ( !ptr )
				continue;
			*ptr = 0;

			// clear white space at the begining of any command
			ptr = buf;
			while ( isspace(*ptr) ) ptr++;

			// check for valid line.
			if ( strlen(ptr) < 1 )
				continue;
			if ( buf[0] == '#' )
				continue;
			if ( buf[0] == ';' )
				continue;

			tcli.cmd.part[tcli.cmd.num++] = strdup( buf );
		}
	}

	// Send the first command
	if ( send_tcli_command() )
		myexit(1);

	af_poll_add( tcli.conn.client->sock, POLLIN, handle_server_socket_event, &tcli );

	// Handle socket.
	main_loop();

	myexit(0);

	return 0;
}

