/****************************************************************************/
/*                                                                          */
/* This program provides an example of a very simplistic client             */
/*                                                                          */
/*                +---------------------------+                             */
/*                |  Attach program to Bus    |                             */
/*                |  using PAMS_ATTACH_Q      |                             */
/*                +---------------------------+                             */
/*                              |                                           */
/*                +---------------------------+                             */
/*                | Initialize Variables, etc.|                             */
/*                +---------------------------+                             */
/*            +---------------->|                                           */
/*            |   +---------------------------+     +----------+            */
/*            |   |   Wait for user to enter  |     |          |            */
/*            |   |   a message.              |---->|   exit   |            */
/*            |   +---------------------------+     +----------+            */
/*            |                 |                                           */
/*            |   +---------------------------+                             */
/*            |   |   Send user message to    |                             */
/*            |   | server (ourself) using    |                             */
/*            |   |       PAMS_PUT_MSG        |                             */
/*            |   +---------------------------+                             */
/*            |                 |                                           */
/*            |   +---------------------------+                             */
/*            |   |   Wait for a reply from   |                             */
/*            |   | the server (our msg.)     |                             */
/*            |   |       PAMS_GET_MSGW       |                             */
/*            |   +---------------------------+                             */
/*            |                 |                                           */
/*            +---------<-------+                                           */
/*                                                                          */
/****************************************************************************/


        /* Include standard "C" and system definition files. */

#include <stdio.h>                        /* Standard I/O definitions         */
#include <stddef.h>
#include <string.h>

#ifdef VMS
#include <ssdef.h>                      /* VMS System Service Return Codes  */
#include <descrip.h>                    /* Character descriptor macros      */


            /* Include PAMS specific definition files. */

#pragma nostandard
#include pams_c_entry_point             /* PAMS function type declarations  */
#include pams_c_process                 /* Known Queue number definitions.  */
#include pams_c_group                   /* Known Group number definitions   */
#include pams_c_type_class              /* Generic Type/Class definitions   */
#include pams_c_return_status_def       /* PAMS return status definitions   */
#include pams_c_symbol_def              /* Generic PSEL/DSEL definitions    */
#pragma standard
#else
#include "p_entry.h"   /* PAMS function type declarations  */
#include "p_proces.h"                   /* Known Queue number definitions.  */
#include "p_group.h"                    /* Known Group number definitions   */
#include "p_typecl.h"                   /* Generic Type/Class definitions   */
#include "p_return.h"                   /* PAMS return status definitions   */
#include "p_symbol.h"                   /* Generic PSEL/DSEL definitions    */
#endif

#include "p_dmqa.h"   /* QUEUE ADAPTER */

#ifdef DMQ_QADAPTER
#define BUF_SIZE DMQA__MSGMAX
#endif

#ifdef VMS
#ifndef BUF_SIZE
#define BUF_SIZE 32000
#endif
#endif

#ifdef ultrix
#ifndef BUF_SIZE
#define BUF_SIZE 32000
#endif
#endif

#ifndef BUF_SIZE
#define BUF_SIZE 2048
#endif

        /* Define data type for PAMS target/source addresses.  */

typedef union
   {
   long int      all;
   struct
      {
      short int  queue;
      short int  group;
      } au;
   } que_addr;

            /* Declare local variables, pointers, and arrays */

static char             server;                 /* Answer from server prompt*/
static long int         status;                 /* Completion status code   */

static long int         attach_mode =
                         PSYM_ATTACH_TEMPORARY; /* Temp queue               */
static que_addr         queue_number;           /* Actual queue number      */
static long int         queue_type =
                         PSYM_ATTACH_PQ;        /* Type of queue            */
static char             queue_name[14] =
                         "SIMPLE_CLIENT";       /* Our name                 */
static long int         queue_name_len = 13;    /* Name length              */

static long int         search_list[3] = { PSEL_TBL_PROC,
                                           PSEL_TBL_GRP,
                                           PSEL_TBL_DNS_LOW };

static long int         search_list_len = 3;

struct psb              psb;                    /* PAMS status block        */

#ifdef VMS
$DESCRIPTOR (server_process, "SIMPLE_SERVER");  /* Server process name      */
#endif

        /* Define outbound ("put") message variables and arrays */

