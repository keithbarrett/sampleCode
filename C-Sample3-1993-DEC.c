/*
** Function: These are the main routines for creating link drivers on
**  DECmessageQ/VMS. The actually transport used depends on what
**  transport routines are linked with this image.
** 
**              Note:   These routines assume tha the CommServer is running,
**                      and that the global structures it maintains have
**                      been initialized.
**
**  IDENT HISTORY:
**
**  Version     Date    Who     Description
**  V21-06  23-Jun-1993 kgb     Added CIP HUNG bit usage. Added msgs.
**  V21-05  21-Jun-1993 kgb     Changed stat updates
**  V21-04  17-Jun-1993 kgb     Changed to remq/insq logic
**  V21-03  16-Jun-1993 kgb     Add support for "ONE-LINK" transport connects
**  V21-02  11-Jun-1993 kgb     Removed timestamp on startup.
**                              Fixed version numbers in ident history.
**                              Load pointer to CommServer tcb in ldstruct.
**                              Process DMQCS_LDMS_DO_NOT_LOG.
**                              Added LDMS_STATUS_RPT logic.
**                              Created dmqcs_free_group() and
**    _                         dmqcs_xfer_group_to_cs() routines.
**  V21-01  01-Jun-1993 kgb     Changed usage of _ISF_ bits
**  V20-19  30-Apr-1993 kgb     Misc improvements for DECnet LD
**                              Created dmqcs_free_all_groups() routine
**                              Spread out performance more fairly over events
**  V20-18  02-Apr-1993 kgb     Added logic for DMQCS__UNTRANSLATABLE
**  V20-17  19-Mar-1993 kgb     Added logic for processing DMQCS__TAKE_UMA
**  V20-16  09-Mar-1993 kgb     Fixed "connect to self" accvio
**  V20-15  25-Feb-1993 kgb     Moved AST routines to transport level
**  V20-14  25-Feb-1993 kgb     Support OOB I/O events
**  V20-13  17-Feb-1993 kgb     Used ldstruct.flags for case sensitive searches
**  V20-12  11-Feb-1993 kgb     Changed many _MOVC5() to _MOVC3()
**  V20-11  01-Feb-1993 kgb     Use DMQCS_LINE_BUF_MAX
**  V20-10  27-Jan-1993 kgb     Minor improvement to shutdown. Fixed
**                              uninited vars in close_link and shutdown
**  V20-09  25-Jan-1993 kgb     Improve "changing outbound to match inbound"
**  V20-08  20-Jan-1993 kgb     bad conditional call to take_uma
**  V20-07  18-Jan-1993 kgb     p_entry changes for obsolete prototypes
**  V20-06  12-Jan-1993 kgb     Don't WPING on VMS/AXP. Added a newline if dbg
**  V20-05  14-Oct-1992 mpm     Change to get OS type at run-time
**  V20-02  28-Aug-1992 kgb     Minor Alpha Changes
**  V20-01  ??-???-???? kgb     SSB release DmQ V2.0
**  Z20-03  09-Jun-1992 kgb     improved write completion loop
**  Z20-02  22-May-1992 kgb     FT2B release
**  Z20-01  22-May-1992 kgb     FT2 release
**  Y01-01  ??-???-???? kgb     FT1 release - Initial program
**
**--
*/


/*
**  Notes about this program --
**
**  1) The routine can only BASE processing on the state.prefix and
** state.generic fields; the state.substate field is reserved for
** transport specific triggering. The best this file can do is
** is clear that field, or blindly increment that field if the
** state.generic is not changing.
**
**  2) When a link has to come down; the close routine sets the shutdown bit
** in the LCB, and calls the transport specific CLOSE routine (which
** closes the channel). Closing the channel will cause all ASTs and I/O
** to awaken, but the shutdown pending bit will prevent further queing.
** As these events are processed, the IOSTRs will be freed back. When
** There are no more IOSTRs, the LCB will either be removed and the
** group freed, or a retry timer will be queued, depending on various
** circumstances.
**
**  3) The routines in this file have to act in a generic manner. Transport
** dependent activity does not belong here.
**
**  4) This file should be compiled with /STAND=PORT to keep it clean and
** generic.
*/



#ifdef __DECC
#pragma module dmqcs_linkdriver DMQ$VERSION 
#else
#pragma nostandard
#pragma builtins
#module        dmqcs_linkdriver DMQ$VERSION 
#pragma standard
#endif

#include <stddef.h>   /* Standard definitions */
#include <stdlib.h>   /* Standard library */
#include <string.h>   /* String and mem functions */
#include <stdio.h>   /* Standard I/O */
#include <types.h>   /* Data type definitions */
/* System include files */

#include <ssdef.h>   /* VMS SS$_xxx definitions */
#include <descrip.h>   /* VMS String desriptors */
#include <lib$routines.h>  /* VMS LIB$ routines */
#include <libdef.h>   /* VMS LIB$_ return codes */
#include <starlet.h>   /* SYS$xxx calls */
/* VMS Specific include files */

#include "dmq_c_environment.h"
#include "p_entry.h"   /* PAMS function type declarations  */
#include "pams_c_internal_typecl.h"     /* Generic Type/Class definitions   */
#include "p_return.h"   /* PAMS return status definitions   */
#include "p_symbol.h"   /* Generic PSEL/DSEL definitions    */
#include "pams_c_msg_def.h"  /* Message definition file          */
#include "p_proces.h"   /* Queue name definitions           */
#include "pams_c_mcs.h"   /* mcs definitions                  */
/* DECmessageQ include files */

#include "dmqcs_c_transport_ref.h" /* DMQCS definition file */
#include "dmq_c_return_status_def.h" /* Return status codes */
#include "dmqcs_c_in.h"   /* Byte ordering functions */
#include "dmqcs_c_ld_msg_def.h"  /* Various LD symbols */
#include "dmqcs_c_ld_def.h"  /* Link Driver file */
/* DMQCS include files */

#pragma nostandard
LD_STRUCT _align(QUADWORD) ldstruct;        /* LinkDriver Structure */
EVENT_STRUCT _align(QUADWORD) *qev_ptr;
EVENT_STRUCT _align(QUADWORD) *q_event_ptr;
#pragma standard

#define YES   -1
#define NO   0
#define SEND_XGROUP  1
#define RECV_XGROUP  0
#define PAMS_BUFFER_TYPE YES
#define NON_PAMS_BUFFER_TYPE NO
#define DO_THE_SEND  YES

static long      attach_mode     = PSYM_ATTACH_BY_NAME;
static q_address     queue_number;     /* Actual queue number */
static long      queue_type     = PSYM_ATTACH_PQ;
static long      *long_queue_addr;
static long const     one      = 1L;
static long const     psym_value     = PSYM_IGNORE;

