/* fos - Clustering Failover Services
**
** Red Hat Clustering Tools 
** Copyright 1999 Red Hat, Inc.
**
** Authors: 
**          Keith Barrett <kbarrett@redhat.com>
**          Erik Troan <ewt@redhat.com> 
**
**
** This software may be freely redistributed under the terms of the GNU
** public license.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**
** MODIFICATION HISTORY
**
** 3/1/1999     1.0 -   Keith Barrett <kbarrett@redhat.com>
**              1.1     Initial creation from lvs.c code
**
** 3/3/2000     1.2     Keith Barrett <kbarrett@redhat.com>
**                      Use real ip for pinging, not vip
**
** 3/4/2000     1.3 -   Keith Barrett <kbarrett@redhat.com>
**              1.6     Changed logic so fos handles stopping daemons
**                      instead of nanny. Proclaimed stable!
**                      Start/stop now allows a number of parameters.
**
** 3/6/2000     1.7     Keith Barrett <kbarrett@redhat.com>
**                      Don't interfere with signal handlers of child services
**                      Expanded error message text for -f -F
**
** 7/20/2000    1.8 -   Keith Barrett <kbarrett@redhat.com>
**              1.9     Pass udp switch to nanny. Added --version
**                      Added Bryce's CFGFILE logic.
**
** 5/29/2001    1.10    Keith Barrett <kbarrett@redhat.com>
**                      Pass --nodaemon to nanny
**
*/

#define _PROGRAM_VERSION "fos 1.10"


#include <errno.h>
#include <net/if.h>                         /* for getInterfaceAddress() */
#include <sys/ioctl.h>                      /* for getInterfaceAddress() */
#include <ctype.h>                          /* for getInterfaceAddress() */
#include <fcntl.h>
#include <netinet/in.h>
#include <popt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lvsconfig.h"
#include "util.h"

#define _(a) (a)
#define N_(a) (a)

#define FS_MODE_MONITOR    (1 << 2)
#define FS_MODE_SERVICES   (1 << 3)

/* Globals icky :-( */
static int termSignal = 0;
static int currentFSmode = 0;
static int debug_mode = 0;

struct clientInfo {
    struct in_addr address;
    int port;
    pid_t clientPID;        /* -1 if died */
};




static void handleChildDeath(int signal) {
    /* We don't need to do anything here as we always check for dead
       children when we get a signal. */
}




static void handleTermSignal(int signal) {
    termSignal = signal;
}





int runCommand( int flags, char ** argv, struct clientInfo * clients) {

    pid_t child = 0;
    int status;

    logArgv(flags, argv);

    if (!(child = fork())) {
        if (flags & LVS_FLAG_TESTSTARTS)
            exit(0);
        execv(argv[0], argv);
        exit(1);
    }

    waitpid(child, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status))
        return 1;

    if (clients)
        clients->clientPID = child;

    return 0;
}



void parseCommandStr(char * cmd_str, char ** argp) {

    char * marker  = NULL;
    char * cmd_ptr;


    cmd_ptr = cmd_str;
    marker  = strchr(cmd_ptr, ' ');

    if (marker) {
        while (marker) {
            *marker   = 0;
            *argp++  = cmd_ptr;
            cmd_ptr   = ++marker;
            marker    = strchr(cmd_ptr, ' ');
        }
            *argp++   = cmd_ptr;
    } else
        *argp++ = cmd_ptr;

    *argp = NULL;
}





char *strip_quotes(char *in_string) {
    char *tmp_ptr;
    char *out_string = NULL;

    if (in_string) {
        out_string = in_string;

        if (*in_string == '\"')
            ++out_string;

        if (strlen(out_string)) {
            tmp_ptr = strlen(out_string) + out_string - 1;
            if (*tmp_ptr == '\"')
                *tmp_ptr = 0;
        }
    }

    return out_string;
}