static que_addr         source;                 /* Source queue address     */
static char             put_buffer[BUF_SIZE+1]; /* Message buf. +1 null byte*/
static short int        put_class;              /* Message class code       */
static char             put_delivery;           /* Delivery mode            */
static short int        put_priority;           /* Message priority         */
static long int         put_resp_que;           /* Response queue           */
static short int        put_size;               /* Message size             */
static long int         put_timeout;            /* Time-out for blocked msg */
static short int        put_type;               /* Message type code        */
static char             put_uma;                /* Undeliverable msg action */


        /* Define inbound ("get") message variables and arrays */

static que_addr         target;                 /* Target queue address     */
static char             get_buffer[BUF_SIZE+1]; /* Message buf. +1 null byte*/
static short int        get_class;              /* Message class code       */
static short int        get_priority;           /* Message priority         */
static long int         get_select;             /* Message selection mask   */
static short int        get_size;               /* Message size             */
static long int         get_timeout;            /* Time-out for blocked msg */
static short int        get_type;               /* Message type code        */


main (argc,argv)
int argc;
char **argv;
{
#ifndef DMQ__QADAPTER
 long  local_long  = 0;
#endif
 short  local_short = 0;
        

   printf ("\n\nDMQATEST - Simple test program\n");


#ifdef DMQ__QADAPTER
        DMQA__argc = argc;
        DMQA__argv = argv;

        fflush (stdin);
        printf ("\n\nDo you wish to send inquiry message?\n>");

 put_buffer[0] = 0;
        if (gets(put_buffer) == '\0') exit(1);

        if ((put_buffer[0] == 'y') || (put_buffer[0] == 'Y'))
  {
  status = DMQA__test(get_buffer, -1, &local_short, 0, 0L);
         printf("DMQA__test returned status %d\n", status);
  }
#endif


   /* Call PAMS attach Q attach our program a temporary queue */
   /* by name and assign our queue number. */

   status = pams_attach_q (&attach_mode, &queue_number.all, &queue_type,
                           queue_name, &queue_name_len, 0, 0, 0, 0, 0); 

   if (status != PAMS__SUCCESS)
   {
      printf ("\nError returned by PAMS_ATTACH_Q code= '%d'\n", status);
      exit (status);
   }

            /* Attach queue was successful continue */

   printf ("\nAttached to %d.%d", queue_number.au.group, 
                                  queue_number.au.queue);

   /* Ask the user if he/she wishes to start the SIMPLE_CLIENT */
   /* to receive the messages. If not, then send the messages to ourself */

#ifndef DMQ__QADAPTER
   do
   {
      printf ("\nDo your wish to start the server process [Y/N]? ");
      server = toupper(getc(stdin));
   } while (server != 'Y' && server != 'N');

   /* If the user answered "Y"es, then use the system() function */
   /* to spawn a subprocess to run the DCL command procedure to start */
   /* the server.  Else, set the target queue to our own queue. */

   if (server == 'Y')
   {
      system ("@DMQ$EXAMPLES:SIMPLE_RUN_SERVER");
      printf ("\nStarting SIMPLE_SERVER process.");

      /* Use PAMS_LOCATE_Q find the address of our server.  The server */
      /* process must attach to the SPARE1 queue by name. */

       local_long = PSYM_WF_RESP;
       status = pams_locate_q ( "SPARE1",
    &queue_name_len,
    &target.all,
    &local_long,
                                0L,
                                0L,
    search_list,
    &search_list_len,
    0);

      if (status != PAMS__SUCCESS)
      {
         printf ("\nError returned by PAMS_LOCATE_Q code= '%d'\n", status);
         exit (status);
      }
   }
   else
#else
  if (1)
#endif
   {
      target.all = queue_number.all;
#ifndef DMQ__QADAPTER
      printf ("\nServer not started.  Messages will be sent to ourself.");
#endif
   }

   /* Initialize the variables that we will be using for messaging. */

   get_priority = 0;                    /* Receive all messages              */
   get_timeout = 600;                  /* 60 second time-out on rcv         */
   get_select = 0;                    /* No special selection mask         */

   put_class = 1;                    /* Send class 1 message              */
   put_priority = 0;                    /* Send at standard priority         */
   put_resp_que = 0;                    /* Response queue - default          */
   put_type = -123;                 /* Msg type is user defined          */
   put_timeout = 0;                    /* Accept standard time-out          */
   put_delivery = PDEL_MODE_NN_MEM;     /* No notify, memory queuing         */
   put_uma = PDEL_UMA_DISC;        /* Discard if undeliverable          */

   /* Prompt for a message, send it to the server and wait for  */
   /* a response from the server. */

   while (1==1)
   {
      /* Clear type-ahead buffer, then prompt for and read the message. */
      /* If EOF or EXIT, we are done, break out of the loop. */

      fflush (stdin);
      printf ("\n\nEnter message or enter EXIT to exit\n>");
      if (gets(put_buffer) == '\0') break;
      if (strncmp(put_buffer, "EXIT", 4) == 0) break;
      if (strncmp(put_buffer, "exit", 4) == 0) break;

      /* Set put message size to buffer length (+1 to include null). */

      put_size = strlen(put_buffer)+1;

      /* Send the message to the target queue.  If an error is */
      /* returned, report it and exit, otherwise display class & type. */

      status = pams_put_msg (   put_buffer,
                                (char *) &put_priority,
                                &target.all,
                                &put_class,
                                &put_type,
                                &put_delivery,
                                &put_size,
                                &put_timeout,
                                &psb,
                                &put_uma,
                                &put_resp_que,
                                0L,
                                0L,
                                0L); 
 
      if (status != PAMS__SUCCESS)
      {
         printf ("Error returned by PAMS_PUT_MSG code=%d\n", status);
         exit (status);
      }	

      printf ("\nCLIENT: Sent Msg to %d.%d class='%d', type='%d'\n",
              target.au.group, target.au.queue, 
              put_class, put_type);

      /* Now wait for the server process to send the reply message. */
      /* If time-out statuc, report it and continue.  If any other */
      /* error, report it and exit.  Otherwise, display what was received. */

      local_short = BUF_SIZE;
      status = pams_get_msgw (  get_buffer,
                                (char *) &get_priority,
                                &source.all,
                                &get_class,
                                &get_type,
                                &local_short,
                                &get_size, 
                                &get_timeout,
                                &get_select,
                                &psb,
                                (char *) 0,
                                0,
       (char *) 0,
                                0L,
                                0L); 

      if (status == PAMS__TIMEOUT)
         printf ("PAMS_GET_MSGW Timeout\n");

      else if (status != PAMS__SUCCESS)
      {
         printf ("\nError returned by PAMS_GET_MSGW code=%d\n", status);
         exit (status);
      }
      else
      {
         printf ("\nCLIENT: Received from %d.%d  class='%d', type='%d'\n",
                 source.au.group, source.au.queue, 
                 get_class, get_type);
         printf ("Message='%s'\n", get_buffer);
      }
   }
   /* Go back and wait for user to enter another message to send. */

   /* Clean-up and exit. */

#ifndef DMQ__QADAPTER
   /* If the server process was started, then use the system() function */
   /* to stop the server process using the DCL STOP command. */
   if (server == 'Y')
   {
      status = SYS$FORCEX (0, &server_process, 0);
      printf ("\nStopping SIMPLE_SERVER process.");
   }
#endif

   /* Detach from message bus. */

   (void) pams_exit();

   /*  Tell the user we're done, then exit. */

   printf ("\n\nProgram finished\n");
   exit (PAMS__SUCCESS);
}