static LCB      *spare_lcb_ptr = (LCB *)0;
static TCB      *spare_tcb_ptr = (TCB *)0;
static GROUP      *spare_group_ptr = (GROUP *)0;
static IOSTR      *spare_iostr_ptr = (IOSTR *)0;
static EVENT_STRUCT     *spare_event_ptr = (EVENT_STRUCT *)0;
/* The above four fields are not used in the program. They exist to help */
/* interactive debugging by providing some spare stattyped storage ptrs */

extern char     our_queue_name[MAX_QUEUE_NAME_LEN + 1];
extern char     our_transport_name[TP_FIELD_LEN + 1];

char line_buf[DMQCS_LINE_BUF_MAX];
char time_buf[30];

#pragma nostandard
globalref unsigned char pams_server_process;
globalref unsigned char pams_map_sec_u;
globalref unsigned char pams_group_section_access;
globalref char header_display_is_required;
globalref unsigned char pams_override_suppression;
globalref struct mcs *mcs_ptr;
globalref long  pams_ef;
#pragma standard



/*
**    CLOSE LINK
**
**
** Module:      dmqcs_close_link()
**
** Function:    Closes and queues a shutdown for the specified link:
**
**  1) Sets the shutdown pending bit on the LCB to prevent
**     further ASTs and event queueing.
**
**  2) Calls the transport specific close routine, which
**     closes the channel - causing all I/O to awaken.
**
**  3) The events will be processed as standard errors
**     (as if the link came down by itself), and UMAs should
**     take place. However, the shutdown pending will also
**     cause the iostrs to be removed. When all iostrs are
**     freed, the link will either reque or get removed.
**
**  This is an upper level routine, that calls the transport
**  specific close routine to terminate the link.
**
**
** Inputs:      lcb_ptr     Pointer to the LCB
**  flag     remove (dealloc structures) link after shutdown?
**
** Outputs:     none
**
** External Subroutines:
**  dmqcs_tp_close()
**  dmqcs_queue_link_event()
**  dmqcs_cancel_timers()
**  dmqcs_move_iostr()
**  dmqcs_send_ld_msg()
**  pams_c_error_log()
**  printf()
**  sprintf()
**  dmqcs_gmtime()
**  
*/

void dmqcs_close_link(lcb_ptr, flag)

LCB *lcb_ptr;
long flag;
{
    long    sys_status;
    GROUP   *group_ptr;
    IOSTR   *local_iostr;
    int     call_pams     = YES;
    LCB     *olcb_ptr     = (LCB *)0;
    int     process_group   = NO;

    void    dmqcs_tp_close();
    void    dmqcs_queue_link_event();
    void    dmqcs_move_iostr();
    long    pams_wake();
    long    dmqcs_send_ld_msg();
    void    pams_c_error_log();
    void    dmqcs_gmtime();
    void    dmqcs_cancel_timers();

    DMQCS_DBG_ENTRY("** Entering dmqcs_close_link() **")

    group_ptr = lcb_ptr->group_ptr;
    if (group_ptr) process_group = YES;

    if (flag) lcb_ptr->status |= DMQCS_LCBS_REMOVE_LINK;

    if (lcb_ptr->status & DMQCS_LCBS_SHUTDOWN_PEND)
    {
 DMQCS_DBG_INLINE(">Link already shutting down<")
 (void) dmqcs_tp_close(lcb_ptr);     /* This is needed! */
    }
    else
    { 
 lcb_ptr->status |= DMQCS_LCBS_SHUTDOWN_PEND;

 lcb_ptr->status &=  (~( DMQCS_LCBS_AWAITING_LINK |
    DMQCS_LCBS_PROTOCOL_HANDSHAKE   |
    DMQCS_LCBS_WRITE_EXISTS));

 switch (lcb_ptr->cur_state.id)
 {
     case 'W' :
     case 'C' :
  if (group_ptr)
  {
      group_ptr->group_status &= (~DMQCS_GS_OOB_UP);

      if (group_ptr->group_status & DMQCS_GS_WRITE_UP)
      {
   group_ptr->group_status &= (~DMQCS_GS_WRITE_UP);

   if (dmq_env_ptr->stats_enabled)
   {
       DMQCS_LD_UPDATE_STAT_FIELD(
    &group_ptr->wmsg_stats,
    &group_ptr->wmsg_stats.connections_lost,
    one);
       DMQCS_LD_UPDATE_STAT_FIELD(
    &group_ptr->cur_tcb.wlink_stats,
    &group_ptr->cur_tcb.wlink_stats.connections_lost,
    one);
       dmqcs_gmtime(
    &group_ptr->wmsg_stats.time_disconnected);
       dmqcs_gmtime(
    &group_ptr->cur_tcb.wlink_stats.time_disconnected);
   }
      }

      olcb_ptr = (LCB *) group_ptr->cur_tcb.lcbr_ptr;
  }

  if (lcb_ptr->tcb_ptr)
  {
      DMQCS_LD_UPDATE_STAT_FIELD(
   &lcb_ptr->tcb_ptr->wlink_stats,
   &lcb_ptr->tcb_ptr->wlink_stats.connections_lost,
   one);
      dmqcs_gmtime(
   &lcb_ptr->tcb_ptr->wlink_stats.time_disconnected);
  }
  break;


     case 'R' :
     case 'L' :
  if (group_ptr)
  {
      group_ptr->group_status &= (~DMQCS_GS_OOB_UP);

      if (group_ptr->group_status & DMQCS_GS_READ_UP)
      {
   group_ptr->group_status &= (~DMQCS_GS_READ_UP);

   if (dmq_env_ptr->stats_enabled)
   {
       DMQCS_LD_UPDATE_STAT_FIELD(
    &group_ptr->rmsg_stats,
    &group_ptr->rmsg_stats.connections_lost,
    one);
       DMQCS_LD_UPDATE_STAT_FIELD(
    &group_ptr->cur_tcb.rlink_stats,
    &group_ptr->cur_tcb.rlink_stats.connections_lost,
    one);
       dmqcs_gmtime(
    &group_ptr->rmsg_stats.time_disconnected);
       dmqcs_gmtime(
    &group_ptr->cur_tcb.rlink_stats.time_disconnected);
   }
      }

      olcb_ptr = (LCB *) group_ptr->cur_tcb.lcbw_ptr;
  }

  if (lcb_ptr->tcb_ptr)
  {
      DMQCS_LD_UPDATE_STAT_FIELD(
   &lcb_ptr->tcb_ptr->rlink_stats,
   &lcb_ptr->tcb_ptr->rlink_stats.connections_lost,
   one);
      dmqcs_gmtime(
   &lcb_ptr->tcb_ptr->rlink_stats.time_disconnected);
  }
  break;


     case 'X' :
  DMQCS_DBG_INLINE("This is the listener LCB")
  process_group = NO;
  break;


     default :
  break;
 }


 if (process_group)
     dmqcs_cancel_timers(lcb_ptr, group_ptr, -1L,
  (char) lcb_ptr->cur_state.id);

 (void) dmqcs_tp_close(lcb_ptr);

 if (lcb_ptr->status & DMQCS_LCBS_DROP_ALL_LINKS)
 {
     lcb_ptr->status &= (~DMQCS_LCBS_DROP_ALL_LINKS);

     if (olcb_ptr)
     {
  olcb_ptr->status &= (~DMQCS_LCBS_DROP_ALL_LINKS);

  if ((olcb_ptr->status & DMQCS_LCBS_SHUTDOWN_PEND) == 0)
  {
      olcb_ptr->status |=
   (lcb_ptr->status & DMQCS_LCBS_DROP_OWNERSHIP);

      DMQCS_DBG_ENTRY("CLOSING DOWN OPPOSITE LINK")
      dmqcs_close_link(olcb_ptr, YES);
      DMQCS_DBG_ENTRY("CONTINUING WITH ORIGINAL LINK")
  }
     }
 }


 /*
 **  We've done the very urgent stuff, now let's proceed
 */

 if (lcb_ptr->cur_state.id == 'W')
 {
     while (local_iostr = lcb_ptr->next_iostr)
     {
  /*
  **  We have to "dequeue" these iostrs and queue them up
  **  as write-completed events with shutdown status so
  **  UMA's can be processed.
  */

  local_iostr->status = DMQCS__ABORTED;

  lcb_ptr->next_iostr = (IOSTR *) local_iostr->fptr;
  /* Get next_iostr */

  dmqcs_move_iostr(   local_iostr,
        &lcb_ptr->iostr_list,
        &lcb_ptr->last_iostr,
        &ldstruct.write_done_start,
        &ldstruct.write_done_end,
        YES,
        NO);

  lcb_ptr->no_iostrs--;
  lcb_ptr->event_count++;
  /* move from LCB to the write complete event list */

  if (call_pams)
  {
      (void) pams_wake(&psym_value, &psym_value);
      call_pams= NO;     /* Just call this routine once */
  }
     }
 }


 if (process_group)
 {
     if (group_ptr->link_state == DMQCS_GLS_LINK_COMPLETE)
     {
   if (*lcb_ptr->system_addr)
      (void) sprintf(line_buf,
       "Dropping connection to group %d (%s) on system %s (%s)",
       group_ptr->group_number,
       group_ptr->group_name,
       lcb_ptr->system_name,
       lcb_ptr->system_addr);
   else
      (void) sprintf(line_buf,
       "Dropping connection to group %d (%s) on system %s",
       group_ptr->group_number,
       group_ptr->group_name,
       lcb_ptr->system_name);

  DMQCS_DBG_INLINE(line_buf)

  group_ptr->link_state = DMQCS_GLS_CONNECT_PENDING;

  (void) dmqcs_send_ld_msg(   PAMS_COM_SERVER,
         MSG_TYPE_LD_LINK_DOWN,
         0L,
         group_ptr->group_number,
         DMQCS__SUCCESS,
         (char *)0,
         0);
  /* Tell commserver the link went down */
     }

     if ((group_ptr->group_status &
      (DMQCS_GS_READ_UP | DMQCS_GS_WRITE_UP)) == 0)
  group_ptr->link_state = DMQCS_GLS_NO_CONNECTION;
 }
    }


    /*
    ** Now; Queue a shutdown event just to be sure that at least one
    ** event for this link exists to cause the rest of the shutdown.
    ** This will also sets the event state to  shutdown if it doesn't
    ** happen otherwise.
    */

    dmqcs_queue_link_event( (IOSTR *)0,
       lcb_ptr,
       DMQCS__SUCCESS,
       (char) -1,
       (char) DMQCS_LS_SHUTDOWN,
       (char) 0,
       "dmqcs_close_link()",
       &qev_ptr);

    DMQCS_DBG_INLINE("-- Exiting dmqcs_close_link() --")
}



