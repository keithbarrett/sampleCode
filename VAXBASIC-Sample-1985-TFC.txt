10	%TITLE "MH2MHQM - Mini-Hub Queue Manager"
	%SBTTL	"Module Heading and Modification History"
	%IDENT	"02.02"

	!		Module File Name:	MH2MHQM.BAS
	!		Module Title	:	Mini-Hub MHQM
	!		Version		:	02
	!		Edit		:	02
	!		Creation Date	:	09-Jan-1985
	!


	! MODIFICATION HISTORY:

	! Version	Date				Reason

	! V01-01	15-Mar-85	KGB	Added MODEM BREAK for LDU
	!					types B and C



	%PAGE
	%SBTTL	"Module Description"


	!		M O D U L E   D E S C R I P T I O N


	! FUNCTIONAL DESCRIPTION:

	!	This is  the  Mini-Hub Queue Manager, it runs 'in the back-
	! ground' until told to shutdown.

	!	This program needs DETACH and SYSPRV priviledges on VMS.

	%PAGE



	!	This  is  a  description  of  the  methods  involved in sending
	! information  to  a  User-program.  The  are  2  reasons  to talk to a
	! user,  1) To tell him/her that  the  requested  transaction  has been
	! accepted or rejected,   2) To  send the  data  returned  by  an  LDU.
	!
	!	When relaying  LDU data to a user, the following must be
	! Done:		1) Move the ID from the MHQ into the RESPONSE-ID
	!		2) Set the RESPONSE EFN (should already be free)
	!		3) delete the MHQ/SSL entries
	!
	!	When  sending  other  information  to  the  user, the following
	! must be done:
	!		1) Check WAKEUP EFN, if set then
	!			a) Wait 1 sec and try again
	!			b) if this happens x times, clear the EFN
	!		2) Move ID from the MHQ into the WAKEUP-ID
	!		3) Move an MH2.ERR_nnn into USER_STATUS
	!		4) Set Wakeup EFN
	!
	!
	!	NOTE:	Only Shutdown, On-Line, and debug changes function
	!		when the MHQM is off-line.




	%PAGE
	%SBTTL	"Program Environment"


	! SIDE EFFECTS:

	! %VARIANT = 0% is for VMS    Operating System
	! %VARIANT = 1% is for RSX    Operating system
	! %VARIANT = 2% is for RSTS/E Opersting System



	!		P R O G R A M   E N V I R O N M E N T


	! OPTIONS:

	OPTION	TYPE=EXPLICIT

	%IF %VARIANT = 0% %THEN
		OPTION	SIZE=INTEGER LONG		! VMS
	%ELSE
		OPTION	SIZE=INTEGER WORD		! NON-VMS
	%END %IF



	! INCLUDE FILES:

	! MH2MAP.INC			in GLOBAL STORAGE section
	!	MHQMAP.INC		in Global storage section
	!	QOLMAP.INC		in Global storage section
	!	MH2SYMB.INC		in Local  storage section
	!	MH2ERRCON.INC 		in Local  storage section
	! %IF %VARIANT = 0%
	!	"MH2LIB:VMSEXCON.INC"	in EXTERNAL section
	! %IF %VARIANT = 1%
	!	"MH2LIB:RSXEXCON.INC"	In EXTERNAL section



	! LOCAL STORAGE:


	DIM #2%, STRING MHQ(32767%) = 256%		 ! MHQ

	%IF %VARIANT = 0% %THEN				 ! VMS
		%INCLUDE	"MH2LIB:MH2SYM.INC"	 ! MH2 Symbols
		%INCLUDE	"MH2LIB:MH2ERRCON.INC"	 ! MH2 Err constants

		DECLARE	STRING	CONSTANT TT = "TT:"	! Own terminal
		DECLARE	LONG	CONSTANT SECONDS = -10%*1000%*1000%
	%ELSE						! RSX
		%INCLUDE	"SR:[100,377]MH2SYM.INC"
		%INCLUDE	"SR:[100,377]MH2ERRCON.INC"

		DECLARE	STRING	CONSTANT TT = "TI:"	! Own terminal
	%END %IF

	DECLARE WORD	CONSTANT NO		=  0%
	DECLARE	WORD	CONSTANT YES		= -1%
	DECLARE	WORD	CONSTANT EDIT_ARG	= 103%	! EDIT$(,1+2+4+32+64)

	DECLARE	STRING	CONSTANT NULL_STRING = ""


	DECLARE	WORD		 DIE_ON_ERROR		! Trigger
	DECLARE	WORD		 I			! Indexing
	DECLARE	WORD		 TMP			! Temporary uses
	DECLARE	WORD		 TMP_ERR		! Temporary uses
	DECLARE	WORD		 CLEANUP_FLAG		! In Cleanup routine?
	DECLARE	WORD		 START_FLAG		! Start flag
	DECLARE	WORD		 QOL_REC_SIZE		! QOL recordsize
	DECLARE	WORD		 REQUE			! Requeue MHQ entry?
	DECLARE	WORD		 CALL_SLR		! Call Standard Log?
	DECLARE	WORD		 WAIT_FOR_RTS		! Another Flag
	DECLARE	WORD		 PROCESS_STATUS		!   "    "    "

	DECLARE	INTEGER		 SYS_STATUS		! For EXEC Calls
	DECLARE	INTEGER		 SUCCESS		! For EXEC Success
	DECLARE	INTEGER		 EFN_FLAGS		! Event Flag State
	DECLARE	INTEGER		 EFN_WASSET		! Was Set
	DECLARE	INTEGER		 EFN_WASCLR		! Was clr
	DECLARE	INTEGER		 EFN_EXISTS		! Was there
	DECLARE	INTEGER		 TIMEOUT		! Timeout error
	DECLARE	INTEGER		 MHQ_SIZE_LIMIT		! MHQM limit
	DECLARE	INTEGER		 TMPL			! Temporary uses

	DECLARE	STRING		 ERR_TEXT		! Additional Err Text
	DECLARE	STRING		 TMP_STR
	DECLARE	STRING		 PROCESS_ID		! For subroutines
	DECLARE	STRING		 PROCESS_USER_ID	! "    "   "


	! GLOBAL STORAGE:

			MAP	(MH2USR)				&
									&
	  WORD		TFC_DEBUG			! DEBUG flag	&
	, WORD		TEMP						&
	, LONG		TIMER_VALUE			! Timer value	&
	, LONG		TIMER_VALUE2



			MAP	(MH2USR)				&
									&
	  WORD		FILL						&
	, STRING	TEMP1 = 1%					&
	, STRING	TEMP2 = 2%


	%IF %VARIANT = 0% %THEN				! VMS
		%INCLUDE	"MH2LIB:MH2MAP.INC"	! MH2 Global PSECT
		%INCLUDE	"MH2LIB:MHQMAP.INC"	! MHQ
		%INCLUDE	"MH2LIB:QOLMAP.INC"	! QOL
	%ELSE						! RSX
		%INCLUDE	"SR:[100,377]MH2MAP.INC"
		%INCLUDE	"SR:[100,377]MHQMAP.INC"
		%INCLUDE	"SR:[100,377]QOLMAP.INC"
	%END %IF



	! EXTERNAL REFERENCES:

	!	Current subroutine tree:

	!			      MH2MHQM
	!			         |
	!  .------.-------.-------.------+-----.-------.-------.---------.
	! FNT   CNVDAT  CNVTIM  RESEFN   |  GETSYM  BITOPS  FFCBAS  (RSX: RDXF)
	!		                 |
	!		.-----------.----+----.----------.
	!	     EXCERR	 QOLSTO  |  QOLPOP    CLSMHQ
	!				 |
	!     .------------.-------------+--------.---------.----------.
	!   INMHQM      MQMOLR        STDLOG    MQMITR    MQMELR     MH2QIO
	!     |		   |             |        |         |          |
	!   INTBL   .------+----.	FNT    .------.     |          |
	!	  RESEFN  FNT QOLSTO         RDXF    FNT   FNT   PRINTIT/PRNTIT




	EXTERNAL		SUB	 BITOPS(STRING BY REF,		&
						INTEGER BY VALUE,	&
						INTEGER BY VALUE,	&
						INTEGER BY REF)
	! Bit testing/clearing/setting for SSL

	EXTERNAL		SUB	FFCBAS(	STRING BY REF,		&
						INTEGER BY VALUE,	&
						INTEGER BY REF)
	! Find first clear bit and set

	EXTERNAL	WORD	FUNCTION CNVDAT(STRING, STRING, WORD)
	! Convert dates

	EXTERNAL	WORD	FUNCTION CNVTIM(STRING, STRING, WORD)
	! Convert time

	EXTERNAL	STRING	FUNCTION FNT		! Time in HH:MM:SS

	EXTERNAL		SUB	 INMHQM		! INIT MHQM memory

	EXTERNAL		SUB	 RESEFN(WORD, WORD)
	! Reserve EFN

	EXTERNAL		SUB	 CLSMHQ
	! Close MHQ

	EXTERNAL		SUB	 MQMOLR(INTEGER)
	! MHQM On-Line requests

	EXTERNAL		SUB	 MQMITR(INTEGER, STRING)
	! MHQM Introduction

	EXTERNAL		SUB	 MQMELR
	! MHQM Enable Logging requested

	EXTERNAL		SUB	 STDLOG(STRING)
	! Standard logging routine

	EXTERNAL	STRING	FUNCTION GETSYM(WORD)
	! Returns Error symbol for Logging

	EXTERNAL		SUB	EXCERR(INTEGER, STRING)
	! Handle exec errors

	EXTERNAL		SUB	QOLPOP(INTEGER)
	! POP from QOL

	EXTERNAL		SUB	QOLSTO(INTEGER)
	! Store into QOL

	%IF %VARIANT = 0% %THEN				! VMS
		EXTERNAL	LONG	FUNCTION	SYS$ASCEFC
		! Assign Common Event Flag Cluster

		EXTERNAL	LONG	FUNCTION	SYS$READEF
		! Read Event flags

		EXTERNAL	LONG	FUNCTION	SYS$SETEF
		! Set Event Flag

		EXTERNAL	LONG	FUNCTION	SYS$CLREF
		! Clear Event Flag

		EXTERNAL	LONG	FUNCTION	SYS$WAITFR
		! Wait for event Flag

		EXTERNAL	LONG	FUNCTION	SYS$WFLOR
		! Wait for OR of event Flags

		EXTERNAL	LONG	FUNCTION	SYS$SETIMR
		! Timer

		EXTERNAL	LONG	FUNCTION	SYS$CANTIM
		! Cancel Timer

		%INCLUDE	"MH2LIB:VMSEXCON.INC"
		! VMS external constants
	%ELSE						! RSX
		EXTERNAL	WORD	FUNCTION	CRGF
		! Common Event Flags

		EXTERNAL		SUB		RDXF
		! Read Event flags

		EXTERNAL	WORD	FUNCTION	SETEF
		! Set Event Flag

		EXTERNAL	WORD	FUNCTION	CLREF
		! Clear Event Flag

		EXTERNAL	WORD	FUNCTION	WAITFR
		! Wait for event Flag

		EXTERNAL	WORD	FUNCTION	WFLOR
		! Wait for OR of event Flags

		EXTERNAL	WORD	FUNCTION	MARK
		! Timer

		EXTERNAL	WORD	FUNCTION	CANMT
		! Cancel Timer

		%INCLUDE	"SR:[100,377]RSXEXCON.INC"
		! RSX External Constants
	%END %IF



	%PAGE
	%SBTTL	"Main Program Code"