/*  DMQA__hton* and DMQA_ntoh*    Convert datatype storage format         */
/*                                                                          */
/*  These functions are similar to to the unix networking "ntoh" and "hton" */
/*  functions; they take a short or longword (whose byte order is specific  */
/*  to the system being used) and will produce a fixed "known" byte         */
/*  order - independent of the system. They differ, however, in that they   */
/*  automatically "determine" what the byte order of the platform being     */
/*  is. rather than using hard-coded conditions. They also are provided     */
/*  because not all systems offer the other "ntoh" type functions.          */
/*                                                                          */
/*  There are 3 definitions that can be used to tailor the function-        */
/*  ality of this file beyond it's normal logic of determining byte order.  */
/*  DMQA__USE_VAX_ORDER and DMQA__USE_NETWORK_ORDER determine whether the   */
/*  "byte order" PRODUCED AS A RESULT is to be in VAX byte order, or in     */
/*  Network byte order (little endian or big endian). The default is to use */
/*  VAX order, but this is only to make life easier in debugging DmQA       */
/*  applications. The other definition, DMQA__REVERSE_BITS, will cause the  */
/*  bits within the resulting bytes to be placed in reversed order. There   */
/*  are no current platforms that require this; the default is keep bits    */
/*  intact.                                                                 */
/*                                                                          */
/*  These functions are designed to be generic, and have no product         */
/*  specific logic other than the variable and functions names themselves.  */
/*                                                                          */
/*                                                                          */
/*  Version     Date    Who     Description                                 */
/*                                                                          */
/*  V1.0    08-Mar-1991 kgb     First release; Keith Barrett                */
/*  V1.0    22-Apr-1991 kgb     Minor appearence changes.                   */
/*                                                                          */
/*                                                                          */


