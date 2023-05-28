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
#define MAXDECODE	250
#define MAXCOMS 128
#define YESNO(x) ( (x) ? ("Yes") : ("No") )
#define ONOFF(x) ( (x) ? ("On") : ("Off") )

#define CLOGIN		2
#define SSET		1


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

comport coms[2];

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
// the NetLink call back function vector:
typedef void (*CommandCallBack_t)(int destination, int subcommand, unsigned char * data, int datasize );
CommandCallBack_t CommandCallBack[256] = { NULL };
void Register_CommandCallBack(unsigned char command, CommandCallBack_t ptr);
#define REGISTER_CALLBACK(command,function) Register_CommandCallBack(command, (CommandCallBack_t)function)
#define GET_CALLBACK(command) (CommandCallBack[command])
#define PING_CMD				0x01
#define READPOWEROUTLET_CMD		0x20

int send_NetLink_command(int destination,int command,int subcommand,unsigned char * data, unsigned int datasize);
int send_NetLink_login(char * password);
int process_NetLink_message(af_client_t *cl, char *buf, int *len);
int decode_NetLink_command(int *destination,int *command,int *subcommand,unsigned char * raw, unsigned int len, unsigned char ** data, int * datasize, unsigned char ** nextpacket);
unsigned char * c2p ( const int i );

unsigned char ctempjk;
int retjk;

void Register_CommandCallBack(unsigned char command, CommandCallBack_t ptr) {
	CommandCallBack[command] = ptr;
	printf("Callback routine registered for NetLink command number 0x%02x\n", command);
}

CommandCallBack_t ProcessPing (int destination, int subcommand, unsigned char * envelope, int datasize ) {
	// let's interogate switch settings on each ping...
	if ( subcommand == 1 ) {
		printf("Interrogating Power Outlet status...\n");
		send_NetLink_command(0,0x20,0x02,c2p(1), 1);
		send_NetLink_command(0,0x20,0x02,c2p(2), 1);
		send_NetLink_command(0,0x20,0x02,c2p(3), 1);
		send_NetLink_command(0,0x20,0x02,c2p(4), 1);
		send_NetLink_command(0,0x20,0x02,c2p(5), 1);
		send_NetLink_command(0,0x20,0x02,c2p(6), 1);
		send_NetLink_command(0,0x20,0x02,c2p(7), 1);
		send_NetLink_command(0,0x20,0x02,c2p(8), 1);
	}
	return(0);
}

CommandCallBack_t ProcessPowerOutletStatus (int destination, int subcommand, unsigned char * envelope, int datasize ) {
	char ctemp[260];
	// print the switch setting
	if ( datasize > 255 ) goto ret1;
	if ( subcommand == 0x10 ) {			// Response to a command
		if ( datasize < 9 ) goto ret1;
		strncpy(ctemp,(const char *)(envelope+2),4);
		ctemp[4] = '\000';
		printf ("Power outlet %u is %s, cycle time is %s seconds\n",*envelope,ONOFF(*(envelope+1)),ctemp );
	}
ret1:
	return(0);
}


unsigned char * c2p ( const int i ) { ctempjk = (unsigned char)i; return(&ctempjk);}

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

void handle_cmd_timeout(void *ignore)	//	where is this used??
{
	af_log_print(LOG_ERR, "command timeout (%d secs) expired", tcli.conn.cmd_timo);
	myexit(1);
}

#define MAX_SOCK_READ_BUF 4096

