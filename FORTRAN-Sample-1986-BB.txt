C+* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
C
C	LOOPHPF.FOR		Keith G. Barrett		17-APR-86
C
C
	PROGRAM LOOKHPF
C
C
C
C Functional description:
C

C
C	NOTE:	This routine  makes no  assumptions about variable types or
C		compilation switches. All logic and variables are explicit.
C
C
C Passed:	<None>
C
C Returned:	<None>
C
C Caveats:	<None>
C
C
C
C
C		     M O D I F I C A T I O N   H I S T O R Y
C
C
C	Edit Date	Who	Reason
C	---------	---	-----------------------------------------------
C
C
C-* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
C
C
C			       E N V I R O N M E N T
C
C Include files:
C
	INCLUDE	'CONSOLE_INC:EMFDEF.PAR'
C
C
C Parameter declarations:
C
C	<None>
C
C
C Local variables:
C
	PARAMETER	CLASS    = 1	! Class within HPF
	PARAMETER	HPFSIZ   = 2	! Size of HPF Item
	PARAMETER	SUCCESS  = 1	! "Success" status value
	PARAMETER	NORMFAIL = 2	! "Normal Failure" status value
	PARAMETER	TASKERR  = 4	! "Task error" status value
	PARAMETER	GAP      = 202	! "Gap found" ISTAT ID

	INTEGER*2	HPITEM		! Item within HPF buffer
	INTEGER*2	IDSTAT		! Error ID of ISTAT
	INTEGER*2	TMP		! Temporary uses
	INTEGER*2	JFLAG
	INTEGER*2	HEDR(2)
	INTEGER*2	NAMFIL		! Point-name file number
	INTEGER*2	FCNT
	INTEGER*2	I		! Temporary uses
	INTEGER*2	HSHPTR		! Hash pointer
	INTEGER*2	ITEMNO		! Item number if name file
	INTEGER*2	PNAME(12)	! Name as words
	INTEGER*2	ISTAT		! Returned status
	INTEGER*2	FLAG		! Error flag
	INTEGER*2	HPFLOC		! Item number in HPF
	INTEGER*2	HPF		! HPF file number

	BYTE		FILENO		! point-name file number
	BYTE		HPFBUF(HPFSIZ)	! Item buffer for HPF
	BYTE		NAMBUF(26)	! Name input buffer

	EQUIVALENCE	(HPITEM, HPFBUF(1))
	EQUIVALENCE	(PNAME(1),NAMBUF(1))
C
C
C External calls:
C
C	FDBUFF to buffer files
C	FDOPEN to open HPF (if not already open)
C	FDSTAT returns error ID for ISTAT
C	FDCLOS to close files this routine OPENED
C	FDGPOS to get items from HPF
C	FDINIT to initialize FDBD routines
C	HPFLUP to find item
C
C
C Common areas:
C
C	<None>
C
C
C
C External references:
C
C	:FNAHPF		Analog  HPF, found in S1TGEN
C	:FNDHPF		Digital HPF, found in S1TGEN
C
C
C+* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
C
C
C
C				M A I N   L O G I C
C
C
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C			      Initialize variables                            !
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C
	JFLAG = EMF$D_INVOKING
C
C
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C				OPEN FDBD files				      !
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C
	TYPE *
	TYPE *
	TYPE *,' LOOKHPF V1.0'
	TYPE *

	IDSTAT = 0
	FLAG = 4

	CALL FDINIT(ISTAT)

	IF ((ISTAT .NE. SUCCESS) .AND. (ISTAT .NE. 2)) THEN
		TMP = ISTAT
		CALL FDSTAT(TMP, IDSTAT) 	! Extract ISTAT ID
		GOTO 32000
	END IF
	FLAG = 0
C
C Get file to access
C

2	CONTINUE
	TYPE 3
3	FORMAT(/' [A]nalog or [D]igital: ',$)
	ACCEPT 5,FILENO
5	FORMAT(1A1)

	IF (FILENO .EQ. 'A') THEN
		HPF = :FNAHPF
		NAMFIL = :FNPNAM
	ELSE
		IF (FILENO .EQ. 'D') THEN
			HPF = :FNDHPF
			NAMFIL = :FNDINM
		ELSE
			TYPE *,' Must be D or A'
			GOTO 2
		END IF
	END IF
C
C
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C				Enter Point-name			      !
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C

150	TYPE 199
199	FORMAT(/' Enter  point-name  : ',$)

	ISTAT = SUCCESS

	READ (5,200) (NAMBUF(N),N=1,25)
200	FORMAT(25A1)

	IF (NAMBUF(1) .EQ. ' ') GOTO 32000
C
C
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C			    Find item					      !
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C
	CALL HPFLUP(ISTAT, NAMFIL, ITEMNO, NAMBUF, JFLAG, FCNT, HPFLOC, HEDR)
C
C
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C			    Report result, start again                        !
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C

	IF (ISTAT .EQ. SUCCESS) THEN
		TYPE 225,ITEMNO,HPFLOC,FCNT
225	FORMAT(' Item',I7,', found in slot #',I6,', in',I6,' tries.')
	ELSE
		IF (ISTAT .EQ. NORMFAIL) THEN
			TYPE 226,FCNT
226			FORMAT(' Item not found, took',I6,' tries')
		ELSE
			TYPE 227,ISTAT,ITEMNO,FCNT
227			FORMAT(' Error #',I4,', Item #',I7,', Tries =',I7)

		END IF
	END IF

	GOTO 150
C
C
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C                                   Exit                                      !
C - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
C

32000	CONTINUE

	TYPE *

	IF (ISTAT .NE. SUCCESS) TYPE 32020,FLAG,ISTAT,IDSTAT
32020	FORMAT(' ERROR: FLAG,ISTAT,IDSTAT=',I7,I7,I7)

C
C
C CLOSE HPF if necessary
C
	CALL FDCLOS(TMP, HPF)
	CALL FDDONE(ISTAT)
	IF (ISTAT .NE. SUCCESS) PAUSE 'ERROR ON DONE'
C
32767	END