int shutdownIPservices(     struct lvsConfig * config, 
                            struct lvsVirtualServer * FOserver,
                            int flags,
                            struct clientInfo * clients,
			    int ignore) {

    char * argv[40];
    char ** arg = argv;
    char virtAddress[50];
    char port[10];
    int rc;
    char *dup_string;
    char *cmd_string;


    if (!FOserver->isActive)
        return 0;
    
    dup_string = strdup(FOserver->stop_cmd);
    cmd_string = strip_quotes(dup_string);
    parseCommandStr(cmd_string, arg);
    /* turn command line into argv[] pieces */

    sprintf(virtAddress, "%s", inet_ntoa(FOserver->virtualAddress));
    sprintf(port, "%d", FOserver->port);

    piranha_log(flags, (char *) "Shutting down local service %s:%s ",
        virtAddress, port);

    rc = runCommand(flags, argv, 0);

    if (rc)
        piranha_log(flags,
	    (char *)
            "Warning; shutdown of local service %s:%s returned error %d",
            virtAddress, port, rc);

    free(dup_string);

    if (ignore)
      return 0;
    else
      return rc;
}




int shutdownClientMonitor(  struct lvsConfig * config, 
                            struct lvsVirtualServer * FOserver,
                            struct clientInfo * clients,
                            int flags) {

    char * virtAddress;
    char portNum[10];
    pid_t pid;

    if (!FOserver->isActive)
        return 0;
    
    virtAddress = strdup(inet_ntoa(FOserver->virtualAddress));
    sprintf(portNum, "%d", FOserver->port);

    pid = clients->clientPID;

    if ((pid != -1) && (pid != 0)) {
        piranha_log(flags,
            (char *) "Shutting down monitor for %s %s:%s running as pid %d",
            FOserver->name, virtAddress, portNum, pid);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        clients->clientPID = 0;
    }

    free(virtAddress);
    return 0;
}




int startClientMonitor( struct lvsConfig * config, 
                        struct lvsVirtualServer * FOserver,
                        struct in_addr partnerAddress,
                        struct clientInfo * clients,
                        int flags) {

    char * virtAddress;
    char * realAddress;
    char * argv[42];
    char ** arg = argv;
    char portNum[10];
    char timeoutNum[20];
    pid_t child;
    char *start_cmd  = NULL;
    char *stop_cmd   = NULL;
    char *send_str   = NULL;
    char *expect_str = NULL;

    child=getpid();	/* don't leave this initialized bad things happen */
    
    if (!FOserver->isActive)
        return 0;
    
    virtAddress = strdup(inet_ntoa(FOserver->virtualAddress));
    realAddress = strdup(inet_ntoa(partnerAddress));
    sprintf(portNum,    "%d", FOserver->port);
    sprintf(timeoutNum, "%d", FOserver->timeout);

    *arg++ = FOserver->clientMonitor;
    *arg++ = (char *) "-c";

    *arg++ = (char *) "-h";
    *arg++ = realAddress;

    *arg++ = (char *) "-V";
    *arg++ = virtAddress;

    *arg++ = (char *) "-p";
    *arg++ = portNum;

    if (debug_mode)
        *arg++ = (char *) "-v";                 /* Pass along debug mode */

    if (FOserver->protocol == IPPROTO_UDP)
        *arg++ = (char *) "-u";
    /* UDP */

    if (FOserver->send_str) {
        send_str = strdup(FOserver->send_str);
        *arg++ = (char *) "-s";
        *arg++ = strip_quotes(send_str); 
    }

    if (FOserver->expect_str) {
        expect_str = strdup(FOserver->expect_str);
        *arg++ = (char *) "-x";
	*arg++ = strip_quotes(expect_str);
    }

    if (FOserver->start_cmd) {
        start_cmd = strdup(FOserver->start_cmd);
        *arg++ = (char *) "-R";
        *arg++ = strip_quotes(start_cmd);
    }

    if (FOserver->stop_cmd) {
        stop_cmd = strdup(FOserver->stop_cmd);
        *arg++ = (char *) "-D";
        *arg++ = strip_quotes(stop_cmd);
    }

    *arg++ = (char *) "-t";
    *arg++ = timeoutNum;

    if (!(flags & LVS_FLAG_ASDAEMON)) {
        *arg++ = (char *) "--nodaemon";         /* Pass along --nodaemon */
    }

    *arg++ = NULL;

    logArgv(flags, argv);

    if (!(flags & LVS_FLAG_TESTSTARTS)) {
        piranha_log(flags,
            (char *) "Starting monitor for %s:%s running as pid %d",
            virtAddress, portNum, child);

        child = fork();

        if (!child) {
            execv(argv[0], argv);
            exit(1);
        }
    } else {
        child = -1;
    }

    free(virtAddress);
    free(realAddress);

    if (start_cmd)  free(start_cmd);
    if (stop_cmd)   free(stop_cmd);
    if (send_str)   free(send_str);
    if (expect_str) free(expect_str);

    clients->clientPID = child;
    return 0;
}