/* Compiler options - determine whether to use full argument prototypes.    */
/* (Note: Cannot use a prototype prefix macro, due to AS/400's inability    */
/* of allowing spaces in a function definition).                            */

#ifdef VMS
#ifndef __USE_ARGS
#define __USE_ARGS TRUE
#endif
#endif

#ifdef ultrix
#ifndef __USE_ARGS
#define __USE_ARGS TRUE
#endif
#endif

#ifdef __STDC__
#ifndef __USE_ARGS
#define __USE_ARGS TRUE
#endif
#endif

#ifdef __ANSI__
#ifndef __USE_ARGS
#define __USE_ARGS TRUE
#endif
#endif

#ifdef __SAA__
#ifndef __USE_ARGS
#define __USE_ARGS TRUE
#endif
#endif
/* IBM */

#ifdef __SAA_L2__
#ifndef __USE_ARGS
#define __USE_ARGS TRUE
#endif
#endif
/* IBM */

#ifdef __EXTENDED__
#ifndef __USE_ARGS
#define __USE_ARGS TRUE
#endif
#endif
/* IBM */


#ifndef DMQA__USE_NETWORK_ORDER
#define DMQA__USE_NETWORK_ORDER 0
#endif

#ifndef DMQA__USE_VAX_ORDER
#if DMQA__USE_NETWORK_ORDER
#define DMQA__USE_VAX_ORDER 0
#else
#define DMQA__USE_VAX_ORDER 1
#endif
#endif

/* The above defines determine the byte order used - VAX or network. The */
/* default (per the decision to be as compatible to VMS as possible) is  */
/* VAX byte order, but this can be changed without program impact (as    */
/* long as both communicating programs use the same order.               */


#ifndef DMQA__REVERSE_BITS
#define DMQA__REVERSE_BITS 0
#endif
/* This flag (when set) will cause the bits within a byte to be reversed */
/* before storing result. This may be needed for some specific platforms */


    static int need_init  = -1;
    char DMQA__long_order[4] = {0,0,0,0};
    char DMQA__short_order[2] = {0,0};

#if DMQA__REVERSE_BITS
    static unsigned char    bit_array[16]   = { 0,  8,  4, 12,  2, 10,  6, 14,
      1,  9,  5, 13,  3, 11,  7, 15};
#endif


/* This function initializes the internal array that determines the byte   */
/* order of this platform. It is called automatically if needed. It        */
/* determines the order by placing a known numerical constant into memory, */
/* then examining the bytes to see what landed where.                      */
         
void DMQA__init_nbo()
{
#if DMQA__USE_VAX_ORDER
    unsigned long    temp_longword  = 67305985L;
    unsigned short   temp_short     = 513;
#else
    unsigned long    temp_longword  = 16909060L;
    unsigned short   temp_short     = 258;
#endif
/* The above constants will produce numerical byte values of 1,2,3,4 or */
/* 4,3,2,1; depending on system and constant chosen.                    */

    extern  int     need_init;
    extern  char    DMQA__long_order[4];
    extern  char    DMQA__short_order[2];
     char    *char_ptr;

    if (need_init)
    {
 char_ptr       = (char *) &temp_longword;
 DMQA__long_order[0]   = *char_ptr++;
 DMQA__long_order[1]   = *char_ptr++;
 DMQA__long_order[2]   = *char_ptr++;
 DMQA__long_order[3]   = *char_ptr;
 /* This array determines how our system is storing bytes. We store a */
 /* constant and examine the resulting bytes */

 char_ptr        = (char *) &temp_short;
 DMQA__short_order[0]   = *char_ptr++;
 DMQA__short_order[1]   = *char_ptr;
 /* Same idea, but for shorts */

 need_init = 0;
 /* We need only do this once */
    }
}


