/*	User-level thread system
 *
 */

#include <setjmp.h>
#include <string.h>
#include "aux.h"
#include "umix.h"
#include "mythreads.h"

static int MyInitThreadsCalled = 0;	// 1 if MyInitThreads called, else 0
static int next_avail = 1;
static int last_created = 0;
static int current_create = 0;
static int current_run = 0;
static int number = 1;
static int head = 0;
static int tail = 0;

static struct thread {			// thread table
	int valid;			// 1 if entry is valid, else 0
	jmp_buf env;
	jmp_buf clean;
	void (*f)();			// current context
	int p;

	int whoYme;

	int next;
	int prev;
} thread[MAXTHREADS];

#define STACKSIZE	65536		// maximum size of thread stack
void deleteFromQueue(int);
void appendToQueue(int);
void PrintQ();
/*	MyInitThreads () initializes the thread package. Must be the first
 *	function called by any user program that uses the thread package.  
 */

void MyInitThreads ()
{
	int i, g, h;

	if (MyInitThreadsCalled) {		// run only once
		Printf ("InitThreads: should be called only once\n");
		Exit ();
	}
	for (i = 0; i < MAXTHREADS; i++) {	// initialize thread table
		char s[i * STACKSIZE];

		if (((int) &s[STACKSIZE-1]) - ((int) &s[0]) + 1 != STACKSIZE) {
			Printf ("Stack space reservation failed\n");
			Exit ();
		}
		// jump from createThread()
		if ((g = setjmp (thread[i].env)) == 0) {
			// make a copy of a clean env
			memcpy(thread[i].clean, thread[i].env, sizeof thread[i].env);
		} else {
			if (g == 11) g = 0;
			void (*f)() = thread[g].f;
			int p = thread[g].p;
			// jump back from yieldThread()
			//DPrintf ("Thread %d has been set its function...\n", g);
			if ((h = setjmp (thread[g].env)) == 0) {
				//DPrintf("Come in...and the h is: %d, and it's jumpin to %d ...\n", h, current_create);
				longjmp(thread[current_create].env, current_create);
			} else {
				// when yielded back, run the function
				if (h == 11) h = 0;
				//DPrintf ("Thread %d is running its function...\n", h);
				thread[h].f(thread[h].p);
				MyExitThread();
			}
		}
		// run when called init
		if (i == 0) { 
			thread[i].valid = 1; 
		} else {
			thread[i].valid = 0; 
		}
		thread[i].prev = -1;
		thread[i].next = -1;
	}
	/*//Printf("Thread 0 is initialized...\n");
	thread[0].valid = 1;			// initialize thread 0
	thread[0].prev = -1;
	thread[0].next = -1;*/

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
	int i, cur, me, m, count;
	if (! MyInitThreadsCalled) {
		Printf ("CreateThread: Must call InitThreads first\n");
		Exit ();
	}
	// get current running thread
	current_create = MyGetThread();
	//DPrintf ("current thread is: %d...\n", current_create);
	count = 10;
	if (setjmp(thread[current_create].env) == 0) {
		// find the available postion to create thread
		while (1) {
			if (thread[next_avail].valid) {
				next_avail = (next_avail+1)%10;
			} 
			else {
				cur = next_avail;
				last_created = cur;
				next_avail = (next_avail+1)%10;
				// append to the tail of the queue
				appendToQueue(cur);
				//PrintQ();
				// mark the entry for the new thread valid
				thread[cur].valid = 1;
				thread[cur].f = func;
				thread[cur].p = param;
				//memcpy(thread[cur].env, thread[cur].clean, sizeof thread[cur].clean);
				number++;
				//DPrintf ("created thread is: %d...and how many: %d ...\n", cur, number);
				// take care the thread 0 problem
				m = cur;
				if (cur == 0) m = 11;
				longjmp(thread[cur].env, m);
			}
			if (count-- <= 0) {
				last_created = -1;
				break;
			}
		}
	} 
	//Printf ("return from create thread is: %d...\n", last_created);
	return (last_created);		// done, return new thread ID
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
	int me, g;
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
	me = MyGetThread();
	//Printf("T %d is yielding to T %d...\n", me, t);
    if ((g = setjmp (thread[me].env)) == 0) {
    	    thread[t].whoYme = me;
    	    
    	    deleteFromQueue(me);
    	    appendToQueue(me);

    	    current_run = t;
    	    //Printf("T %d is yielding to T %d...\n", me, t);
    	    //PrintQ();
            longjmp (thread[t].env, t);
    }
    return thread[g].whoYme;
}

/*	MyGetThread () returns ID of currently running thread.  
 */

int MyGetThread ()
{
	if (! MyInitThreadsCalled) {
		Printf ("GetThread: Must call InitThreads first\n");
		Exit ();
	}
	return current_run;
}

/*	MySchedThread () causes the running thread to simply give up the
 *	CPU and allow another thread to be scheduled.  Selecting which
 *	thread to run is determined here. Note that the same thread may
 * 	be chosen (as will be the case if there are no other threads).  
 */

void MySchedThread ()
{
	int task, m;
	if (! MyInitThreadsCalled) {
		Printf ("SchedThread: Must call InitThreads first\n");
		Exit ();
	}

	task = head;
	deleteFromQueue(task);
	appendToQueue(task);

	current_run = task;
	//DPrintf ("scheduled thread is: %d...\n", task);
	// take care the thread 0 problem
	m = task;
	if (task == 0) m = 11;
	longjmp(thread[task].env, m);
}


void deleteFromQueue(int me) {
	//DPrintf("delete thread %d from queue...\n", me);
	if (number > 1) { 
		if (me == head) {
			thread[ thread[me].next ].prev = -1;
			head = thread[me].next;
		} else if (tail == me) {
			thread[ thread[me].prev ].next = -1;
			tail = thread[me].prev;
		} else {
			thread[ thread[me].next ].prev = thread[me].prev;
			thread[ thread[me].prev ].next = thread[me].next;
		}
		thread[me].prev = -1;
		thread[me].next = -1;
	} else {
		thread[me].prev = -1;
		thread[me].next = -1;
		head = -1;
		tail = -1;
	}
	//PrintQ();
}

void appendToQueue(int me) {
	//DPrintf("append thread %d to tail thread %d...\n", me, tail);
	if (head == -1) {
		head = me;
		tail = me;
	} else {
		thread[tail].next = me;
		thread[me].prev = tail;
		tail = me;
	}
	//PrintQ();
}
/*	MyExitThread () causes the currently running thread to exit.  
 */

void MyExitThread ()
{
	int me = MyGetThread();
	if (! MyInitThreadsCalled) {
		Printf ("ExitThread: Must call InitThreads first\n");
		Exit ();
	}

	deleteFromQueue(me);
	//DPrintf("Deleting thread %d...\n", me);
	thread[me].valid = 0;
	memcpy(thread[me].env, thread[me].clean, sizeof thread[me].clean);
	//DPrintf("Deleting thread %d...\n", me);
	--number;
	//DPrintf("%d threads are running...\n", number);
	//PrintQ();
	if (number > 0) {
		MySchedThread();
	} 
	else {
		Exit ();
	}
}

void PrintQ() {
	int idx = head;
	DPrintf("Queue is like: head");
	while (idx != -1) {
		DPrintf("| %d ", idx);
		idx = thread[idx].next;
	}
	DPrintf("tail is: %d \n", tail);
}