int startIPservice(     struct lvsConfig * config, 
                        struct lvsVirtualServer * FOserver,
                        int flags,
                        struct clientInfo * clients) {
    char * argv[40];
    char ** arg = argv;
    char virtAddress[50];
    char port[10];
    int rc;
    char *dup_string;
    char *cmd_string;


    if (!FOserver->isActive)
        return 0;

    dup_string = strdup(FOserver->start_cmd);
    cmd_string = strip_quotes(dup_string);
    parseCommandStr(cmd_string, arg);
    /* turn command line into argv[] pieces */

    sprintf(virtAddress, "%s", inet_ntoa(FOserver->virtualAddress));
    sprintf(port, "%d", FOserver->port);

    piranha_log(flags, (char *) "Starting local service %s:%s ...",
        virtAddress, port);

    rc = runCommand(flags, argv, clients);

    free(dup_string);

    if (rc)
        piranha_log(flags, (char *) "Failed to start service %s:%s!",
            virtAddress, port);

    return rc;
}





/*
**  getInterfaceAddress() -- Get address of a network device
*/

static int getInterfaceAddress( int s,
                                char * device,
                                struct in_addr * addr, 
                                int flags) {
    struct ifreq req;
    struct sockaddr_in *addrp;
  
    strcpy(req.ifr_name, device);
  
    if (ioctl(s, SIOCGIFADDR, &req)) {
        if (strncmp("tunl", device, 4))    /* log only if not tunl dev */
            piranha_log(flags,
                (char *) "SIOCGIFADDR failed: %s", strerror(errno));
        /*
        ** Note: suppress errors about tunl because it's in /proc even
        ** if it is not being used, so it will appear here.
        */

        return 1;
    }

    addrp = (struct sockaddr_in *) &req.ifr_addr;
    memcpy(addr, &addrp->sin_addr, sizeof(*addr));
    return 0;
}    





/*
**  amMaster() -- determine if we are the master or backup node
*/

int amMaster(struct lvsConfig * config, int flags) {
  
    int fd;
    int i;
    char buf[1024];
    char * start, * end;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct in_addr addr;
  
    if ((fd = open("/proc/net/dev", O_RDONLY)) < 0) {
        piranha_log(flags, (char *) "failed to open /proc/net/dev!\n");
        return 1;
    }
  
    i = read(fd, buf, sizeof(buf));
    close(fd);
    buf[i] = '\0';
  
  
    /* skip the first two lines */
  
    start = strchr(buf, '\n');
    if (!start)
        return 0;
  
    start = strchr(start + 1, '\n');
    if (!start)
        return 0;
  
    start++;
  
    while (start && *start) {
        while (isspace(*start)) start++;
            end = strchr(start, ':');
        if (!end)
            return 0;
        *end = '\0';
    
        if (strcmp(start, "lo")) {
            if (!getInterfaceAddress(s, start, &addr, flags) &&
                    !memcmp(&addr, &config->primaryServer, sizeof(addr))) {
                close(s);
                return 1;
            }
        }

        start = strchr(end + 1, '\n');
    
        if (start)
            start++;
    }
  
    close(s);
    return 0;
}