/* This function converts an unsigned long  HOST value (local to our system) */
/* into an unsigned longword in the proper byte order for networking usage.  */

unsigned long DMQA__htonl(ulong_value)
unsigned long ulong_value;
/* Convert host order to network order for longwords */
{
#if DMQA__REVERSE_BITS
    extern  unsigned char   bit_array[16];
     unsigned char   lowbits;     /* Temp area for storing lowbits */
     unsigned char   highbits;     /* temp area for high bits */
#endif
    extern  int      need_init;
    extern  char     DMQA__long_order[4];
     unsigned char   *charptr;     /* ptr to local variable */
     unsigned char   *charptr2;     /* ptr to argument value */
     unsigned long   local_long;     /* local longword */
     int      i = 0;     /* index */
     unsigned char   local_char;     /* Local byte */


    if (need_init) DMQA__init_nbo();     /* Init if needed */

    local_long  = 0;                        /* Init variable */

    charptr = (unsigned char *) &local_long;
    charptr2 = (unsigned char *) &ulong_value;
    /* Set up pointers */

    do
    {
 local_char = *charptr2++;     /* Grab a byte */

#if DMQA__REVERSE_BITS
 highbits    = bit_array[local_char & 15] << 4;
 lowbits     = bit_array[(local_char >> 4)];
 local_char  = lowbits | highbits;
 /* Reverse the bits if desired */
#endif
 *(charptr + (DMQA__long_order[i] - 1)) = local_char;
 /* copy the byte to its "correct" location in our local variable */
    } while (++i < 4);

    return local_long;
}



/* This function converts an unsigned short HOST value (local to our system) */
/* into an unsigned short in the proper byte order for networking usage.     */

unsigned short DMQA__htons(ushort_value)
unsigned short ushort_value;
/* Convert host order to network order for shortwords */
{
#if DMQA__REVERSE_BITS
    extern  unsigned char   bit_array[16];
     unsigned char   lowbits;     /* Temp area for storing lowbits */
     unsigned char   highbits;     /* temp area for high bits */
#endif
    extern  int      need_init;
     unsigned char   *charptr;     /* ptr to local variable */
     unsigned char   *charptr2;     /* ptr to argument value */
     unsigned short  local_short;    /* local shortword */
     int      i = 0;     /* index */
     unsigned char   local_char;     /* Local byte */


    if (need_init) DMQA__init_nbo();     /* init if needed */

    local_short = 0;                        /* Init variable */

    charptr = (unsigned char *) &local_short;
    charptr2 = (unsigned char *) &ushort_value;
    /* Set up pointers */

    do
    {
 local_char = *charptr2++;     /* Grab a byte */

#if DMQA__REVERSE_BITS
 highbits    = bit_array[local_char & 15] << 4;
 lowbits     = bit_array[(local_char >> 4)];
 local_char  = lowbits | highbits;
 /* Reverse the bits if desired */
#endif
 *(charptr + (DMQA__short_order[i] - 1)) = local_char;
 /* Copy byte into its "correct" location in out local variable */
    } while (++i < 2);

    return local_short;
}



/* This function converts an unsigned long in the NETWORK order (produced */
/* by DMQA__htonl) into unsigned long HOST format (local to our system)   */

unsigned long DMQA__ntohl(ulong_value)
unsigned long ulong_value;
/* Convert network order to host order for longwords*/
{
    extern int  need_init;

    if (need_init) DMQA__init_nbo();
    return DMQA__htonl(ulong_value);
    /* Just call the same function again -- it undoes itself */
}



/* This function converts an unsigned short in the NETWORK order (produced */
/* by DMQA__htonl) into unsigned short HOST format (local to our system)   */

unsigned short DMQA__ntohs(ushort_value)
unsigned short ushort_value;
/* Convert network order to host order for shortwords */
{
    extern int  need_init;

    if (need_init) DMQA__init_nbo();
    return (unsigned short) DMQA__htons(ushort_value);
    /* Just call the same function again -- it undoes itself */
}