void handle_server_socket_event( af_poll_t *af )
{
	int status;
	int i;
	char buf[MAX_SOCK_READ_BUF];
	int len = MAX_SOCK_READ_BUF;

	//af_log_print(LOG_DEBUG, "%s: revents %d", __func__, revents);

	// Read with 1 ms timeout since we are using the poll loop to get here.
	if ( strcmp(tcli.conn.client->prompt,"") ) {
		status = af_client_read_timeout( tcli.conn.client, buf, &len, 1 );
	} else {
		status = af_client_read_raw_timeout( tcli.conn.client, buf, &len, 1 );
	}

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
		len = 0;
		break;
	case AF_OK:
		// tcli prompt was detected
		tcli.conn.busy = FALSE;
		tcli.cmd.current++;
		break;
	default:
		af_log_print(LOG_ERR, "%s: oops, unexpected status %d from client_read", __func__, status);
		tcli.bailout = TRUE;
		len = 0;
		break;
	}

	// Handle any data we read from the sock
	if ( len > 0 ) {
		if ( strcmp(tcli.conn.client->prompt,"") )
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
		} else {										// NetLink clients come here
			printf("received %i bytes : (0x)",len);
			for ( i = 0; i < len; i++) {
				printf(" %02x", (unsigned char)(*(buf+i)));
			}
			printf("\n");
			fflush(stdout);
			process_NetLink_message(tcli.conn.client, buf, &len);
		}
	}	// len > 0

	return;
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
			ip = ntohl(server.sin_addr.s_addr);
		}
		server.sin_port = htons(usport);
	}

	af_log_print(LOG_INFO, "tcli server: %s, ip = %u", service, ip);
//	ip = INADDR_LOOPBACK;	// for debug:	test known ip address

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

	if ( strcmp(tcli.conn.client->prompt,"") ) {
		// detect the initial prompt
		if ( af_client_get_prompt( tcli.conn.client, tcli.conn.connect_timo*1000 ) )	// Note: this is a macro for af_client_read_timeout (see appf.h)
		{
			af_log_print(LOG_ERR, "failed to detect prompt (\"%s\") within timeout (%d secs)", tcli.conn.client->prompt, tcli.conn.connect_timo );
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
		if ( send_tcli_command() ) myexit(1);
	} else {
		// This is a NetLink run
		// fix up client data for NetLink use
		coms->remote = lpServerName;
		tcli.conn.client->extra_data = (void*) coms;
		// set any required NetLink callback functions
		REGISTER_CALLBACK(PING_CMD, ProcessPing);
		REGISTER_CALLBACK(READPOWEROUTLET_CMD, ProcessPowerOutletStatus);
		// send the password
		if ( send_NetLink_login(tcli.cmd.part[tcli.cmd.current]) ) myexit(1);
	}

	af_poll_add( tcli.conn.client->sock, POLLIN, handle_server_socket_event, &tcli );

	// Handle socket.
	main_loop();

	myexit(0);

	return 0;
}

int send_NetLink_command(int destination,int command,int subcommand,unsigned char * data, unsigned int datasize) {
	unsigned int sum = 0;
	unsigned char * datapacket;
	unsigned char * ptr;
	int i;
	int status;
	int exitval;
	int packetsize;

	packetsize = datasize + 5;
	ptr = datapacket = (unsigned char *) calloc(packetsize + 3 , 1);
	*ptr = 0xfe;							// header
	ptr++;
	*ptr = (unsigned char)(datasize +3);	// length
	ptr++;
	*ptr = (unsigned char)(destination);	// Destination
	ptr++;
	*ptr = (unsigned char)(command);		// Command
	ptr++;
	*ptr = (unsigned char)(subcommand);		// Sub Command
	ptr++;
	if (datasize) {
		for (i = 0; i<datasize; i++) {
			*ptr = *(data+i);				// next byte of data
			ptr++;
		}
	}
	for ( i=0; i<packetsize; i++ ) {
		sum+=*(datapacket+i);
	}
	*ptr = sum & 0x7f;
	ptr++;
	*ptr = 0xff;




		af_log_print(LOG_DEBUG, "%s: sending NetLink command \"%i\", sub command \"%i\" to %i with %i data bytes", __func__, command, subcommand, destination, datasize );

		// show user the command we're sending unless suppressed
		if (tcli.opt.hide_prompt == FALSE)
		{

			printf("sending : (0x)");
			for (i=0; i< (packetsize+2); i++) {
				printf(" %02x", *(datapacket+i));
			}
			printf("\n");
			fflush(stdout);
		}

		status = af_client_send_raw( tcli.conn.client, datapacket, (size_t)(packetsize+2) );
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



	free(datapacket);
	return(exitval);
}
int send_NetLink_login(char * password) {
	char * userpassword = NULL;
	userpassword = (char*)malloc(strlen(password)+7);
	strcpy(userpassword,"user|");
	strcat(userpassword,password);
	if ( send_NetLink_command(0,CLOGIN,SSET,(unsigned char *)userpassword, strlen(userpassword)) ) {
		free(userpassword);
		return(1);
	}
	free(userpassword);
	return(0);
}