1000	!
	!
	!		M A I N   L O G I C
	!
	!

 TEST_DEBUG:

	ON ERROR GOTO DEBUG_ERR_TRAP
	TFC_DEBUG = NO
	OPEN "TF7:" FOR INPUT AS FILE 12%, RECORDSIZE 1%
	CLOSE 12%
	TFC_DEBUG = YES
	! Is DEBUG to be on?


 DEBUG_ERR_TRAP:

	RESUME 1010
	! No DEBUG


 PROGRAM_START:

1010	ON ERROR GOTO ERROR_ROUTINE
	! Default error MHQM



 LOCAL_CONSTANTS:

	ERR_TEXT	= NULL_STRING
	DIE_ON_ERROR	= NO
	START_FLAG	= YES
	TIMER_VALUE2	= -1%
	QOL_REC_SIZE	= LEN(QOL_REC)
	WAIT_FOR_RTS	= YES
	! Init variables

	%IF %VARIANT = 0% %THEN					! VMS
		SUCCESS		= SS$_NORMAL
		EFN_WASSET	= SS$_WASSET
		EFN_WASCLR	= SS$_WASCLR
		EFN_EXISTS	= SS$_NORMAL
		TIMEOUT		= SS$_TIMEOUT
	%ELSE
		SUCCESS		= IS.SUC			! RSX
		EFN_WASSET 	= IS.SET
		EFN_WASCLR 	= IS.CLR
		EFN_EXISTS	= IE.RSU
		TIMEOUT		= IS.TMO
	%END %IF



 MAIN_LOGIC:

	CALL	MQMITR(SYS_STATUS, ERR_TEXT)
	! introduction to MHQM (display ID, attach to EFNs, etc.)

	GOTO EXEC_ERROR UNLESS SYS_STATUS = SUCCESS
	! Error

	MHQ_SIZE_LIMIT = LEN(MH2_SSL) * 8%
	! Size of MHQ




	!		W A I T   F O R   E V E N T



 EFN_WAIT:

	CALL_SLR = TFC_DEBUG OR ((MH2_MHQM_STATUS AND			&
		LOGGING_STATUS) <> 0%)
	! Set CALL Standard Logging routine flag

	GOSUB SEND_TO_SYMBIONT						&
		IF (MH2_CURRENT_MHQ_REC = 0%) AND			&
			(MH2_MHQM_QOL_SIZE <> 0%)			&
			AND (MH2_MHQM_STATUS AND ONLINE_STATUS)
	! Send message to symbiont if necessary and able

	MH2_REQUEST_PROCESSED = NO
	! Clear flag


 EFN_WAIT2:

	ERR_TEXT = "Setting TIMER"
	! We are about to set our USER/MHQM wakeup timer

	%IF %VARIANT = 0% %THEN				! VMS
		TIMER_VALUE = SECONDS * MH2_MHQM_TIMER
		! MH2_MHQM_TIMER is in shared memory, and may change it's
		! value at anytime because of the 'outside' world

		TMP = MHQM_WAKEUP_EFN OR 				&
			MHQM_SYMBIONT_WAKEUP_EFN OR MHQM_TIMER_EFN
		TMP = TMP OR USER_RTS_EFN IF WAIT_FOR_RTS
		! Set bits for WFLOR

		SYS_STATUS = SYS$SETIMR(MHQM_TIMER_EFN_NO BY VALUE	&
		,	TIMER_VALUE BY REF,,)
		! Set timer
	%ELSE						! RSX
		CALL MARK(MHQM_TIMER_EFN_NO, MH2_MHQM_TIMER, 2%,	&
			SYS_STATUS)
		! Set timer
	%END %IF

	GOTO EXEC_ERROR UNLESS SYS_STATUS = SUCCESS
	! error

	MH2_MHQM_STATUS = MH2_MHQM_STATUS AND NOT BUSY_STATUS
	! Turn off busy

	ERR_TEXT = "Waiting for Next Event"
	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$WFLOR(MH2_EFN_CLUSTER BY VALUE,	&
			TMP BY VALUE)	
		! Wait for the bits mapped in TMP
	%ELSE						! RSX
		IF WAIT_FOR_RTS THEN
			CALL WFLOR(	MHQM_WAKEUP_EFN_NO,		&
					MHQM_SYMBIONT_WAKEUP_EFN_NO,	&
					MHQM_TIMER_EFN_NO,		&
					USER_RTS_EFN_NO)
		! wait for any EFN
		ELSE
			CALL WFLOR(	MHQM_WAKEUP_EFN_NO,		&
					MHQM_SYMBIONT_WAKEUP_EFN_NO,	&
					MHQM_TIMER_EFN_NO)
		! wait for any EFN except RTS
		END IF
	%END %IF
	! Wait for the event to happen



	!			W A K E U P



	MH2_MHQM_STATUS = MH2_MHQM_STATUS OR BUSY_STATUS
	! Set busy

	GOSUB READ_EFN
	! Read EFNs before canceling timer and clearing EFNs



	!		C A N C E L   T I M E R   A N D   E F N



	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$CANTIM(0% BY VALUE,)
	%ELSE						! RSX
		CALL CANMT(MHQM_TIMER_EFN_NO, SYS_STATUS)
	%END %IF
	! Cancel Timer

	IF EFN_FLAGS AND MHQM_TIMER_EFN THEN
		SLEEP 1%
		! To give Users time to timeout

		%IF %VARIANT = 0% %THEN
			SYS_STATUS = SYS$CLREF(MHQM_TIMER_EFN_NO BY VALUE)
		%ELSE
			CALL CLREF(MHQM_TIMER_EFN_NO, SYS_STATUS)
		%END %IF
		! Clear timer EFN
	END IF



	!	S E T / C L E A R   D E B U G   S T A T U S



	TMP = TFC_DEBUG
	TFC_DEBUG = YES * SGN(MH2_MHQM_STATUS AND DEBUG_STATUS)
	! Establish debug condition

	IF TMP <> TFC_DEBUG THEN
		ERR_TEXT = "*** Turning " + MID("ON OFF",		&
			(TFC_DEBUG + 1%) * 3% + 1%, 3%) + " DEBUG "
		PRINT "%DEBUG: " + ERR_TEXT + "at ";FNT;" ***"
		PRINT #1%,CR + LF;FNT;"  ";ERR_TEXT;"***" IF BUFSIZ(1%)
	END IF
	! Log DEBUG change (if any)

	PRINT CR + LF + "%DEBUG: Wakeup at ";FNT IF TFC_DEBUG
	! They're Hereeeeeee

	PRINT #1% IF BUFSIZ(1%) > 0% AND EFN_FLAGS AND			&
			(MHQM_WAKEUP_EFN OR MHQM_SYMBIONT_WAKEUP_EFN)
	! Skip a line in the log file for clarity



	!			U S E R   R T S



	IF WAIT_FOR_RTS = YES AND (EFN_FLAGS AND (MHQM_WAKEUP_EFN OR 	&
		MHQM_SYMBIONT_WAKEUP_EFN OR MHQM_TIMER_EFN		&
		OR USER_RTS_EFN)) = USER_RTS_EFN THEN

		WAIT_FOR_RTS = NO
		PRINT "%DEBUG: *** User RTS, resetting timer ***"	&
			IF TFC_DEBUG
		GOTO EFN_WAIT2
	END IF
	! An RTS only (when we check for it) means start timer over

	WAIT_FOR_RTS = YES UNLESS EFN_FLAGS AND USER_RTS_EFN
	! Timeout will cause this



	!		C L E A R   O U R   W A K E U P   E F N



	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$CLREF(MHQM_WAKEUP_EFN_NO BY VALUE)
	%ELSE						! RSX
		CALL CLREF(MHQM_WAKEUP_EFN_NO, SYS_STATUS)
	%END %IF
	! Clear Our wakeup EFN

	MH2_MHQM_STATUS2 = 0%
	MH2_MHQM_STATUS = MH2_MHQM_STATUS AND NOT (EXEC_ERROR OR BASIC_ERROR)
	! Clear some garbadge

	MH2_MHQM_REASONS = MH2_MHQM_REASONS OR MHQM_SCAN_MHQ_BIT	&
		IF EFN_FLAGS AND MHQM_TIMER_EFN
	! Timer expiration also means scan MHQ





	!		O N - L I N E   R E Q U E S T



	IF (MH2_MHQM_STATUS AND ONLINE_STATUS) AND (MH2_MHQ_OPEN = NO)	&
		THEN

		ERR_TEXT = "On-Line Request"

		CALL MQMOLR(SYS_STATUS)
	END IF
	! MHQM requested on-line. So Open MHQ, build QOL/SSL, etc.




	!	D I S A B L E / E N A B L E   L O G G I N G

	!		      R E Q U E S T



	IF MH2_MHQM_STATUS AND ONLINE_STATUS THEN

		ERR_TEXT = "Enable/Disable Logging"

		GOSUB DISABLE_LOGGING IF MH2_MHQM_REASONS AND		&
			DISABLE_LOGGING_BIT
		! Disable logging

		CALL MQMELR IF MH2_MHQM_REASONS AND ENABLE_LOGGING_BIT
		! Enable logging

	END IF
	! These commands are only executed if we are on-line

	CALL_SLR = TFC_DEBUG OR ((MH2_MHQM_STATUS AND			&
		LOGGING_STATUS) <> 0%)
	! Set CALL Standard Logging routine flag




	!		S H U T D O W N



	IF MH2_MHQM_REASONS AND SHUTDOWN_BIT THEN

		ERR_TEXT = "Shutdown"

		MH2_MHQM_REASONS = 0%
		! No more reasons allowed

		CALL STDLOG("*** Shutdown Requested ***") IF CALL_SLR
		! Log if necessary

		PROCESS_STATUS	= MH2.ERR_SHUTDOWN
		PROCESS_ID	= NULL_STRING
		GOSUB NOTIFY_USER
		! Tell users

		MH2_REQUEST_PROCESSED = YES
		! Clear reason

		MH2_MHQM_STATUS = MH2_MHQM_STATUS OR SHUTDOWN_STATUS
		! Set status

		GOTO CLEANUP
		! Final processing
	END IF




	!		R E C E I V E   F R O M   S Y M B I O N T



	IF EFN_FLAGS AND MHQM_SYMBIONT_WAKEUP_EFN THEN

		GOSUB RECEIVE
		! Set status, Log, and clear EFN

		IF MH2_HANDLER_STATUS AND (BASIC_ERROR OR EXEC_ERROR) THEN
			GOSUB SYMBIONT_DIED
			! Symbiont has sent us notice of his death
			! so me must forward it to the others

			IF MH2_CURRENT_MHQ_REC THEN

				GOSUB GET_MHQ_REC
				! Get MHQ record

				REQUE = YES IF MH2_CURRENT_MHQ_REC
				! Set re-queue flag
			END IF
		ELSE
			IF MH2_CURRENT_MHQ_REC = 0% THEN
			! Symbiont responded when we weren't expecting it

				IF TFC_DEBUG THEN
					PRINT "%DEBUG: Symbiont sent "	&
						;"response/err when no"	&
						;" MHQ Rec is inuse"

					PRINT "%DEBUG: Symbiont " +	&
						"ignored!"
				END IF
				! Log error
			ELSE
				GOSUB GET_MHQ_REC
				! Get MHQ record

				IF MH2_CURRENT_MHQ_REC = 0% THEN

					PRINT "%DEBUG: Response ";	&
						"ignored!" IF TFC_DEBUG
				ELSE
					TMPL = MH2_STATUSL
					TMPL = MH2.ERR_TIMEOUT		&
						IF TMPL = TIMEOUT
					TMPL = 0% IF TMPL = SUCCESS
					TMPL = MH2_STATUS2 UNLESS TMPL
					TMP_ERR = TMPL
					! Establish err condition of resp.

					IF TMP_ERR THEN
						GOSUB SYMBIONT_SENT_ERR
					ELSE
						GOSUB SYMBIONT_SENT_MESSAGE
					END IF
				END IF
			END IF
		END IF

		IF MH2_CURRENT_MHQ_REC THEN
		! If there was an MHQ record...

			IF REQUE AND (MHQ_FLAGS AND MHQ_NOWAIT_BIT)	&
				AND (TMP_ERR = MH2.ERR_LDUBUSY) THEN
			! And it needs to be re-qued and the user
			! isn't waiting for it and it was only an LDU busy

				CALL STDLOG(" Record will be sent " +	&
					"again") IF CALL_SLR
				! Log it
		
				MHQ_PRIORITY = -121%		! 135
				! Reduce priority to give others a chance

				MHQ_STATUS = MHQ_STATUS AND		&
					NOT MHQ_IN_QOL_STATUS
				! Clear 'IN QOL' status bit

				MHQ(MH2_CURRENT_MHQ_REC) = MHQ_REC
				! Write it back to MHQ

				! There use  to  be logic here which would add
				! the   record  back  into  the  QOL,  thereby
				! causing an immediate re-try.  It was removed
				! so that the Timer-expiration will re-send it
				! instead.
			ELSE
				PRINT "%DEBUG: Record will NOT be " +	&
					"re-sent" IF TFC_DEBUG AND TMP_ERR

				GOSUB DELETE_MHQ_ENTRY
				! Delete MHQ/SSL entry
			END IF
		END IF

		MH2_MHQM_STATUS = MH2_MHQM_STATUS AND NOT RECEIVE_STATUS
		MH2_CURRENT_MHQ_REC, MH2_CURRENT_MHQ_SEQ_NO = 0%
		! Clear status
	END IF




	!	M A N U A L L Y   F O R C E D   W A K E U P




	IF MH2_MHQM_REASONS AND FORCED_BIT THEN

		ERR_TEXT = "Forced Wakeup"

		MH2_MHQM_REASONS = MH2_MHQM_REASONS AND NOT FORCED_BIT
		! Clear reason

		MH2_REQUEST_PROCESSED = YES
		! Set flag

		IF CALL_SLR THEN
			ERR_TEXT = "*** Wakeup Manually Forced ***"
			ERR_TEXT = "*** MHQM Started ***" IF START_FLAG

			CALL STDLOG(ERR_TEXT) IF CALL_SLR

			MH2_REQUEST_PROCESSED = YES
		END IF

		START_FLAG = NO
		! Log it
	END IF




	!		O F F - L I N E   R E Q U E S T E D




	IF MH2_MHQ_OPEN = YES AND					&
		(MH2_MHQM_STATUS AND ONLINE_STATUS) = NO THEN
		
		ERR_TEXT = "Off-Line Request"

		MH2_REQUEST_PROCESSED = YES

		CALL STDLOG("*** MHQM Requested OFF-LINE ***") IF CALL_SLR

		GOSUB CLOSE_MHQ
		! Close Que

		! The actual CLOSE routine will notify the users
	END IF




	!	 T H E   R E S T   O F   T H E S E   W A K E U P
	! R E A S O N S   W I L L   N O T   F U N C T I O N   U N L E S S
	!	      T H E   M H Q M   I S   O N - L I N E




	IF (MH2_MHQM_STATUS AND ONLINE_STATUS) = NO THEN

		IF TFC_DEBUG = YES AND (MH2_MHQM_REASONS AND		&
			(SEND_REQUEST_BIT OR MHQM_SCAN_MHQ_BIT)) <>	&
				MHQM_SCAN_MHQ_BIT THEN

			IF MH2_MHQM_REASONS AND SEND_REQUEST_BIT THEN
				ERR_TEXT = "%DEBUG: MHQM is OFFLINE, "	&
					+ ", ignoring SEND REQUEST"
			ELSE
				ERR_TEXT = "%DEBUG: MHQM is OFFLINE, "	&
					+ ", ignoring REQUEST"
			END IF

			PRINT ERR_TEXT
		END IF

		MH2_MHQM_REASONS = MH2_MHQM_REASONS AND			&
			NOT(SEND_REQUEST_BIT OR MHQM_SCAN_MHQ_BIT)
		! Clear reasons

		GOTO EFN_WAIT
		! Start over
	END IF





	!		S E N D   R E Q U E S T



	!	This routine processes requests from Users to send to the
	! symbiont.  The request is validated and, if it passes, put into
	! the MHQ/SSL/QOL.  The  next  pass through the program code will
	! cause the MHQ to be sent.


	IF MH2_MHQM_REASONS AND SEND_REQUEST_BIT THEN

		GOSUB SEND
		! Set status, clear reasons, etc.

		IF PROCESS_STATUS <> MH2.ERR_SUCCESS THEN
			! Only if not an unknown user

			IF SYS_STATUS <> MH2.ERR_UNKNOWN THEN
				CALL STDLOG("Request Rejected, " +	&
					GETSYM(PROCESS_STATUS))		&
						IF CALL_SLR
				! Log it

				GOSUB NOTIFY_USER
				! Tell the guy that he's out of luck
			END IF
		ELSE

			GOSUB ADD_REC_TO_MHQ
			! Attempt to add record to MHQ

			IF SYS_STATUS <> SUCCESS THEN

				PROCESS_STATUS = SYS_STATUS
				GOSUB NOTIFY_USER
				! Can't place in MHQ
			ELSE
				IF (MHQ_STATUS AND MHQ_WAIT_STATUS) =	&
					NO THEN

					CALL QOLSTO(SYS_STATUS)
					! Add to QOL

					PRINT "%DEBUG: Adding " +	&
						"to QOL slot #";	&
						SYS_STATUS IF TFC_DEBUG
				END IF

				CALL STDLOG("User " + MHQ_PROCESS +	&
					MHQ_USER_ID + " requires LDU" +	&
						" response")		&
						IF CALL_SLR = YES AND	&
							(MHQ_FLAGS AND	&
						NO_RESPONSE_BIT) = NO

				IF MHQ_FLAGS AND MHQ_NOWAIT_BIT THEN

					PROCESS_STATUS	= MH2.ERR_SUCCESS
					GOSUB NOTIFY_USER
					! If user doesn't want to wait for
					! transmission, Tell him to go on
					! his way
				ELSE
					CALL STDLOG("User " +		&
						PROCESS_ID +		&
						MHQ_PROCESS +		&
						" Awaits transmission")	&
							IF CALL_SLR
				END IF
			END IF
		END IF

		MH2_MHQM_STATUS = MH2_MHQM_STATUS			&
			AND NOT MHQM_PROCESSING_USER_STATUS
		! Clear status
	END IF





	!		  T I M E R   E X P I R A T I O N
	!			( S C A N   M H Q )



	!	This routine can make a few assumptions because it will not
	! be  executed  UNLESS  our  timer  expires (which will only happen
	! if nobody  is  talking  to us),  OR  a  MANUALLY set request took
	! place (which can only be done by TFC).


	IF MH2_MHQM_REASONS AND MHQM_SCAN_MHQ_BIT THEN

		ERR_TEXT = "Scan MHQ"

		MH2_MHQM_REASONS = MH2_MHQM_REASONS AND NOT		&
			MHQM_SCAN_MHQ_BIT
		! Clear bit

		MH2_REQUEST_PROCESSED = YES
		! Set flag

		PRINT "%DEBUG: *** Scanning MHQ  ***" IF TFC_DEBUG

		MH2_MHQM_STATUS = MH2_MHQM_STATUS OR MHQM_SCAN_MHQ_STATUS
		! Set status

		IF MH2_MHQ_REC_COUNT - MH2_MHQM_QOL_SIZE = 0% THEN
			!PRINT "%DEBUG: No eligible records in MHQ"	&
			!	IF TFC_DEBUG

			GOTO TIME_EXPIRED_EXIT
		ELSE
			PRINT "%DEBUG: There are ";MH2_MHQ_REC_COUNT -	&
				MH2_MHQM_QOL_SIZE;			&
				" MHQ recs not in QOL" IF TFC_DEBUG
		END IF

		TMP = 0%
		! This is used to determine when no more MHQ records

		FOR TMPL = 0% TO MHQ_SIZE_LIMIT - 1%
		! Entire MHQ

			GOTO TIME_EXPIRED_EXIT				&
				IF TMP = MH2_MHQ_REC_COUNT
			! We are done

			GOSUB READ_EFN
			! Check EFNs

			GOTO TIME_EXPIRED_EXIT				&
				IF (EFN_FLAGS AND			&
					(MHQM_WAKEUP_EFN OR		&
					MHQM_SYMBIONT_WAKEUP_EFN OR	&
					USER_RTS_EFN))
				! We have been talked to, so abort this

			GOTO TIME_EXPIRED_EXIT IF MH2_MHQM_QOL_SIZE =	&
				MH2_MHQM_QOL_LIMIT
			! Done if QOL is full

			CALL BITOPS(MH2_SSL BY REF,			&
				TMPL BY VALUE, 0% BY VALUE,		&
					SYS_STATUS BY REF)
			! Test SSL bit for MHQ entry

			IF SYS_STATUS = EFN_WASSET THEN

				MHQ_REC = MHQ(TMPL + 1%)
				! Get record

				TMP = TMP + 1%
				! Up counter

				IF (MHQ_STATUS AND			&
					MHQ_IN_QOL_STATUS) = NO	THEN

					CALL QOLSTO(SYS_STATUS)
					! Try to add it
					! No error should be possible

					MHQ_STATUS = MHQ_STATUS OR	&
						MHQ_IN_QOL_STATUS
					! Set Bit

					MHQ(TMPL + 1%) = MHQ_REC
					! Put record back

					PRINT "%DEBUG: MHQ Rec #";	&
						TMPL + 1%;" added to";	&
						" QOL slot #";		&
						SYS_STATUS IF TFC_DEBUG
				END IF
			END IF
		NEXT TMPL
			

 TIME_EXPIRED_EXIT:

		MH2_MHQM_STATUS = MH2_MHQM_STATUS AND			&
			NOT MHQM_SCAN_MHQ_STATUS
		! Clear bit
	END IF




	!		U N K N O W N   W A K E U P



	CALL STDLOG("*** Unknown reason for wakeup ***")		&
		IF (MH2_REQUEST_PROCESSED = NO) AND CALL_SLR
	! Log the condition




	!		E N D   O F   L O O P


	GOTO EFN_WAIT

	


 CLEANUP:

	IF MH2_MHQ_OPEN = YES AND CLEANUP_FLAG = NO THEN

		CLEANUP_FLAG = YES
		! Set trigger

		GOSUB CLOSE_MHQ
		! Close MHQ
	END IF

	CLEANUP_FLAG = NO

	MH2_MHQM_STATUS = MH2_MHQM_STATUS AND (LOGGING_STATUS		&
		OR EXEC_ERROR OR BASIC_ERROR OR DEBUG_STATUS)
	! Clear grabage status bits

	PRINT "%DEBUG: Clearing USER PROGRAM WAKEUP EFN" IF TFC_DEBUG
	SLEEP 1%
	! Sleep to ensure users got the message before erasing it

	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$CLREF(USER_WAKEUP_EFN_NO BY VALUE)
	%ELSE						! RSX
		CALL CLREF(USER_WAKEUP_EFN_NO, SYS_STATUS)
	%END %IF
	! Clear event


	CALL STDLOG("MHQM Status has been cleared") IF CALL_SLR

	GOSUB CLOSE_LOGFILE
	! Close logfile

	DIE_ON_ERROR = YES
	PRINT "%DEBUG: Clearing OUR On-Line EFN" IF TFC_DEBUG

	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$CLREF(MHQM_EFN_NO BY VALUE)
	%ELSE						! RSX
		CALL CLREF(MHQM_EFN_NO, SYS_STATUS)
	%END %IF

	%IF %VARIANT <> 0% %THEN
		ERR_TEXT = "Detaching Common EFNs"
		PRINT "%DEBUG: " + ERR_TEXT IF TFC_DEBUG
		CALL ELGF(, SYS_STATUS)
		GOTO EXEC_ERROR UNLESS SYS_STATUS = SUCCESS
	%END %IF


	CALL INMHQM
	! Clear memory

	PRINT CR + LF + "Program execution terminated at ";FNT;LF