/*
**       SEND EXTERNAL ACCEPT
**
**
** Module:      dmqcs_send_xtern_accept()
**
** Function:    This sends the response to an external request for group
**  ownership transfer, when that group was originally busy
**  at the time of the request. This routine is called by
**  shutdown when it discovers an outstanding request.
**
** Inputs:      notify_ptr Pointer to LD_EXTERN_RESP record
**
** Outputs:     None
**
** External Subroutines:
**  dmqcs_dealloc_lbuf()
**  sprintf()
**  pams_c_error_log()
**  dmqcs_send_ld_msg()
*/

static void dmqcs_send_xtern_accept(notify_ptr)

char *notify_ptr;
{
    long     sys_status;
    char     *lbuf_ptr;
    LD_EXTERN_RESP  *xtern_rec_ptr;
    dmq_address     *q_ptr;

    void    pams_c_error_log();
    long    dmqcs_send_ld_msg();
    void    dmqcs_dealloc_lbuf();


    DMQCS_DBG_ENTRY("* Entering dmqcs_send_xtern_accept() *")

    lbuf_ptr     = notify_ptr;
    xtern_rec_ptr   = (LD_EXTERN_RESP *) notify_ptr;
    q_ptr     = (dmq_address *) &xtern_rec_ptr->ld_addr;

    (void) sprintf(line_buf,
 "Transferring group %d ownership to %d.%d",
 xtern_rec_ptr->group_no,
 q_ptr->au.group_id,
 q_ptr->au.queue_id);
    PAMS_LOG("I", line_buf, I_ERROR);

    sys_status = dmqcs_send_ld_msg( xtern_rec_ptr->ld_addr,
        MSG_TYPE_LD_REQUEST_ACCEPTED,
        0L,
        xtern_rec_ptr->group_no,
        DMQCS__SUCCESS,
        (char *) &xtern_rec_ptr->user_data,
        xtern_rec_ptr->user_len);
    if ((sys_status & 1) == 0)
    {
 (void) sprintf(line_buf,
     "Unable to send link arbitration msg for group %d to %d.%d",
     xtern_rec_ptr->group_no,
     q_ptr->au.group_id,
     q_ptr->au.queue_id);
 PAMS_LOG("W", line_buf, sys_status);
    }

    dmqcs_dealloc_lbuf(&lbuf_ptr);

    DMQCS_DBG_INLINE("- Exiting dmqcs_send_xtern_accept() -")
}