int process_NetLink_message(af_client_t *cl, char *buf, int *len) {
	int destination,command,subcommand,datasize;
	unsigned char *envelope;
	unsigned char *nextpacket;
	char *tempbuf;
	int templen;
	int iret = -1;
	tempbuf = buf;
	templen = *len;

	while ( iret < 0 ) {
		iret = decode_NetLink_command( &destination, &command, &subcommand, (unsigned char *)tempbuf, (unsigned int)(templen), &envelope, &datasize, &nextpacket);
		if ( iret > 0 ) return(iret);
		printf( " destination, command, subcommand - %i, %i, %i\n",destination, command, subcommand);
		switch ( (unsigned char)command )
			{
				case 0x10 :	// NACK
					printf("NACK received - error detected at device\n");
					switch ( *envelope )
					{
					case 0x01 :	//	 Bad CRC on previous command
						printf("Bad CRC on previous command\n");
						break;
					case 0x02 :	//	 Bad Length on previous command
						printf("Bad Length on previous command\n");
						break;
					case 0x03 :	//   Bad Escape sequence on previous command
						printf("Bad Escape sequence on previous command\n");
						break;
					case 0x04 :	//   Previous command invalid
						printf("Previous command invalid\n");
						break;
					case 0x05 :	//   Previous sub-command invalid
						printf("Previous sub-command invalid\n");
						break;
					case 0x06 :	//   Previous command incorrect byte count
						printf("Previous command incorrect byte count\n");
						break;
					case 0x07 :	//   Invalid data bytes in previous command
						printf("Invalid data bytes in previous command\n");
						break;
					case 0x08 :	//   Invalid Credentials (note: need to login again)
						printf("Invalid Credentials (note: need to login again)\n");
						break;
					case 0x10 :	//   Unknown Error
						printf("Unknown Error\n");
						break;
					case 0x11 :	//   Access Denied (EPO)
						printf("Access Denied (EPO)\n");
						break;
					default:
						printf("un-recognized error number (\'0x%02x\')\n",*envelope);
						return(-99);
					}
					break;
				case 0x01 :	// Ping/Pong
					if ( subcommand == 1 ) {
						printf("Ping...(Sending Pong)\n");
						// send Pong
						send_NetLink_command(0,0x01,0x10,(unsigned char *)"", 0);
					}
					break;
				case 0x02 :	// login/response
					if ( subcommand == 0x10 && datasize == 4) {
						if ( *envelope == 0 ) {
							printf("login rejected...\n");
						} else if ( *envelope == 1 ) {
							printf("login successful...\n");
						} else {
							printf("unrecognized login response...\n");
						}
					}
					break;
				case 0x20 :	// Read/write Power Outlet
					break;
				case 0x30 :	// Read/write Dry Contact
					break;
				case 0x21 :	// Read/write Outlet name
					break;
				case 0x31 :	// Read/write Contact name
					break;
				case 0x22 :	// Read Outlet count
					break;
				case 0x32 :	// Read Contact count
					break;
				case 0x36 :	// Sequencing command
					break;
				case 0x23 :	// Energy Management Command
					break;
				case 0x37 :	// Emergency Power Off Command
					break;
				case 0x40 :	// Log Alerts Commands
					break;
				case 0x41 :	// Log Status Commands
					break;
					// Sensor Value Commands:
				case 0x50 :	// Kilowatt Hours
					break;
				case 0x51 :	// Peak Voltage
					break;
				case 0x52 :	// RMS Voltage Changes
					break;
				case 0x53 :	// Peak Load
					break;
				case 0x54 :	// RMS Load
					break;
				case 0x55 :	// Temperature
					break;
				case 0x56 :	// Wattage
					break;
				case 0x57 :	// Power Factor
					break;
				case 0x58 :	// Thermal Load
					break;
				case 0x59 :	// Surge Protection State
					break;
				case 0x60 :	// Energy Management State
					break;
				case 0x61 :	// Occupancy State
					break;
					// Threshold Commands:
				case 0x70 :	// Low Voltage Threshold
					break;
				case 0x71 :	// High Voltage Threshold
					break;
				case 0x73 :	// Max Load Current
					break;
				case 0x74 :	// Min Load Current
					break;
				case 0x76 :	// Max Temperature
					break;
				case 0x77 :	// Min Temperature
					break;
					// Log stuff:
				case 0x80 :	// Log Entry Read
					break;
				case 0x81 :	// Get Log Count
					break;
				case 0x82 :	// Clear Log
					break;
					// Product Rating and Information:
				case 0x90 :	// Part Number
					break;
				case 0x91 :	// Product Amp Hour Rating
					break;
				case 0x93 :	// Product Surge Existence
					break;
				case 0x94 :	// Current IP Address
					break;
				case 0x95 :	// MAC Address
					break;

				default:
					return(-99);
					break;
			}
// process command callbacks
		if ( CommandCallBack[command] ) GET_CALLBACK(command) ( destination, subcommand, envelope, datasize );
// are there more commands to process in this record?
		if ( iret < 0 ) {
			templen -= ( nextpacket - (unsigned char *)tempbuf );
			tempbuf = (char *)nextpacket;
		}
	}

	return(1);
}

