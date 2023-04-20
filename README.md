# libappf
C Application Framework Library for building daemons


/*****************************************************************************/
/*                                                                           */
/* Author: Colin Whittaker                                                   */
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
 */

Example code is also included for:

 A telnet server that connects to a com port (com2net.c) (Note: I have made some significant changes to this routine, so I separated it out into its own repository. See git@github.com:dad98253/com2net.git)
 
 A telnet CLI transaction application (tcli.c) (dumps a telnet(ish) local port to stdout) (I tested this against the local DXCluster node and it works great! No audible bell for \a :)
 
 A sample program that will daemonize itself (daemonize.c)
 
 A program named cJSON.c (I have no idea why Colin included this progrtam in his repository. It does not appear to use the api. However, I have retained it in my repository just in case I ever wish to submit a pull request back to Colin... I have made no changes to this routine. -jck )

   