/*
**       COMPLETE LINK SHUTDOWN
**
**
** Module:      dmqcs_SHUTDOWN()
**
** Function:    This routine handles the final shutdown process for a link. It
**  must handle the following items:
**
**  1) Queueing a retry timer if necessary
**  2) Removal of the LCB and from the group if necessary
**  3) "freeing" the group if necessary
**  4) Sending a user response to another linkdriver if needed
**
**  This routine will be called by any event that catches the
**  shutdown pending bit on within the lcb. 
**
**
** Inputs:      lcb_ptr     Pointer to the LCB
**
** Outputs:     None
**  (queues error event)
**
** External Subroutines:
**  dmqcs_cancel_timers()
**  dmqcs_timer_number()
**  pams_set_timer()
**  dmqcs_remove_lcb()
**  dmqcs_is_lcb_inuse()
**  dmqcs_dealloc_lbuf()
**  dmqcs_dealloc_oob_lbuf()
**  printf()
**  sprintf()
**  pams_c_error_log()
**  dmqcs_close_link()
**  dmqcs_send_ld_msg()
**  exit()
**  dmqcs_send_xtern_accept()
**  dmqcs_find_tcb()
**  dmqcs_free_all_groups()
**  _dmqcs_xfer_group_to_cs()
*/

static void dmqcs_SHUTDOWN(lcb_ptr)

