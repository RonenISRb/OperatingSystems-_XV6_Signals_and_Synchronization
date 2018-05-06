#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

int 
allocpid(void) 
{
   int pid;
   pushcli();
   do{
      pid = nextpid;
   }
   while(!cas(&nextpid, pid, pid+1));
	
   popcli();
   return pid+1;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  
  //Task 4
  pushcli();
  do{
	 for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->state == UNUSED){
			break;			
		}			
	 }
	 if(p == ptable.proc + NPROC){
		popcli();
		return 0;
	 }
  } while(!cas(&p->state, UNUSED, EMBRYO));
  popcli();
  //End task 4
  
  p->pid = allocpid();

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  
    //Task 2
  for(int i = 0; i < SIG_SIZE; i++){
    p->signal_handlers[i] = (void*)SIG_DFL;
  }
  p->signal_mask = 0;
  p->pending_signals = 0;
  p->sig_stopped = 0;
  //End task 2
  
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  
  //Task 4
  pushcli();
  if(!cas(&p->state, EMBRYO, RUNNABLE)){
    panic("\t***** CAS:User init failed.");
  }
  popcli();
  //End task 4
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  //Task 2.1.2
  for(int i = 0; i < SIG_SIZE; i++){
    np->signal_handlers[i] = curproc-> signal_handlers[i];
  }
  np->signal_mask = curproc->signal_mask;
  //End task 2
  
  //Task 4
  pushcli();
  if(!cas(&np->state, EMBRYO, RUNNABLE)){
        panic("\t***** CAS:Fork failed.");
  }
  popcli();
  //End task 4

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  //Task 4
  pushcli();
  //End task 4

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  
  //Task 4
  if(!cas(&curproc->state, RUNNING, NEG_ZOMBIE)){
    panic("\t***** CAS:Exit failed.");
  }	
  //End task 4
	
  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  pushcli();
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(cas(&(p->state), ZOMBIE, UNUSED)){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        popcli();
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      popcli();
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    //task4
	pushcli();
	//end task 4
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(!cas(&(p->state), RUNNABLE, RUNNING)) {
          continue;
      }

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      swtch(&(c->scheduler), p->context);
      switchkvm();
	  
	  // Process is done running for now.
      // It should have changed its p->state before coming back.
	   c->proc = 0;
	  
	  //Task 4
	   if(cas(&p->state, NEG_SLEEPING, SLEEPING)){
          p->state = RUNNABLE;
      }
      cas(&p->state, NEG_RUNNABLE, RUNNABLE);
      if(cas(&p->state, NEG_ZOMBIE, ZOMBIE)){
        wakeup1(p->parent);
      }
	  //end task 4
    }
    popcli();
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
	//task 4
  pushcli();
  if(!cas(&(myproc()->state), RUNNING, NEG_RUNNABLE)){
       panic("\t***** CAS:Yield failed.");
  }
  sched();
  popcli();
  //end task 4
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  popcli();

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    pushcli();  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  //p->chan = chan;

  do{
	  p->chan = chan;
  }while(!cas(&p->state, RUNNING, NEG_SLEEPING));
  
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    popcli();
    acquire(lk);
  }
  //end task 4
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	  //Task 4
    if(p->chan == chan && (cas(&p->state,SLEEPING,NEG_RUNNABLE))){}
	//end task 4
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  pushcli();
  wakeup1(chan);
  popcli();
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid, int signum)
{
  if(signum < 0 || signum >= SIG_SIZE){
    return -1;
  }
	
  struct proc *p;
  
  pushcli();
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      if(!(p->state == SLEEPING && signum == SIGSTOP)){
        p->pending_signals = p->pending_signals | (1<<signum); // task 2.2.1
      }
	  popcli();
	  return 0;
    }
  }
  popcli();
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [NEG_SLEEPING] "neg_sleep",
  [RUNNABLE]  "runble",
  [NEG_RUNNABLE] "neg_runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie",
  [NEG_ZOMBIE] "neg_zombie",
  };
  
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

//Task 2
uint sigprocmask(uint sig_mask){
	struct proc *curproc = myproc();
	uint old = curproc->signal_mask;
	curproc->signal_mask = sig_mask;
	return old;
}

sighandler_t signal(int signum, sighandler_t handler){
	if(signum < 0 || signum >= SIG_SIZE){
		return (sighandler_t)-2;
	}
	
	sighandler_t result;
	struct proc *curproc = myproc();
	result = curproc->signal_handlers[signum];
	curproc->signal_handlers[signum] = handler;

	return result;
}

void sigret(void){
	struct proc *curproc = myproc();
	memmove(curproc->tf,&curproc->tf_backup,sizeof(struct trapframe));
	curproc->signal_mask = curproc->signal_mask_backup;
}

int is_masked(int mask, int signum){
	return ((1 << signum) & mask) == (1 << signum);
}

int is_signal_pending(int pending_signals, int signum){
	return ((1 << signum) & pending_signals) == (1 << signum);
}

void handle_user_signal(struct proc *p, int signum){
	sighandler_t handler = p->signal_handlers[signum];

	p->signal_mask_backup = p->signal_mask;
	memmove(&p->tf_backup, p->tf, sizeof(struct trapframe));
	
	p->signal_mask = 0xffffffff;
	p->tf->esp -= (uint)&call_sigret_end - (uint)&call_sigret_start;
	memmove((void*)p->tf->esp, call_sigret_start, (uint)&call_sigret_end - (uint)&call_sigret_start);
	*((int*)(p->tf->esp - 4)) = signum;
	*((int*)(p->tf->esp - 8)) = p->tf->esp;
	p->tf->esp -= 8;
	p->tf->eip = (uint)handler;
}

void handle_kernel_signal(struct proc *p, int signum){	
	switch(signum){
		case SIGSTOP:
			p->sig_stopped = 1;
			break;
		case SIGCONT:
			p->sig_stopped = 0;
			break;
		case SIGKILL:
		default:
			p->killed = 1;
			break;
		break;
	}
}

void handle_signals(struct trapframe *tf){
	struct proc* curproc = myproc();
	if(curproc == 0 || (tf->cs & 3) != DPL_USER){
		return;
	}
	do{
		if(curproc->sig_stopped && !is_signal_pending(curproc->pending_signals, SIGCONT)){
			yield();
		} else {
			for(int i=0; i<SIG_SIZE; i++){
				sighandler_t handler = curproc->signal_handlers[i];
				if(handler == (sighandler_t)SIG_IGN || is_masked(curproc->signal_mask, i) || !is_signal_pending(curproc->pending_signals, i)){
					continue;
				}
				if(handler == (sighandler_t)SIG_DFL){
					handle_kernel_signal(curproc, i);
				} else {
					handle_user_signal(curproc, i);
				}
				curproc->pending_signals &= ~(1UL << i);
			}
		}
	} while(curproc->sig_stopped);
}