9999	GOTO PROGRAM_EXIT
	! Skip over rest of code


	%PAGE
	%SBTTL	"Subroutines and Functions"


10000	!
	!
	!		L O C A L   S U B R O U T I N E S
	!
	!




 SEND:

	ERR_TEXT = "Send Request"

	MH2_MHQM_STATUS = MH2_MHQM_STATUS OR MHQM_PROCESSING_USER_STATUS
	! Set status

	MH2_MHQM_REASONS = MH2_MHQM_REASONS AND NOT SEND_REQUEST_BIT
	! Clear Bit

	MH2_REQUEST_PROCESSED = YES
	! Set flag

	CALL STDLOG("*** Send to LDU Requested ***") IF CALL_SLR
	! Log reason if necessary

	CALL STDLOG("Request is from a TFC utility")			&
		IF (MH2_USER_FLAGS AND USER_STANDALONE_BIT) AND CALL_SLR

	IF MH2_REQUEST_ID = NULL_STRING THEN

		CALL STDLOG("Send is from an unknown user, Request " +	&
			"ignored") IF CALL_SLR
		! Log it

		PROCESS_STATUS = MH2.ERR_UNKNOWN
		! As long as SYS_STATUS <> MH2.ERR_SUCCESS
	ELSE
		! MH2IO checks to see if the MHQM is up and running,
		! and checks the symbiont also, so we don't have to.

		PROCESS_USER_ID	= MH2_USER_ID
		PROCESS_ID	= MH2_REQUEST_ID
		PROCESS_STATUS	= MH2.ERR_SUCCESS
		MH2_USER_ID	= NULL_STRING
		MH2_REQUEST_ID	= NULL_STRING
		! Save/Clear stuff

		PROCESS_STATUS = MH2.ERR_QOLFULLF			&
			IF (MH2_USER_FLAGS2 AND	MHQ_WAIT_STATUS) = NO	&
				AND MH2_MHQM_QOL_SIZE = MH2_MHQM_QOL_LIMIT
		! User will wait for response, but there's no
		! room in the QOL

		ERR_TEXT = "Processing Send Request"
	END IF

	RETURN





 ADD_REC_TO_MHQ:

	SYS_STATUS = SUCCESS
	! Starting point

	CALL FFCBAS(MH2_SSL BY REF, MHQ_SIZE_LIMIT BY VALUE, TMPL BY REF)
	! Find first free SSL entry and reserve it

	IF TMPL = MH2.ERR_MHQFULL THEN
		SYS_STATUS = MH2.ERR_MHQFULL
		! MHQ full

		PRINT "%DEBUG: MHQ full, can't add record" IF TFC_DEBUG
	ELSE
		MH2_LAST_MHQ_SEQ_NO = MH2_LAST_MHQ_SEQ_NO + 1%
		MH2_LAST_MHQ_SEQ_NO = 1% IF MH2_LAST_MHQ_SEQ_NO = 32767%
		! Increment Seq #

		CALL STDLOG("Adding Seq #" +				&
			NUM1$(MH2_LAST_MHQ_SEQ_NO) +			&
			" to MHQ, LDU cmd '" +				&
			MH2_LDU_CMD_STR + "', Type " +			&
			CHR$((MH2_USER_FLAGS AND LDU_TYPE_BITS) / 16% +	&
			65%)) IF CALL_SLR

		MHQ_PRIORITY	= MH2_USER_PRIORITY
		MHQ_SEQUENCE	= MH2_LAST_MHQ_SEQ_NO
		QOL_MHQ_SLOT	= TMPL
		MHQ_BUF		= MH2_USER_SBUF
		MHQ_BLEN	= MH2_USER_SLEN
		MHQ_FLAGS	= MH2_USER_FLAGS
		MHQ_STATUS	= MH2_USER_FLAGS2
		MHQ_PROCESS 	= PROCESS_ID
		TMP = CNVDAT(DATE$(0%), MHQ_QDATE, 0%)
		TMP = CNVTIM(TIME$(0%), MHQ_QTIME, 0%)
		MHQ_USER_ID	= PROCESS_USER_ID
		MHQ_UIC		= MH2_USER_UIC
		MHQ_AUTH_ID	= NULL_STRING
		MHQ_LDU_CMD_STR	= MH2_LDU_CMD_STR
		MHQ_STATUS	= MHQ_STATUS OR MHQ_IN_QOL_STATUS	&
			UNLESS MHQ_STATUS AND MHQ_WAIT_STATUS
		! Build record

		PRINT "%DEBUG: Adding as MHQ rec #";TMPL;", Priority ";	&
			MHQ_PRIORITY;", Flags =";MHQ_FLAGS IF TFC_DEBUG
		MHQ(TMPL) = MHQ_REC
		! Add record to MHQ

		MH2_MHQ_REC_COUNT = MH2_MHQ_REC_COUNT + 1%
		! Increment MHQ record counter
	END IF

	RETURN





 SYMBIONT_SENT_MESSAGE:

	IF (MHQ_FLAGS AND NO_RESPONSE_BIT) = NO THEN
		! An LDU response

		IF CALL_SLR THEN
			GOSUB BUILD_ADDR
			! Construct address string

			CALL STDLOG(" Received data from " + TMP_STR)
			! Log it
		END IF

		IF MHQ_FLAGS AND MHQ_NOWAIT_BIT THEN
			CALL STDLOG(" Can't notify user about "	+	&
				"response") IF CALL_SLR
		ELSE
			GOSUB SEND_TO_USER
			! Forward LDU response to user
		END IF

		RETURN
	END IF

	GOSUB SYMBIONT_SENT_ERR
	! Log it and tell the user

	RETURN





 SYMBIONT_SENT_ERR:

	IF CALL_SLR THEN
		CALL STDLOG(" Received '" + GETSYM(TMP_ERR) +		&
			"' from Symbiont")
		! Log error

		CALL STDLOG(" Can't notify user about this.")		&
			IF (MHQ_FLAGS AND MHQ_NOWAIT_BIT) AND		&
				(TMP_ERR <> MH2.ERR_SUCCESS)
	END IF

	IF (MHQ_FLAGS AND MHQ_NOWAIT_BIT) = NO THEN
	! If user is still waiting ...

		PROCESS_ID	= MHQ_PROCESS
		PROCESS_USER_ID	= MHQ_USER_ID
		PROCESS_STATUS	= TMP_ERR
		! Set up blk to send

		GOSUB NOTIFY_USER
		! Send it
	ELSE
		REQUE = YES
		! Re-que
	END IF

	RETURN





 GET_MHQ_REC:

	PRINT "%DEBUG: Processing MHQ Rec #";MH2_CURRENT_MHQ_REC;	&
		", expecting Seq #";MH2_CURRENT_MHQ_SEQ_NO IF TFC_DEBUG

	MHQ_REC = MHQ(MH2_CURRENT_MHQ_REC)
	! Get record

	IF MHQ_PRIORITY <> -1% THEN
		PRINT "%DEBUG: MHQ record had Sequence #";MHQ_SEQUENCE	&
			IF TFC_DEBUG

		IF MH2_CURRENT_MHQ_SEQ_NO <> MHQ_SEQUENCE THEN

			CALL STDLOG("??? MHQ and QOL had unmatched " +	&
				"sequence numbers.") IF CALL_SLR
			! Log

			MH2_CURRENT_MHQ_REC, MH2_CURRENT_MHQ_SEQ_NO = 0%
			! Clear pointers
		END IF
	ELSE
		CALL STDLOG("??? QOL indexed a blank MHQ record") IF CALL_SLR

		MH2_CURRENT_MHQ_REC, MH2_CURRENT_MHQ_SEQ_NO = 0%
		! Referenced a deleted record
	END IF

	RETURN





 SYMBIONT_DIED:

	CALL STDLOG("Symbiont sent " + MID("EXEC BASIC",		&
		(ABS((MH2_HANDLER_STATUS AND				&
			BASIC_ERROR) <> NO)) * 5% + 1%,			&
				5%) + " error #" + NUM1$(MH2_STATUSL))	&
					IF CALL_SLR
	! Log BASIC/EXEC error in the symbiont

	MH2_HANDLER_STATUS = 0%
	MH2_STATUSL = SUCCESS
	! Clear status

	PROCESS_STATUS	= MH2.ERR_SYMBDIED
	PROCESS_ID	= NULL_STRING
	GOSUB NOTIFY_USER
	! Tell all users

	RETURN




 RECEIVE:

	ERR_TEXT = "Receive from Symbiont"

	MH2_REQUEST_PROCESSED = YES
	! Set flag

	MH2_MHQM_STATUS = MH2_MHQM_STATUS OR RECEIVE_STATUS
	! Set status

	PRINT "%DEBUG: *** Message received from Symbiont ***" IF TFC_DEBUG

	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$CLREF(MHQM_SYMBIONT_WAKEUP_EFN_NO	&
			BY VALUE)
	%ELSE						! RSX
		CALL CLREF(MHQM_SYMBIONT_WAKEUP_EFN_NO, SYS_STATUS)
	%END %IF
	! Clear the EFN signifying receive from symbiont

	REQUE, TMP_ERR = NO
	! Starting values

	RETURN




 DISABLE_LOGGING:

	MH2_MHQM_REASONS = MH2_MHQM_REASONS AND	NOT DISABLE_LOGGING_BIT
	! Clear reason

	MH2_REQUEST_PROCESSED = YES
	! Set flag

	CALL STDLOG("*** Logging Requested Disabled ***") IF CALL_SLR
	! Log it if required

	GOSUB CLOSE_LOGFILE
	! Close logfile

	RETURN




 CLOSE_LOGFILE:

	PRINT #1%,LF + LF + FNT +					&
		"  This is the final entry in this log!!"		&
			IF MH2_MHQM_STATUS AND LOGGING_STATUS
	! Log reason if necessary

	MH2_MHQM_STATUS = MH2_MHQM_STATUS AND NOT LOGGING_STATUS
	! Clear logging status

	CLOSE 1%
	! Close file

	RETURN




 BUILD_ADDR:

	IF (MHQ_FLAGS AND LDU_TYPE_BITS) = LDU_TYPE_A THEN

		TMP_STR = "LDU #" + NUM1$((ASCII(MH2_ADDRESS_S2) *256.) &
			+ ASCII(MH2_ADDRESS_S1)) + "-" +		&
			NUM1$(ASCII(MH2_ADDRESS_S3))
	ELSE
		TMP_STR = "LDU #" + NUM1$(ASCII(MH2_ADDRESS_S1) +	&
			(256. * ASCII(MH2_ADDRESS_S2)) +		&
			(32768. * ASCII(MH2_ADDRESS_S3)))
	END IF

	TMP_STR = "LDU <All>" IF					&
				MH2_ADDRESS_1 = -1%			&
		AND		MH2_ADDRESS_2 = -1%			&
		AND		MH2_ADDRESS_3 = -1%

	TMP_STR = "<Modems>" IF	MH2_ADDRESS_1 = 0%			&
		AND		MH2_ADDRESS_2 = 0%			&
		AND		MH2_ADDRESS_3 = 0%
	! Modem BREAK character

	TMP_STR = TMP_STR + ", Type " + CHR$(65% +			&
		((MHQ_FLAGS AND LDU_TYPE_BITS) / 16%))
	! Figure out how to display LDU address if we need it
	! Type A is XXXXX-XX for site-LDU
	! Type B is XXXXXXXX for full address

	RETURN




 CLOSE_MHQ:

	CALL STDLOG("Closing MHQ and taking MHQM Off-Line") IF CALL_SLR

	PROCESS_ID	= NULL_STRING
	PROCESS_STATUS	= MH2.ERR_MHQMOFFL
	GOSUB NOTIFY_USER
	! Tell ALL

	CALL	CLSMHQ
	! Do it

	MH2_MHQ_OPEN = NO
	RETURN


		
 READ_EFN:

	! ERR_TEXT = "Reading EFNs"

	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$READEF(MH2_EFN_CLUSTER BY VALUE,	&
			EFN_FLAGS BY REF)
	%ELSE						! RSX
		CALL RDXF(EFN_FLAGS BY REF)
		SYS_STATUS = SUCCESS
	%END %IF

	! GOTO EXEC_ERROR UNLESS SYS_STATUS = SUCCESS OR		&
	!	SYS_STATUS = EFN_WASSET OR SYS_STATUS = EFN_WASCLR

	! ERR_TEXT = NULL_STRING

	RETURN




 SEND_TO_SYMBIONT:

	MH2_MHQM_STATUS = MH2_MHQM_STATUS OR MHQM_MHQ_STATUS
	! Set processing bit

	MH2_CURRENT_MHQ_REC, MH2_CURRENT_MHQ_SEQ_NO = 0%
	! Clear pointer (this routine can loop back on itself, so it is
	! more efficient to clear here than the other places

	ERR_TEXT = "Send to Symbiont"

	IF MH2_MHQM_QOL_SIZE > 0% AND (EFN_FLAGS AND			&
		SYMBIONT_EFN) <> 0% AND (MH2_HANDLER_STATUS AND		&
			ONLINE_STATUS) <> 0% THEN
		! If the is a QOL and the handler is there and online...

		IF MH2_HANDLER_STATUS AND BUSY_STATUS THEN

			PRINT "%DEBUG: %Handler busy, skipping Send"	&
				IF TFC_DEBUG
		ELSE
			PRINT "%DEBUG: *** Searching QOL for next " +	&
				"record to send ***" IF TFC_DEBUG

			CALL QOLPOP(SYS_STATUS)
			! Get QOL record

			PRINT "%DEBUG: QOLPOP returned slot #";		&
				SYS_STATUS IF TFC_DEBUG

			MH2_CURRENT_MHQ_REC	= QOL_MHQ_SLOT
			MH2_CURRENT_MHQ_SEQ_NO	= QOL_SEQUENCE
			! Save pointer before we lose it

			GOSUB GET_MHQ_REC
			! Get MHQ rec

			IF MH2_CURRENT_MHQ_REC = 0% THEN

				PRINT "%DEBUG: Will try for " +		&
					"another record" IF TFC_DEBUG

				GOTO SEND_TO_SYMBIONT
				! Try again
			ELSE
				MH2_CURRENT_MHQ_SEQ_NO = MHQ_SEQUENCE
				! Da seq #

				CALL RESEFN(USER_RESPONSE_EFN,		&
					USER_RESPONSE_EFN_NO)
				! Make sure we can send
				! (Even globals cause buffer to get
				!  destroyed)

				MH2_MHQM_STATUS = (MH2_MHQM_STATUS AND	&
					NOT(MHQM_MHQ_STATUS)) OR	&
						SEND_STATUS
				! Set sending, clear MHQ processing

				MH2_HANDLER_SEND_FLAGS	=		&
					MHQ_FLAGS AND 255%
				MH2_HANDLER_REASONS	=		&
					MH2_HANDLER_REASONS OR		&
						SEND_REQUEST_BIT
				MH2_SBUF = MHQ_BUF
				MH2_SLEN = MHQ_BLEN
				! Set it up

				IF CALL_SLR THEN
					GOSUB BUILD_ADDR
					! Build address string

					CALL STDLOG("*** Sending Seq #"	&
						+ NUM1$(		&
						MH2_CURRENT_MHQ_SEQ_NO)	&
						+ " to " + TMP_STR	&
						+ " ***")
				END IF

				IF TFC_DEBUG THEN

					TMP = MH2_COMMAND
					TMP = TMP + 256% IF TMP < 0%
					TMP = 0% IF MHQ_LDU_CMD_STR = "BRC"
					! extract LDU command value

					PRINT "%DEBUG: LDU Cmd is '";	&
						MHQ_LDU_CMD_STR;	&
						"' (#";NUM1$(TMP);"),"

					PRINT "%DEBUG: for user ";	&
						EDIT$(MHQ_PROCESS,	&
						EDIT_ARG);" ";		&
						MHQ_USER_ID;", entered ";

					TMP_STR = "??-???-??"
					TMP = CNVDAT(MHQ_QDATE,		&
						TMP_STR, 1%)
					PRINT TMP_STR + " ";

					TMP_STR = "??:??"
					TMP = CNVTIM(MHQ_QTIME,		&
						TMP_STR, 2%)
					PRINT TMP_STR

					PRINT "%DEBUG: Setting " +	&
						"SYMBIONT WAKEUP EFN"
				END IF

				%IF %VARIANT = 0% %THEN		! VMS
					SYS_STATUS = SYS$SETEF(		&
						SYMBIONT_WAKEUP_EFN_NO	&
							BY VALUE)

				%ELSE				! RSX
					CALL SETEF(			&
						SYMBIONT_WAKEUP_EFN_NO,	&
							SYS_STATUS)
				%END %IF
				! Do it
			END IF
		END IF
	ELSE
		IF TFC_DEBUG THEN

			IF (MH2_MHQM_QOL_SIZE = 0%) THEN		&
				PRINT "%DEBUG: QOL empty, skipping send"
			ELSE
				PRINT "%DEBUG: Symbiont not On-Line, "	&
					+ "skipping send"
			END IF			
		END IF
	END IF

	MH2_MHQM_STATUS = MH2_MHQM_STATUS AND				&
		NOT (MHQM_MHQ_STATUS OR SEND_STATUS)
	! Clear status

	RETURN




 DELETE_MHQ_ENTRY:

	! This routine will delete a record in the MHQ and SSL.
	! MH2_CURRENT_MHQ_REC is the record to delete, it should already
	! be in the MHQ_REC map. The QOL entry is not touched.

	PRINT "%DEBUG: Deleting MHQ/SSL Entry #";MH2_CURRENT_MHQ_REC	&
		IF TFC_DEBUG

	CALL BITOPS(MH2_SSL BY REF, MH2_CURRENT_MHQ_REC - 1% BY VALUE,	&
		-1% BY VALUE, TMPL BY REF)
	! Remove from SSL

	MH2_CURRENT_MHQ_SEQ_NO, MH2_CURRENT_MHQ_REC = 0%
	! Clear 'em

	MH2_MHQ_REC_COUNT = MH2_MHQ_REC_COUNT - 1%
	! Decrement counter

	RETURN




 SEND_TO_USER:

	CALL STDLOG(" Sending LDU response to user " +			&
		MHQ_PROCESS + MHQ_USER_ID) IF CALL_SLR

	MH2_RESPONSE_ID = MHQ_PROCESS
	MH2_USER_RBUF	= MH2_RBUF
	MH2_USER_RLEN	= MH2_RLEN

	PRINT "%DEBUG: Setting USER RESPONSE EFN" IF TFC_DEBUG

	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$SETEF(USER_RESPONSE_EFN_NO BY VALUE)
	%ELSE						! RSX
		CALL SETEF(USER_RESPONSE_EFN_NO, SYS_STATUS)
	%END %IF

	RETURN



 NOTIFY_USER:

	IF CALL_SLR THEN
		TMP_STR = EDIT$(PROCESS_ID, EDIT_ARG)
		ERR_TEXT = "User " + TMP_STR + " " + PROCESS_USER_ID
		ERR_TEXT = "ALL Users" IF TMP_STR = NULL_STRING
		ERR_TEXT = " Notifying " + ERR_TEXT
		ERR_TEXT = ERR_TEXT + ", " +				&
			GETSYM(PROCESS_STATUS)				&
				IF TMP_STR <> NULL_STRING

		CALL STDLOG(ERR_TEXT)
		! Log the call
	END IF

	CALL RESEFN(USER_WAKEUP_EFN, USER_WAKEUP_EFN_NO)
	! Reserve EFN

	MH2_WAKEUP_ID	= PROCESS_ID
	MH2_USER_STATUS = PROCESS_STATUS

	PRINT "%DEBUG: Setting USER WAKEUP EFN" IF TFC_DEBUG

	%IF %VARIANT = 0% %THEN				! VMS
		SYS_STATUS = SYS$SETEF(USER_WAKEUP_EFN_NO BY VALUE)
	%ELSE						! RSX
		CALL SETEF(USER_WAKEUP_EFN_NO, SYS_STATUS)
	%END %IF

	ERR_TEXT = NULL_STRING IF CALL_SLR

	RETURN




15000	!
	!
	!		L O C A L   F U N C T I O N S
	!
	!


	%PAGE
	%SBTTL	"Error Traps"


19000	!
	!
	!		E R R O R   R O U T I N E
	!
	!

 ERROR_ROUTINE:

	MH2_MHQM_STATUS2 = ERR
	MH2_MHQM_STATUS = MH2_MHQM_STATUS OR BASIC_ERROR
	! Set error condition

	GOTO 19990



 EXEC_ERROR:

	MH2_MHQM_STATUS = MH2_MHQM_STATUS AND NOT ONLINE_STATUS
	! Take ourself offline

	CALL	EXCERR(SYS_STATUS, ERR_TEXT)
	! Print message

	MH2_MHQM_STATUS2 = SYS_STATUS
	MH2_MHQM_STATUS = MH2_MHQM_STATUS OR EXEC_ERROR
	GOTO CLEANUP UNLESS DIE_ON_ERROR
	GOTO PROGRAM_EXIT
	! EXEC error


19990	PRINT
	PRINT "%Unexpected Error ";
	PRINT "in module ";ERN$; IF LEN(EDIT$(ERN$, EDIT_ARG))
	PRINT 
	PRINT "%Error (";NUM1$(ERR);") - ";ERT$(ERR)
	PRINT "%Error occurred at or near module line number ";NUM1$(ERL)
	PRINT "%While in routine '";ERR_TEXT;"'" IF LEN(ERR_TEXT)
	PRINT
	RESUME 32767 IF ERL=32760
	! Unknown error



32760	!
	!
	!		P R O G R A M    E X I T
	!
	!

 PROGRAM_EXIT:

	CLOSE I FOR I = 1% TO 12%
	! Close any open I/O channels

	ON ERROR GOTO 0
	! Prepare to end



32767	END