LCB  *lcb_ptr;
{
    long     sys_status     = DMQCS__SUCCESS;
    GROUP     *group_ptr     = (GROUP *)0;
    char     *notify_ptr     = (char *)0;
    long     timer_no     = 0L;
    char     timer_type     = 'P';
    long     delay     = 0L;
    TCB      *tcb_ptr;
    TCB      *ctcb_ptr     = (TCB *)0;
    short     qword[4];
    short     group_no     = 0;
    IOSTR     *local_iostr;
    long     *long_ptr     = (long *)0;
    int      display_msg     = YES;
    int      ctcb_trigger    = NO;
    LCB      *xlcb_ptr;      /* Used for "opposite" lcb */
    LCB      *old_lcb_ptr;
    LD_UP_STRUCT    ld_up_msg;
    long     old_lcb_status  = 0L;
    long     *queue_ptr;
    int      local_index     = 0;
    long     give_up_mask    = DMQCS_LCBS_DROP_OWNERSHIP |
     DMQCS_LCBS_SUPPRESS_RECONNECT   |
     DMQCS_LCBS_REMOVE_LINK;
    IOSTR     *tmp_iostr_ptr;      /* Temp uses */
    GROUP     *tmp_group_ptr;      /* Temp uses */
    short     tmp_group_no    = 0;     /* Temp uses */

    void    dmqcs_dealloc_lbuf();
    void    dmqcs_dealloc_oob_lbuf();
    long    dmqcs_remove_lcb();
    long    dmqcs_is_lcb_inuse();
    void    pams_c_error_log();
    long    dmqcs_send_ld_msg();
    long    dmqcs_timer_number();
    long    pams_set_timer();
    void    dmqcs_cancel_timers();
    TCB     *dmqcs_find_tcb();


    DMQCS_DBG_ENTRY("** Entering dmqcs_SHUTDOWN() **")

    if (lcb_ptr == (LCB *)0)
    {
 sys_status = DMQCS__BADPARAM;
 PAMS_LOG("E", "SHUTDOWN routine received a bad parameter", DMQCS__INTERNAL);
    }
    else
    {
 if ((lcb_ptr->status & DMQCS_LCBS_SHUTDOWN_PEND) == 0)
 {
     sys_status = DMQCS__BADLCB;
     PAMS_LOG("E", "SHUTDOWN routine received a bad parameter", sys_status);
     /* LCB wasn't shutting down */
 }
    }

    if ((sys_status & 1) == 0) return;


    /*
    ** First, we set up some kibbles (variables) and bits :-)
    */

    tcb_ptr = lcb_ptr->tcb_ptr;
    group_ptr   = lcb_ptr->group_ptr;

    if (group_ptr)
    {
 group_no    = group_ptr->group_number;
 long_ptr    = (long *) &group_ptr->queue;

 if (group_no != ldstruct.our_group_no)
     ctcb_ptr = &group_ptr->cur_tcb;

 if (ctcb_ptr)
     notify_ptr = ctcb_ptr->extern_ptr;

 if (group_ptr->group_status & ( DMQCS_GS_UNSOLICITED    |
     DMQCS_GS_DISABLED))
     lcb_ptr->status |= give_up_mask;

 if (ldstruct.flags & (  DMQCS_LDFLAG_PROGRAM_SHUTDOWN |
    DMQCS_LDFLAG_LISTENER_DOWN))
     lcb_ptr->status |= give_up_mask;
    }
    else
 lcb_ptr->status |= (DMQCS_LCBS_REMOVE_LINK  |
       DMQCS_LCBS_SUPPRESS_RECONNECT);
 /* Can't process a link with no group information */


    if (ldstruct.flags & (  DMQCS_LDFLAG_LISTENER_DOWN  |
       DMQCS_LDFLAG_PROGRAM_SHUTDOWN))
 display_msg = NO;

    if (dmqcs_debug)
 (void) printf("%s Request is for lcb #%u, group %d\n",
     DMQCS_DBG_PREFIX, (unsigned long) lcb_ptr->seq_no, group_no);

    if (lcb_ptr->status & DMQCS_LCBS_SUPPRESS_MSG) display_msg = NO;


    if (lcb_ptr->oob_iostr)
    {
 tmp_iostr_ptr = lcb_ptr->oob_iostr;

 if ((lcb_ptr->chan  == 0)  &&
     (lcb_ptr->oob_chan  == 0)  &&
     (tmp_iostr_ptr->istatus == 0)  &&
     (tmp_iostr_ptr->area_ptr == (char *)0) &&
     (tmp_iostr_ptr->tp_struct == (char *)0) )
 {
     lcb_ptr->oob_iostr = (IOSTR *)0;
     /*  For some reason that I can't track down, there is
     **  a race condition which causes a deallocated
     ** oob_iostr to still appear in the lcb on a
     **  shutdown. This is a patch/hack, so that the link REALLY
     **  comes down, and we don't double-free this iostr.
     */

     if (group_no)
     {
  (void) sprintf(line_buf,
      "Warning - Deallocated OOB_IOSTR removed from group %d LCB",
      group_no);
  PAMS_LOG("W", line_buf, W_ERROR);
     }
     else
  PAMS_LOG("W",
      "Warning - Deallocated OOB_IOSTR removed from LCB",
      W_ERROR);

#ifdef DMQCS_NO_SECURITY
     if (ldstruct.no_lcbs)
  ldstruct.no_dropped_oob_iostrs++;
     /* just for memory debugging */
#endif
 }
    }


    sys_status = dmqcs_is_lcb_inuse(lcb_ptr);


    if (sys_status & 1)
    {
 if (lcb_ptr->local_iostr.istatus & DMQCS_ISF_OOB)
     dmqcs_dealloc_oob_lbuf(&lcb_ptr->local_iostr.area_ptr);
 else
     dmqcs_dealloc_lbuf(&lcb_ptr->local_iostr.area_ptr);
 /* Free up lbuff if exists */


 /*
 **  Now we have to perform some activities based on what type of link
 **  went down.
 */

 switch (lcb_ptr->cur_state.id)
 {
     case 'W' :
     case 'C' :
  DMQCS_DBG_INLINE("Outbound/Write went down")

  if (lcb_ptr->status & DMQCS_LCBS_RELOAD_OBOUND)
      lcb_ptr->status &= (~DMQCS_LCBS_SUPPRESS_RECONNECT);

  if ((lcb_ptr->status & DMQCS_LCBS_SUPPRESS_RECONNECT) == 0)
  {
      timer_no = dmqcs_timer_number(
   group_ptr,
   (char) DMQCS_TMRTYPE_RECONNECT,
   (char) 'W');

      /*
      **  If the next nt to process is the beginning of
      ** the list, then our timer becomes a reconnect.
      ** Otherwise, we are performing node failover.
      */

       if (ctcb_ptr)
       if (ctcb_ptr->next_nt < 1)
        ctcb_trigger = YES;

       if (ctcb_trigger)
    delay = ctcb_ptr->reconnect_delay;
      else
      {
   /*
   **  The first cycle of a node failover acts slightly
   **  different than the cycles which occur after the
   **  reconnect interval. Mainly; any output is suppressed
   **  on further cycles. The RECONNECT_TRIG gets set if
   **  a reconnect timer has fired off once, and gets
   **  cleared if a link comes up. If we have this flag,
   **  then use the reconnect timer number for ALL
   **  node failovers, so we keep this info intact.
   */

   if ((lcb_ptr->status & DMQCS_LCBS_RECONNECT_TRIG) == 0)
       timer_no = dmqcs_timer_number(
    group_ptr,
    (char) DMQCS_TMRTYPE_FAILOVER,
    (char) 'W');

   delay = dmq_env_ptr->race_delay * 10;
      }
  }

  if (lcb_ptr->status & DMQCS_LCBS_RELOAD_OBOUND)
  {
      DMQCS_DBG_INLINE("This is an OUTBOUND RELOAD")
      lcb_ptr->status &= (~DMQCS_LCBS_RELOAD_OBOUND);
      lcb_ptr->status |= DMQCS_LCBS_REMOVE_LINK;
      delay = dmq_env_ptr->race_delay * 10;
  }

  break;


     case 'L' :
  DMQCS_DBG_INLINE("Inbound went down")
  lcb_ptr->status |= (DMQCS_LCBS_REMOVE_LINK |
        DMQCS_LCBS_SUPPRESS_RECONNECT);
  break;


     case 'X' :
  DMQCS_DBG_INLINE("Listener went down -- Scanning group table")

  /*
  **  We have to cleanup all timers, and any groups we
  **  own that there are no LCBs outstanding.
  */

  tmp_group_ptr = dmq_grp_ptr;

  while ( (++local_index <= dmq_env_ptr->number_of_groups) &&
      (tmp_group_no != -1) )/* Loop through whole group table */
  {
      ++tmp_group_ptr;
      tmp_group_no    = tmp_group_ptr->group_number;
      queue_ptr     = (long *) &tmp_group_ptr->queue;

      if ((tmp_group_no > 0) &&
   (*queue_ptr == ldstruct.our_queue))
      {
   dmqcs_cancel_timers((LCB *)0, tmp_group_ptr, -1L,
       (char) 'R');
   dmqcs_cancel_timers((LCB *)0, tmp_group_ptr, -1L,
       (char) 'W');
   /* Cancel ANY timers outstanding */

   if ( (tmp_group_ptr->cur_tcb.lcbr_ptr != (char *)0)
   || (tmp_group_ptr->cur_tcb.lcbw_ptr != (char *)0))
   {
       xlcb_ptr = (LCB *) tmp_group_ptr->cur_tcb.lcbr_ptr;

       if (xlcb_ptr)
    if ((xlcb_ptr->status &
        DMQCS_LCBS_SHUTDOWN_PEND) == 0)
    {
        xlcb_ptr->status |= give_up_mask;
        dmqcs_close_link(xlcb_ptr, YES);
    }

       xlcb_ptr = (LCB *) tmp_group_ptr->cur_tcb.lcbw_ptr;

       if (xlcb_ptr)
    if ((xlcb_ptr->status &
        DMQCS_LCBS_SHUTDOWN_PEND) == 0)
    {
        xlcb_ptr->status |= give_up_mask;
        dmqcs_close_link(xlcb_ptr, YES);
    }
   }
      }
  }

  /*
  **  External events must somehow be brought forward
  **  and closed out.
  */

  while (ldstruct.extern_event_list)
  {
      /*
      ** Take us off the extern list
      */

      DMQCS_DBG_INLINE("Eliminating external event")
      xlcb_ptr = ldstruct.extern_event_list;
      ldstruct.extern_event_list = (LCB *) xlcb_ptr->fptr;

      if (ldstruct.extern_event_list)
   ldstruct.extern_event_list->bptr = (char *)0;

      xlcb_ptr->status &= (~DMQCS_LCBS_EXTERN_WAIT);

      /*
      ** Put us on the used list
      */

      ldstruct.lcb_used_list->bptr = (char *) xlcb_ptr;
      xlcb_ptr->fptr     = (char *) ldstruct.lcb_used_list;
      ldstruct.lcb_used_list  = xlcb_ptr;

      xlcb_ptr->status |= give_up_mask;
      xlcb_ptr->status &= (~( DMQCS_LCBS_RELOAD_OBOUND  |
         DMQCS_LCBS_DROP_ALL_LINKS));

      /*
      ** Now, close the channel to eliminate the event
      */

      dmqcs_close_link(xlcb_ptr->chan, YES);
  }

  lcb_ptr->status |= DMQCS_LCBS_REMOVE_LINK;
  break;


     case 'R' :
  DMQCS_DBG_INLINE("Read went down")
  lcb_ptr->status |= (DMQCS_LCBS_REMOVE_LINK |
        DMQCS_LCBS_SUPPRESS_RECONNECT);
  break;
 }
    }


    /*
    ** Now we have to contend with the possibility that an external
    ** event may be outstanding for this link.
    */

    if ((notify_ptr != (char *)0) && ((sys_status & 1) != 0))
    {
 DMQCS_DBG_INLINE("External event encountered")

 lcb_ptr->status |=  give_up_mask;

 lcb_ptr->status &=  (~( DMQCS_LCBS_RECONNECT_TRIG   |
    DMQCS_LCBS_SUPPRESS_MSG     |
    DMQCS_LCBS_RELOAD_OBOUND));

 if (ctcb_ptr)
      ctcb_ptr->extern_ptr = (char *)0;

 switch (lcb_ptr->cur_state.id)
 {
     case 'L' :
     case 'R' :
  xlcb_ptr = (LCB *) group_ptr->cur_tcb.lcbw_ptr;
  break;

     case 'C' :
     case 'W' :
  xlcb_ptr = (LCB *) group_ptr->cur_tcb.lcbr_ptr;
  break;

     default:       /* This cannot happen */
  xlcb_ptr = (LCB *)0;
  break;
 }

 if (xlcb_ptr)
 {
     xlcb_ptr->status |= give_up_mask;
     xlcb_ptr->status &= (~( DMQCS_LCBS_RELOAD_OBOUND    |
     DMQCS_LCBS_RECONNECT_TRIG   |
     DMQCS_LCBS_SUPPRESS_MSG));
 }
    }


    /*
    ** Now that we have established the desired settings of the LCB
    ** status field, we will bring down the link accordingly
    */


    if (sys_status & 1)
    {
 old_lcb_status = lcb_ptr->status;
 old_lcb_ptr = lcb_ptr;

 if (lcb_ptr->status & DMQCS_LCBS_REMOVE_LINK)
 {
     sys_status = dmqcs_remove_lcb(&lcb_ptr);

     if (sys_status & 1)
     {
  if (old_lcb_ptr == ldstruct.main_lcb)
      ldstruct.main_lcb = (LCB *)0;

  if (old_lcb_ptr == ldstruct.listen_lcb)
      ldstruct.listen_lcb = (LCB *)0;
     }
     else
     {
  if (sys_status == DMQCS__BADPARAM)
  {
      PAMS_LOG("W",
   "Warning -- dmqcs_remove_lcb() returned DMQCS__BADPARAM",
   W_ERROR);
  }
  /* There seems to be a bug hiding somewhere. This is just */
  /* to help flush it out */

  sys_status = DMQCS__SUCCESS;
     }
 }
    }


    if ((ctcb_ptr != (TCB *) 0) && ((sys_status & 1) != 0) )
    {
 if ((ctcb_ptr->lcbw_ptr == (char *)0) &&
     (ctcb_ptr->lcbr_ptr == (char *)0))
 {
     /*
     ** If eliminating this link causes the group to have no
     ** links up, then we must update the group to reflect this,
     */

     group_ptr->link_state = DMQCS_GLS_NO_CONNECTION;

     if (display_msg)
     {
    if (*ctcb_ptr->system_addr)
      (void) sprintf(
       line_buf,
       "Connection for group %d (%s) to system %s (%s) is down",
       group_ptr->group_number,
       group_ptr->group_name,
       ctcb_ptr->system_name,
       ctcb_ptr->system_addr);
  else
      (void) sprintf(
       line_buf,
       "Connection for group %d (%s) to system %s is down",
       group_ptr->group_number,
       group_ptr->group_name,
       ctcb_ptr->system_name);

  PAMS_LOG("W", line_buf, W_ERROR);
     }
 }
 else
     group_ptr->link_state = DMQCS_GLS_CONNECT_PENDING; /* Set state */
    }


    /*
    ** If this was a valid link to a group, then let's deal with some
    ** final issues.
    */

    if (    ((sys_status & 1) != 0)        &&
     (ctcb_ptr != (TCB *)0)        &&
     (long_ptr != (long *)0) )
    {
 if ((old_lcb_status & DMQCS_LCBS_DROP_OWNERSHIP) != 0)
 {
     /*
     **  There are several ownership situations to deal with here:
     ** 1) This is the local group (ctcb_ptr = 0). We need
     **     do nothing in this case.
     ** 2) This is a link without a group (ctcb_ptr = 0). Again,
     **     we do nothing.
     ** 3) This is an unsolicited group. In this case, we ONLY
     **     affect the cur_tcb.
     ** 4) This is a "full" group. We need to deal with the
     **     cur_tcb and the other tcb (if there).
     ** 5) If items 3 or 4 are being processed, AND this group
     **     now is empty of links, then free the write pool
     **     (if any).
     ** 6) We may have an external event to process
     */

     if ((*long_ptr == ldstruct.our_queue)   &&
  (ctcb_ptr->lcbr_ptr == (char *)0)   &&
  (ctcb_ptr->lcbw_ptr == (char *)0))
     {
  if (tcb_ptr == (TCB *)0)
      (void) dmqcs_free_group(group_ptr, NO);
      /* Just drop ownership */
  else
  {
      if (ldstruct.flags & DMQCS_LDFLAG_PROGRAM_SHUTDOWN)
   (void) dmqcs_free_group(group_ptr, YES);
   /* drop ownership and buffers */
      else
   (void) dmqcs_free_group(group_ptr, NO);
   /* Just drop ownership */
  }
     }

     if (notify_ptr)
  dmqcs_send_xtern_accept(notify_ptr);
 }


 /*
 **  If this is a joint-transport, and all conditions look good for
 **  transferring this link to the commserver, then try to do so. If
 **  it works, then suppress the reconenct timer because we aren't
 **  responsible anymore.
 */

 if (((ldstruct.flags & DMQCS_LDFLAG_JOINT_TRANSPORT) != 0) &&
     ((old_lcb_status & DMQCS_LCBS_XFER_TO_CS) != 0))
 {
     /*
     **  This can only occur on a shutdown, or a "one-link" situation.
     **  A normal link up/down would never set this XFER bit.
     */

     if ((*long_ptr == 0) &&
  (dmqcs_find_tcb(group_ptr, DMQCS_DECNET_TRANSPORT,
      (long *)0, (long *)0) != (TCB *)0))
     {
  sys_status = _dmqcs_xfer_group_to_cs(group_ptr);

  if (sys_status & 1)
      old_lcb_status |= DMQCS_LCBS_SUPPRESS_RECONNECT;
  else
  {
      sys_status = DMQCS__SUCCESS;
      group_ptr->group_status |= DMQCS_GS_NO_CS_XFER;
      /* Disable future xfer attempts */
  }
     }
 }
    }


    if (    ((sys_status & 1) != 0)        &&
     (ldstruct.lcb_used_list == (LCB *)0)      &&
     (ldstruct.main_lcb == (LCB *)0)       &&
     (ldstruct.main_chan == 0)        &&
     (ldstruct.oob_chan == 0)        &&
     (ldstruct.extern_event_list == (LCB *)0)      &&
     ((ldstruct.flags & DMQCS_LDFLAG_LISTENER_DOWN) != 0) )
    {
 /*
 **  If the network came down, and all the conditions are right, then
 **  we must free all our ownerships, tell the commserver we are down,
 **  and either terminate or set things up to requeue the listener.
 */

        if (ldstruct.flags & DMQCS_LDFLAG_PROGRAM_SHUTDOWN)
            dmqcs_free_all_groups(YES);    /* Free and clear pools */
        else
            dmqcs_free_all_groups(NO);     /* Free, but keep pools */

 ld_up_msg.transport_type    = ldstruct.transport;
 ld_up_msg.tcb_index     = ldstruct.tcb_index;

 (void) dmqcs_send_ld_msg(   PAMS_COM_SERVER,
        MSG_TYPE_LD_LINKDRIVER_DOWN,
        0L,
        ldstruct.our_group_no,
        DMQCS__SUCCESS,
        (char *) &ld_up_msg,
        sizeof(ld_up_msg));


 if (ldstruct.flags & DMQCS_LDFLAG_JOINT_TRANSPORT)
 {
     /*
     ** This transport works closely with the CommServer, so we
     ** have to tell the CommServer to connect to all groups that
     ** we may have "orphaned".
     */

     DMQCS_DBG_INLINE("Scanning group table...")
     local_index     = 0;
     tmp_group_no    = 0;
     tmp_group_ptr   = dmq_grp_ptr;

     while ( (++local_index <= dmq_env_ptr->number_of_groups) &&
  (tmp_group_no != -1) )
     {
  /* Loop through whole group table */

  ++tmp_group_ptr;
  tmp_group_no = tmp_group_ptr->group_number;

  if (tmp_group_no != -1)
  {
      if (group_ptr->stl[0] == DMQCS_DECNET_TRANSPORT)
   (void) _dmqcs_xfer_group_to_cs(tmp_group_ptr);
  }
     }
 }


 if (ldstruct.flags & DMQCS_LDFLAG_PROGRAM_SHUTDOWN)
 {
     /*
     ** This is a complete program shutdown, so all we need to
     ** to is exit.
     */

     PAMS_LOG("W","Link Driver has terminated", W_ERROR);

     if (ldstruct.tcb_ptr)
  ldstruct.tcb_ptr->tcb_status &= (~DMQCS_TCB_RUNNING);
     exit(0);
 }
 else
 {
     PAMS_LOG("W",
  "All links are down - awaiting network return", W_ERROR);
     old_lcb_status &= (~DMQCS_LCBS_SUPPRESS_RECONNECT);
     delay = dmq_env_ptr->def_listen_delay;
     timer_no = dmqcs_timer_number(  ldstruct.group_ptr,
         (char) DMQCS_TMRTYPE_LISTEN_DELAY,
         'W');
 }
    }


    if (sys_status & 1)
    {
 if ( (timer_no   != 0)     &&
  (delay     != 0)     &&
  ((old_lcb_status & DMQCS_LCBS_SUPPRESS_RECONNECT) == 0))
 {
     /*
     **  We are to set up a retry timer and re-do the link
     */

     if (dmqcs_debug)
  (void) printf("%s Declaring reconnect timer %u of %d uSecs\n",
      DMQCS_DBG_PREFIX, timer_no, delay);

     sys_status = pams_set_timer(&timer_no, &timer_type, &delay, qword);
     PAMS_SET_TIMER_PATCH;

     if ((sys_status & 1) == 0)
     {
  (void) sprintf(line_buf,
      "Bad PAMS_SET_TIMER return status for X-Group connect to group %d", 
      group_ptr->group_number);
  PAMS_LOG("W", line_buf, sys_status);
     }
 }
    }
}