int decode_NetLink_command(int *destination,int *command,int *subcommand,unsigned char * raw, unsigned int len, unsigned char ** data, int * datasize, unsigned char ** nextpacket) {
	unsigned int sum = 0;
	unsigned char * datapacket;
	unsigned char chksum;
	int i;
	int packetsize;
	*nextpacket = NULL;

	if ( len < 7 ) {					// minimum length
		printf ("Not a NetLink message\n");
		return(1);
	}
	*datasize = (int)(*(raw+1));			// datasize

	packetsize = *datasize + 2;
	// check checksum
	if (packetsize > 0) {
		for ( i=0; i<packetsize; i++ ) {
			sum+=*(raw+i);
		}
		chksum = sum & 0x7f;
	}
	if ( chksum != *(raw+packetsize) ) {	// checksum
		printf ("Bad message checksum, calculated = 0x%02x, read = 0x%02x\n",chksum , *(raw+packetsize) );
		return(2);
	}
	if ( *raw != 0xfe ) {					// header
		printf ("Bad NetLink message header\n");
		return(3);
	}
	if ( *(raw+packetsize+1) != 0xff ) {	// tail
		printf ("Bad NetLink message tail character\n");
		return(4);
	}
	if ( *(raw+1) != (unsigned char)( len - 4 ) ) {	// length
		*nextpacket = raw + packetsize + 2;
	}


	datapacket = raw + 2;
	*destination = (int)(*datapacket);		// destination
	datapacket++;
	*command = (int)(*datapacket);			// command
	datapacket++;
	*subcommand = (int)(*datapacket);		// sub command
	datapacket++;
	*data = datapacket;						// data

	if ( *nextpacket ) return(-1);
	return(0);
}
