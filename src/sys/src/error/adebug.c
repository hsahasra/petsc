#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: adebug.c,v 1.71 1997/12/01 01:53:22 bsmith Exp bsmith $";
#endif
/*
      Code to handle PETSc starting up in debuggers, etc.
*/

#include <signal.h> 
#include "petsc.h"               /*I   "petsc.h"   I*/
#include "sys.h"
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif 
#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#include "pinclude/petscfix.h"

static char  *Debugger = "gdb", *Display = 0;
static int   Xterm     = 1;

#undef __FUNC__  
#define __FUNC__ "PetscSetDebugger"
/*@C
   PetscSetDebugger - Sets options associated with the debugger.

   Input Parameters:
.  debugger - name of debugger, which should be in your path,
              usually "dbx", "gdb", or "xxgdb".  Also, HP-UX
              supports "xdb", and IBM rs6000 supports "xldb".

.   xterm - flag to indicate debugger window, set to one of:
$     1 to indicate debugger should be started in a new xterm
$     0 to start debugger in initial window (zero makes no 
.           sense when using more than one processor.)
.   display - name of display for opening xterm, or null.

    Fortran Note:
    This routine is not supported in Fortran.

.keywords: Set, debugger, options

.seealso: PetscAttachDebugger(), PetscAttachDebuggerErrorHandler()
@*/
int PetscSetDebugger(char *debugger, int xterm,char *display)
{
  PetscFunctionBegin;
  if (debugger) Debugger = debugger;
  Xterm    = xterm;
  if (Display) {PetscFree(Display); Display = 0;}
  if (display) {
    int len = PetscStrlen(display)+1;
    Display = (char *) PetscMalloc(len*sizeof(char)); if (!Display) PetscFunctionReturn(0);
    PetscStrcpy(Display,display);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PetscAttachDebugger"
/*@C
   PetscAttachDebugger - Attaches the debugger to the running process.

.keywords: attach, debugger

.seealso: PetscSetDebugger()
@*/
int PetscAttachDebugger()
{
  int   child=0,sleeptime=0,flg=0,ierr=0;
  char  program[256];

  PetscFunctionBegin;
  ierr = PetscGetProgramName(program,256);
  if (ierr) {
    (*PetscErrorPrintf)("PETSC ERROR: Cannot determine program name\n");
    PetscFunctionReturn(1);
  }
#if defined(PARCH_t3d) 
  (*PetscErrorPrintf)("PETSC ERROR: Cray t3d cannot start debugger\n");
  MPI_Finalize();
  exit(0);
#elif defined(PARCH_nt) 
  (*PetscErrorPrintf)("PETSC ERROR: Windows NT cannot start debugger\n");
  MPI_Finalize();
  exit(0);
#else
  if (!program[0]) {
    (*PetscErrorPrintf)("PETSC ERROR: Cannot determine program name\n");
    PetscFunctionReturn(1);
  }
  child = fork(); 
  if (child <0) {
    (*PetscErrorPrintf)("PETSC ERROR: Error attaching debugger\n");
    PetscFunctionReturn(-11);
  }

  /* Swap role the parent and child. */
#if !defined(PARCH_solaris)
  if (child) { child=0; }
  else {    child = getppid(); }
#endif


  if (child) { /* I am the parent will run the debugger */
    char  *args[9],pid[9];
    /*
      This kill does not seem to be needed and causes trouble on most machines
      so let's try removing it.    kill(child,SIGSTOP);
    */
#if defined (PARCH_alpha)
    kill(child,SIGCONT);
#endif

    sprintf(pid,"%d",child); 
    if (!PetscStrcmp(Debugger,"xxgdb") || !PetscStrcmp(Debugger,"ups")) {
      args[1] = program; args[2] = pid; args[3] = "-display";
      args[0] = Debugger; args[4] = Display; args[5] = 0;
      (*PetscErrorPrintf)("PETSC: Attaching %s to %s %s\n",args[0],args[1],pid);
      if (execvp(args[0], args)  < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    }
#if defined(PARCH_rs6000)
    else if (!PetscStrcmp(Debugger,"xldb")) {
      args[1] = "-a"; args[2] = pid; args[3] = program;  args[4] = "-display";
      args[0] = Debugger; args[5] = Display; args[6] = 0;
      (*PetscErrorPrintf)("PETSC: Attaching %s to %s %s\n",args[0],args[1],pid);
      if (execvp(args[0], args)  < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    }
#endif
    else if (!Xterm) {
      args[1] = program; args[2] = pid; args[3] = 0;
      args[0] = Debugger;
#if defined(PARCH_IRIX) || defined(PARCH_IRIX64) || defined(PARCH_IRIX5)  
      if (!PetscStrcmp(Debugger,"dbx")) {
        args[1] = "-p";
        args[2] = pid;
        args[3] = program;
        args[4] = 0;
      }
#elif defined(PARCH_hpux)
      if (!PetscStrcmp(Debugger,"xdb")) {
        args[1] = "-l";
        args[2] = "ALL";
        args[3] = "-P";
        args[4] = pid;
        args[5] = program;
        args[6] = 0;
      }
#elif defined(PARCH_rs6000)
      if (!PetscStrcmp(Debugger,"dbx")) {
        args[1] = "-a";
        args[2] = pid;
        args[3] = 0;
      }
#elif defined(PARCH_alpha)
      if (!PetscStrcmp(Debugger,"dbx")) {
        args[1] = "-pid";
        args[2] = pid;
        args[3] = program;
        args[4] = 0;
      }
#endif
      (*PetscErrorPrintf)("PETSC: Attaching %s to %s of pid %s\n",Debugger,program,pid);
      if (execvp(args[0], args)  < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    }
    else {
      if (!Display) {
        args[0] = "xterm";  args[1] = "-e"; 
        args[2] = Debugger; args[3] = program; 
        args[4] = pid;      args[5] = 0;
#if defined(PARCH_IRIX) || defined(PARCH_IRIX64) || defined(PARCH_IRIX5) 
        if (!PetscStrcmp(Debugger,"dbx")) {
          args[3] = "-p";
          args[4] = pid;
          args[5] = program;
          args[6] = 0;
        }
#elif defined(PARCH_hpux)
        if (!PetscStrcmp(Debugger,"xdb")) {
          args[5] = program;
          args[3] = "-P";
          args[4] = pid;
          args[6] = 0;
        }
#elif defined(PARCH_rs6000)
        if (!PetscStrcmp(Debugger,"dbx")) {
          args[3] = "-a";
          args[4] = pid;
          args[5] = 0;
        }
#elif defined(PARCH_alpha)
      if (!PetscStrcmp(Debugger,"dbx")) {
        args[3] = "-pid";
        args[4] = pid;
        args[5] = program;
        args[6] = 0;
      }
#endif
        (*PetscErrorPrintf)("PETSC: Attaching %s to %s on pid %s\n",Debugger,program,pid);
      }
      else {
        args[0] = "xterm";  args[1] = "-d";
        args[2] = Display;  args[3] = "-e";
        args[4] = Debugger; args[5] = program;
        args[6] = pid;      args[7] = 0;
#if defined(PARCH_IRIX) || defined(PARCH_IRIX64) || defined(PARCH_IRIX5)
        if (!PetscStrcmp(Debugger,"dbx")) {
          args[5] = "-p";
          args[6] = pid;
          args[7] = program;
          args[8] = 0;
        }
#elif defined(PARCH_hpux)
        if (!PetscStrcmp(Debugger,"xdb")) {
          args[7] = program;
          args[5] = "-P";
          args[6] = pid;
          args[8] = 0;
        }
#elif defined(PARCH_rs6000)
        if (!PetscStrcmp(Debugger,"dbx")) {
          args[5] = "-a";
          args[6] = pid;
          args[7] = 0;
        }
#elif defined(PARCH_alpha)
      if (!PetscStrcmp(Debugger,"dbx")) {
        args[5] = "-pid";
        args[6] = pid;
        args[7] = program;
        args[8] = 0;
      }
#endif
      (*PetscErrorPrintf)("PETSC: Attaching %s to %s of pid %s on display %s\n",
              Debugger,program,pid,Display);
      }

      if (execvp("xterm", args)  < 0) {
        perror("Unable to start debugger");
        exit(0);
      }
    }
  }
  else { /* I am the child, continue with user code */
  sleeptime = 10; /* default to sleep for eight seconds waiting for debugger */
  ierr = OptionsGetInt(PETSC_NULL,"-debugger_pause",&sleeptime,&flg); CHKERRQ(ierr);
  if (sleeptime < 0) sleeptime = -sleeptime;
#if defined(PARCH_hpux)
    { 
      double x = 1.0;
      int i=10000000;
        while (i--) x++ ; /* cannot attach to sleeper */
    }
#elif defined(PARCH_rs6000)
    {
      int left = sleeptime;
      while (left > 0) {left = sleep(left) - 1;}
    }
#else
    sleep(sleeptime);
#endif
    PetscFunctionReturn(0);
  }
#endif
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "PetscAttachDebuggerErrorHandler"
/*@C
   PetscAttachDebuggerErrorHandler - Error handler that attaches
   a debugger to a running process when an error is detected.
   This routine is useful for examining variables, etc. 

   Input Parameters:
.  line - the line number of the error (indicated by __LINE__)
.  fun - function where error occured (indicated by __FUNC__)
.  file - the file in which the error was detected (indicated by __FILE__)
.  dir - the directory of the file (indicated by __SDIR__)
.  message - an error text string, usually just printed to the screen
.  number - the generic error number
.  p - the specific error number
.  ctx - error handler context

   Options Database Keys:
$   -on_error_attach_debugger [noxterm,dbx,xxgdb,xdb,xldb,gdb]
$       [-display name]

   Notes:
   By default the GNU debugger, gdb, is used.  Alternatives are dbx and
   xxgdb,xldb (on IBM rs6000), xdb (on HP-UX).

   Most users need not directly employ this routine and the other error 
   handlers, but can instead use the simplified interface SETERR, which has 
   the calling sequence
$     SETERRQ(number,p,message)

   Notes for experienced users:
   Use PetscPushErrorHandler() to set the desired error handler.  The
   currently available PETSc error handlers are
$    PetscTraceBackErrorHandler()
$    PetscAttachDebuggerErrorHandler()
$    PetscAbortErrorHandler()
   or you may write your own.

.keywords: attach, debugger, error, handler

.seealso:  PetscPushErrorHandler(), PetscTraceBackErrorHandler(), 
           PetscAbortErrorHandler()
@*/
int PetscAttachDebuggerErrorHandler(int line,char* fun,char *file,char* dir,int num,int p,
                                    char* mess,void *ctx)
{
  int ierr,rank;

  PetscFunctionBegin;
  if (!fun)  fun = "unknownfunction";
  if (!dir)  dir = " ";
  if (!mess) mess = " ";

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  (*PetscErrorPrintf)("[%d]PETSC ERROR: %s() line %d in %s%s %s\n",rank,fun,line,dir,file,mess);

  ierr = PetscAttachDebugger();
  if (ierr) { /* hopeless so get out */
    exit(num);
  }
  PetscFunctionReturn(0);
}