/*
**    MAIN PROGRAM
**
**
**  External References:
** _MOVC5()
** _MOVC3()
** dmqcs_pams_wait()
** dmqcs_tp_init()
** dmqcs_dealloc_es()
** dmqcs_dealloc_iostr()
** sprintf()
** printf()
** dmqcs_set_debug();
** dmqcs_map_gsect();
** dmqcs_signal();
** dmqcs_route_msg();
** dmqcs_move_iostr();
** dmqcs_queue_link_event();
** dmqcs_dequeue_iostr_event();
** dmqcs_alloc_write_buffer();
** dmqcs_free_write_buffer();
** dmqcs_ld_take_uma();
** dmqcs_cnv_protocol();
** dmqcs_queue_error();
** strncpy()
** dmqcs_tod()
** dmqcs_ld_update_stats()
** dmqcs_send_ld_msg()
** pams_wake()
** dmqcs_error_exit()
** dmqcs_complete_wping()
** dmqcs_tp_oob_read()
** *dmqcs_find_tcb()
** dmqcs_dequeue_link_event()
** dmqcs_ld_update_oob_stats()
*/

#ifdef vms
static
#endif
main(argc, argv)

int argc;
char **argv;
{
    struct msg_hdr  *pams_hdr_ptr;
    long     sys_status;
    long     queue_name_len;
    long     *long_ptr;      /* longword pointer for misc uses */
    char     timer_buf[24];     /* Small buffer for timer receive */
    char     msg_priority;
    IOSTR     *iostr_ptr;      /* For other event lists */
    LCB      *lcb_ptr;      /* local lcb ptr for misc use */
    LCB      *xlcb_ptr;      /* spare lcb ptr for misc use */
    GROUP     *group_ptr;      /* local group ptr for misc use */
    int      master_loop = YES;
    long     movc5_tmp = 0;     /* For _MOVC5() calls */
    char     *pams_msg_ptr;
    char     pams_priority;
    pams_address    pams_source;
    short     pams_class;
    short     pams_type;
    unsigned short  pams_len;
    char     pams_journal;
    pams_address    local_target;
    unsigned char   xgroup_msg;
    long     tmp_status;
    int      do_pams_free;
    char     uma_string[132];
    int      route_it;
    long     pool_size;
    int      close_link;
    short     response_type;
    LD_UP_STRUCT    ld_up_msg;
    TCB      *tcb_ptr;
    int      data_write;
    int      do_loop;


    struct
    {
 short word0;
 short word1;
    } timer_filter;


    void     dmqcs_error_exit();
    long     dmqcs_pams_wait();
    long     dmqcs_tp_init();
    void     dmqcs_dealloc_es();
    long     dmqcs_dealloc_iostr();
    void     dmqcs_set_debug();
    long     dmqcs_map_gsect();
    long     dmqcs_signal();
    long     dmqcs_route_msg();
    void     dmqcs_move_iostr();
    void     dmqcs_queue_link_event();
    IOSTR     *dmqcs_dequeue_iostr_event();
    long     dmqcs_alloc_write_buffer();
    long     dmqcs_free_write_buffer();
    long     dmqcs_ld_take_uma();
    long     dmqcs_cnv_protocol();
    void     dmqcs_queue_error();
    void     dmqcs_tp_R_DATAIO();
    void     pams_c_error_log();
    void     dmqcs_tod();
    long     dmqcs_ld_update_stats();
    long     dmqcs_send_ld_msg();
    long     pams_wake();
    long     pams_display_header();
    long     dmqcs_tp_oob_read();
    TCB      *dmqcs_find_tcb();
    EVENT_STRUCT    *dmqcs_dequeue_link_event();
    long     dmqcs_ld_update_oob_stats();


    _MOVC5( (unsigned short) 0,
  (const char *) 0,
  (char) 0,
  (unsigned short) sizeof(ldstruct),
  (char *) &ldstruct,
  (unsigned short *) &movc5_tmp);

    ldstruct.flags  = DMQCS_LDFLAG_LISTENER_DOWN;
    ldstruct.lcb_seq_no  = 0;
    ldstruct.es_free_list = &ldstruct.es_free_head;
    ldstruct.link_event_start = &ldstruct.link_event_head;
    /* Initialize ldstruct region */

    (void) strncpy(my_process_name, ldstruct.queue_name, MAX_QUEUE_NAME_LEN);
    (void) strncpy(user_reference_area.pams_my_bus_id, "xxxx-xxxxx", 10);
    user_reference_area.pams_my_process_name[MAX_QUEUE_NAME_LEN - 1] = 0;
    /* Setup for performing pams_c_error_log() calls */

    dmqcs_tod(time_buf);
    (void) sprintf(line_buf, "%s (%c%d.%d-%d%d) started at %s",
 ldstruct.transport_name, 'V', 2, 1, 0, 2, time_buf);
    PAMS_LOG("I", line_buf, I_ERROR);

    if (((sys_status = dmqcs_map_gsect()) & 1) == 0)
    {
 PAMS_LOG("F",
     "Failed to map global section -- program exiting", sys_status);
 exit(0);
    }

    dmqcs_set_debug();   /* Set debug if logical exists */

    if (dmq_env_ptr->xgroup_enabled == 0)
    {
 PAMS_LOG("F", "XGROUP not enabled - program exiting", F_ERROR);
 exit(0);
    }

    pams_map_sec_u  = 0;
    pams_group_section_access = 1;
    queue_name_len  = (long) strlen(ldstruct.queue_name);

    sys_status = pams_attach_q( &attach_mode,
    (PAMS_ADDRESS *) &queue_number.all,
    &queue_type,
    ldstruct.queue_name,
    &queue_name_len,
    0, 0, 0, 0, 0); 

    if (sys_status & 1)
    {
 pams_server_process     = 1;
 long_queue_addr      = (long *) &queue_number.all;
 pams_override_suppression   = YES;

 (void) strncpy( user_reference_area.pams_my_process_name,
   ldstruct.queue_name,
   queue_name_len);

 if (dmqcs_debug)
     (void) printf("%sAttached to bus %d as queue %d.%d\n",
     DMQCS_DBG_PREFIX,
     dmq_env_ptr->bus_id,
     queue_number.au.group,
     queue_number.au.queue);
    }
    else
    {
 if (sys_status != PAMS__DECLARED)
     dmqcs_error_exit(sys_status, "PAMS_ATTACH_Q failed");
 else
 {
     PAMS_LOG("E", 
  "This linkdriver is already running -- exiting",
  sys_status);

     exit(0);
 }
    }

    ldstruct.group_ptr  = (GROUP *) dmq_grp_ptr;
    ldstruct.proto_local[0] = PAMS_XGRP_HDR_VERSION / 10;
    ldstruct.proto_local[1] = PAMS_XGRP_HDR_VERSION % 10;
    ldstruct.our_byte_order = DMQCS_VAX_BYTE_ORDER;
    ldstruct.our_queue  = *long_queue_addr;
    ldstruct.cs_tcb  = dmqcs_find_tcb(
 ldstruct.group_ptr, DMQCS_DECNET_TRANSPORT, 0, 0);
    /* Initialize our part of the ld_struct structure */
    /* Transport init  routine  will fill in the rest */

    if (((sys_status = dmqcs_tp_init()) & 1) == 0)
    {
 PAMS_LOG("F", "Transport initialization failed - cannot continue execution",
     F_ERROR);
 (void) pams_exit();
 exit(1);
 /* tp_init prints the reasons for its failures, so we don't have to */
    }

    if (strlen(ldstruct.system_address))
 (void) sprintf(line_buf, "Local system is %s (%s), transport addr %s",
     ldstruct.system_name,
     ldstruct.system_address,
     ldstruct.nt_ptr->transport_addr);
    else
 (void) sprintf(line_buf, "Local system is %s, transport addr %s",
     ldstruct.system_name,
     ldstruct.nt_ptr->transport_addr);

    PAMS_LOG("I", line_buf, I_ERROR);


    /*
    ** At this point, the ldstruct is completely filled in,
    ** and our tcb has been corrected.
    **
    **
    **
    **   Start listening process
    */

    if (((sys_status = dmqcs_start_listening()) & 1) == 0)
    {
 if (sys_status != DMQCS__INUSE) 
 {
     PAMS_LOG("I", "Failed to start network listener", sys_status);
 }

 response_type = MSG_TYPE_LD_LINKDRIVER_DOWN;
    }
    else
    {
 ldstruct.tcb_ptr->tcb_status |= DMQCS_TCB_RUNNING;
 response_type   = MSG_TYPE_LD_LINKDRIVER_UP;
    }


    ld_up_msg.transport_type = ldstruct.transport;
    ld_up_msg.tcb_index  = ldstruct.tcb_index;

    (void) dmqcs_send_ld_msg( PAMS_COM_SERVER,
    response_type,
    0L,
    ldstruct.our_group_no,
    sys_status,
    (char *) &ld_up_msg,
    sizeof(ld_up_msg));

    if ((sys_status & 1) == 0)
    {
 (void) pams_exit();
 exit(0);
    }


    PAMS_LOG("I", "Startup completed", I_ERROR);


    /*
    **
    **    MASTER LOOP
    **
    **
    */


    while (master_loop)
    {
 /*
 **   WAIT FOR EVENT
 */

 dmqcs_error_exit(DMQCS__SUCCESS, (char *)0);

 DMQCS_DBG_ENTRY("[[[ Waiting for event ]]]")

 if (((sys_status = dmqcs_pams_wait()) & 1) == 0)
     dmqcs_queue_error( sys_status,
    "MAIN: Event wait",
    "dmqcs_pams_wait() failed",
    (char *)0,
    0);

 /*
 ** We now wait for something to wake us up. When we wake, we
 ** check the following items (in order):
 **
 **     Stray error list (fatal errors, usually in ASTs)
 **     PAMS messages (including timers)
 **
 **     All Link state changes
 **     Stray error list again
 **
 **     All Write completions
 **     Stray error list again
 **
 **	    Read completions
	**	    Stray error list again
	**
	**	    OOB read completions
	**	    Stray error list again
	**
	*/

	dmqcs_error_exit(sys_status, "after event wakeup");


 [End of sample #1]


---------------- Attachment #2 -------------------
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