int run(struct lvsConfig * config, int flags, char * configFile) {

    int i;
    struct clientInfo * clients = NULL;
    sigset_t sigs;
    pid_t pid;
    int status;
    int doShutdown = 0;
    int rc = 0;
    char virtAddress[50];
    char port[10];
    struct in_addr partnerAddress;


    if (flags & LVS_FLAG_ASDAEMON) {
        flags |= LVS_FLAG_SYSLOG;
        if (daemonize(flags))
            return 0;
    }

    if (amMaster(config, flags))
        partnerAddress = config->backupServer;
    else
        partnerAddress = config->primaryServer;
    /* Get address of remote system for monitor pings */

    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGCHLD);
    sigaddset(&sigs, SIGTERM);
    sigaddset(&sigs, SIGHUP);
    sigprocmask(SIG_BLOCK, &sigs, NULL);

    signal(SIGCHLD, handleChildDeath);
    signal(SIGINT,  handleTermSignal);
    signal(SIGTERM, handleTermSignal);
    signal(SIGHUP,  handleTermSignal);

    clients = realloc(clients,
        sizeof(*clients) * config->numFailoverServices);


    sigprocmask(SIG_UNBLOCK, &sigs, NULL);

    for (i = 0; i < config->numFailoverServices; ++i) {

        clients[i].address      = config->failoverServices[i].virtualAddress;
        clients[i].port         = config->failoverServices[i].port;
        clients[i].clientPID    = 0;

        if (config->failoverServices[i].isActive) {
	      piranha_log(flags,
	         (char *) "Stopping local services (if any)");
              shutdownIPservices(config, config->failoverServices + i, flags,
                  clients + i, 0);

            if (currentFSmode == FS_MODE_SERVICES) {
                rc = startIPservice(config, config->failoverServices + i,
                    flags, clients + i);
            } else {
              rc = startClientMonitor(config, config->failoverServices + i,
                    partnerAddress, clients + i, flags);
            }
        }
    }


    sigfillset(&sigs);
    sigdelset(&sigs, SIGINT);
    sigdelset(&sigs, SIGCHLD);
    sigdelset(&sigs, SIGTERM);
    sigdelset(&sigs, SIGHUP);



    while (!doShutdown) {
        sigsuspend(&sigs);

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            for (i = 0; i < config->numFailoverServices; i++) {
                if (clients[i].clientPID == pid)
                    break;
            }

            if (i < config->numFailoverServices) {
                sprintf(virtAddress, "%s",
                    inet_ntoa(config->failoverServices[i].virtualAddress));
                sprintf(port, "%d", config->failoverServices[i].port);

                if (currentFSmode == FS_MODE_MONITOR) {
                    /* We were in monitor mode, so a monitor must have died */
                    piranha_log(flags,
                        (char *)
                        "Monitor for service %s:%s exited. This is a failover condition!",
                        virtAddress, port);

                } else {
                    /* We were in service mode, so a service must have died */
                    piranha_log(flags,
                        (char *)
                        "Service %s %s:%s died. This is a restart condition!",
                        config->failoverServices[i].name,
                        virtAddress, port);

                }

                clients[i].clientPID = -1;
                /* In either case, the child we knew about is dead. */


            } else {
                /*
                **  We got a child death and it doesn't match what we know.
                **  It's likely one of the services dying, but it doesn't
                **  matter because all we can do is shutdown and trigger
                **  pulse to restart us.
                */
                piranha_log(flags, (char *)
                    "Child with pid %d died with status %d -- exiting...",
                    pid, status);
            }

            doShutdown = 1;
	    /* All children dying result in a shutdown */
        }


        if (termSignal) {
            piranha_log(flags,
                (char *) "Shutting down due to signal %d", termSignal);
            doShutdown = 1;
        }

    }



    /*
    **  If we get here, it's because doShutdown was set and it's
    **  time to quit
    */

    for (i = 0; i < config->numFailoverServices; i++) {
        if (currentFSmode == FS_MODE_MONITOR) {
            if (clients[i].clientPID)
                shutdownClientMonitor(config, config->failoverServices + i, 
                    clients + i, flags);
        } else {
            shutdownIPservices(config, config->failoverServices + i, flags,
                clients + i, 0);
        }
    }


    if (flags & LVS_FLAG_ASDAEMON)
        unlink("/var/run/fos.pid");

    piranha_log(flags,
		(char *) "will now exit to notify pulse...");

    exit (0);
}



