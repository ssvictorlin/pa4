/*	User-level thread system
 *
 */

#include <setjmp.h>

#include "aux.h"
#include "umix.h"
#include "mythreads.h"

static int MyInitThreadsCalled = 0;	// 1 if MyInitThreads called, else 0
static int next_avail = 1;
static int current_create = 0;
static struct thread {			// thread table
	int valid;			// 1 if entry is valid, else 0
	jmp_buf env;
	void (*f)();			// current context
	int p;
} thread[MAXTHREADS];

#define STACKSIZE	65536		// maximum size of thread stack

/*	MyInitThreads () initializes the thread package. Must be the first
 *	function called by any user program that uses the thread package.  
 */

void MyInitThreads ()
{
	int i;

	if (MyInitThreadsCalled) {		// run only once
		Printf ("InitThreads: should be called only once\n");
		Exit ();
	}

	for (i = 1; i < MAXTHREADS; i++) {	// initialize thread table
		char s[i * STACKSIZE];

		if (((int) &s[STACKSIZE-1]) - ((int) &s[0]) + 1 != STACKSIZE) {
			Printf ("Stack space reservation failed\n");
			Exit ();
		}

		if (setjmp (thread[i].env) == 0) {
			// do nothing
		} else {
			void (*f)() = thread[i].f;
			int p = thread[i].p;
			if (setjmp (thread[i].env) == 0) {
				longjmp(thread[current_create].env, current_create);
			} 

			(*f)(p);
			MyExitThread();
		}
		thread[i].valid = 0;
	}

	thread[0].valid = 1;			// initialize thread 0

	MyInitThreadsCalled = 1;
}

/*	MyCreateThread (func, param) creates a new thread to execute
 *	func (param), where func is a function with no return value and
 *	param is an integer parameter.  The new thread does not begin
 *	executing until another thread yields to it. 
 */

int MyCreateThread (func, param)
	void (*func)();			// function to be executed
	int param;			// integer parameter
{
	int i, cur, me;
	if (! MyInitThreadsCalled) {
		Printf ("CreateThread: Must call InitThreads first\n");
		Exit ();
	}
	current_create = MyGetThread();
	if (setjmp(thread[current_create].env) == 0) {
		// find the available postion to create thread
		while (1) {
			if (thread[next_avail].valid) {
				next_avail = (next_avail+1)%10;
			} 
			else {
				cur = next_avail;
				next_avail = (next_avail+1)%10;
				thread[cur].valid = 1;
				thread[cur].f = func;
				thread[cur].p = param;
				longjmp(thread[cur].env, cur);
			}
		}
	
	// mark the entry for the new thread valid
	} 

	return (cur);		// done, return new thread ID
}
/*	MyYieldThread (t) causes the running thread, call it T, to yield to
 *	thread t.  Returns the ID of the thread that yielded to the calling
 *	thread T, or -1 if t is an invalid ID. Example: given two threads
 *	with IDs 1 and 2, if thread 1 calls MyYieldThread (2), then thread 2
 *	will resume, and if thread 2 then calls MyYieldThread (1), thread 1
 *	will resume by returning from its call to MyYieldThread (2), which
 *	will return the value 2.
 */

int MyYieldThread (t)
	int t;				// thread being yielded to
{
	if (! MyInitThreadsCalled) {
		Printf ("YieldThread: Must call InitThreads first\n");
		Exit ();
	}

	if (t < 0 || t >= MAXTHREADS) {
		Printf ("YieldThread: %d is not a valid thread ID\n", t);
		return (-1);
	}
	if (! thread[t].valid) {
		Printf ("YieldThread: Thread %d does not exist\n", t);
		return (-1);
	}

        if (setjmp (thread[1-t].env) == 0) {
                longjmp (thread[t].env, 1);
        }
}

/*	MyGetThread () returns ID of currently running thread.  
 */

int MyGetThread ()
{
	if (! MyInitThreadsCalled) {
		Printf ("GetThread: Must call InitThreads first\n");
		Exit ();
	}

}

/*	MySchedThread () causes the running thread to simply give up the
 *	CPU and allow another thread to be scheduled.  Selecting which
 *	thread to run is determined here. Note that the same thread may
 * 	be chosen (as will be the case if there are no other threads).  
 */

void MySchedThread ()
{
	if (! MyInitThreadsCalled) {
		Printf ("SchedThread: Must call InitThreads first\n");
		Exit ();
	}
}

/*	MyExitThread () causes the currently running thread to exit.  
 */

void MyExitThread ()
{
	if (! MyInitThreadsCalled) {
		Printf ("ExitThread: Must call InitThreads first\n");
		Exit ();
	}
}