int main(int argc, const char ** argv) {

    char * configFile = (char *) CFGFILE;   /* Supplied by Makefile */
    int testStart = 0, noDaemon = 0;
    poptContext optCon;
    int rc;
    int flags = 0;
    int fd;
    int line;
    int noFork = 0;
    struct lvsConfig config;
    int FOmode_failover = 0;
    int FOmode_monitor  = 0;
    int display_ver     = 0;
    

    struct poptOption options[] = {
        { "configfile", 'c', POPT_ARG_STRING, &configFile, 0, 
        N_("Configuration file"), N_("configfile") },

        { "nodaemon", 'n', 0, &noDaemon, 0,
        N_("Don't run as a daemon") },

        { "monitor", 'f', 0, &FOmode_monitor, 0,
        N_("Start client monitors and watch for failure") },

        { "active", 'F', 0, &FOmode_failover, 0,
        N_("Start IP services and watch for failure") },

        { "verbose", 'v', 0, &debug_mode, 0,
        N_("Log debugging information") },

        { "test-start", 't', 0, &testStart, 0, 
        N_("Display what commands would be run on startup, but "
           "don't actually run anything") },

        { "nofork", '\0', 0, &noFork, 0,
        N_("Don't fork, but do disassociate") },

        { "version", '\0', 0, &display_ver, 0,
        N_("Display program version") },

        POPT_AUTOHELP
        { 0, 0, 0, 0, 0 }
    };

    optCon = poptGetContext("fos", argc, argv, options,0);
    poptReadDefaultConfig(optCon, 1);

    if ((rc = poptGetNextOpt(optCon)) < -1) {
        fprintf(stderr, _("fos: bad argument %s: %s\n"), 
            poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
            poptStrerror(rc));
        return 1;
    }

    if (poptGetArg(optCon)) {
        fprintf(stderr, _("fos: unexpected arguments\n"));
        return 1;
    }

    poptFreeContext(optCon);
    logName((char *) "fos");
    flags = 0;

    if (display_ver) {
        printf("Program Version:\t%s\n", _PROGRAM_VERSION);
	printf("Built:\t\t\t%s\n", DATE); /* DATE pulled from Makefile */
	printf("A component of:\t\tpiranha-%s-%s\n", VERSION, RELEASE);
        return 0;
    }


    if ((FOmode_failover != 0) && (FOmode_monitor != 0)) {
        fprintf(stderr,
            "fos: Cannot specifiy both -f and -F!\n");
        return 1;

    } else if ((FOmode_failover | FOmode_monitor) == 0) {
        fprintf(stderr,
            "fos: Either --monitor (-f) or --active (-F) required!\n");
        return 1;

    } else if (FOmode_failover) {
        currentFSmode = FS_MODE_SERVICES;
    } else
        currentFSmode = FS_MODE_MONITOR;


    if (testStart)
        flags |= LVS_FLAG_TESTSTARTS;

    if (!noDaemon)
        flags |= LVS_FLAG_ASDAEMON | LVS_FLAG_SYSLOG;
    else
        flags |= LVS_FLAG_PRINTF;

    if (noFork) {
        flags |= LVS_FLAG_NOFORK;
    }

    if ((fd = open(configFile, O_RDONLY)) < 0) {
        fprintf(stderr, "fos: failed to open %s\n", configFile);
        return 1;
    }

    if ((rc = lvsParseConfig(fd, &config, &line))) {
        fprintf(stderr,
            "fos: error parsing %s on line %d.\n", configFile, line);
        return 1;
    }


    lvsRelocateFS(&config);     /* Move failover services to own list */

    if (config.lvsServiceType != LVS_SERVICE_TYPE_FOS) {
      fprintf(stderr, "fos: Service type is not \"fos\" in %s\n",
	      configFile);
      return 1;

    } else if (config.numFailoverServices) {
      return run(&config, flags, configFile);

    } else {
      fprintf(stderr,
	      "fos: No Failover Services defined in %s.\n", configFile);
      return 1;
    }
}

