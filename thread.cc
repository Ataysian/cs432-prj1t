#include <ucontext.h>
#include "thread.h"
#include <iostream>
#include <queue>
#include <set>
#include <map>
#include "interrupt.h"
#define STACK_SIZE 262144

using namespace std;

typedef void (*thread_startfunc_t) (void *);

typedef struct {
  ucontext_t uc;
  char *stack;
  set<unsigned int> locks;
} thread_t;

typedef struct {
  unsigned int label;
  thread_t *holder;
  bool available;
  queue<thread_t*> waiters;
  map< unsigned int, queue<thread_t*> > signalWaiters; //maps CVs to waiters
} lock_t;

queue<thread_t*> ready;
thread_t *current_thread;
thread_t *old_thread;
map<unsigned int, lock_t*> locks;
bool libinitCalled = false;
bool first = true;

bool cleanup_req = false; //on isle four
thread_t *tobe_deleted;

static int run_next_ready(){
  //cout << "ready size: " << ready.size() << endl;
  //no more ready threads, exit

  if(ready.empty()){
    cout << "Thread library exiting." << endl;
    //while(!locks.empty()){

    //}
    exit(0);
  }

  //get ready to run next ready thread
  old_thread = current_thread;
  current_thread = ready.front();
  ready.pop();

  if(first){ //if first time (libinit)
    first = false;
    setcontext(&current_thread->uc);
  }

  else { //not first time
    swapcontext(&old_thread->uc, &current_thread->uc);
  }

}

static int start_thread(thread_startfunc_t func, void *arg){

  //calling function
  interrupt_enable();
  (*func)(arg);
  interrupt_disable();


  //deleting thread
  if(tobe_deleted != NULL){
    //JEANNIE - need to delete stack and thread
    delete [] tobe_deleted->stack;
    delete tobe_deleted;

  }
  tobe_deleted = current_thread;
  run_next_ready();
}

static int create_thread(thread_startfunc_t func, void *arg){

  //JEANNIE - YOU NEED TO use a try/catch to catch bad_alloc exceptions
  //and return -1 in those cases
  thread_t *thread;
  try{
    thread = new thread_t;
    getcontext(&thread->uc);

    thread->stack = new char [STACK_SIZE];
    thread->uc.uc_stack.ss_sp = thread->stack;
    thread->uc.uc_stack.ss_size = STACK_SIZE;
    thread->uc.uc_stack.ss_flags = 0;
    thread->uc.uc_link = NULL;
    
    makecontext(&thread->uc, (void (*) ()) start_thread, 2, func, arg);
    ready.push(thread);
  }
  catch (bad_alloc &bad){
    delete thread->stack;
    delete thread;
    return -1;
  }
  return 0;
}

int thread_libinit(thread_startfunc_t func, void *arg){
  interrupt_disable();
  if(!libinitCalled){ //if libinit was only called once
    libinitCalled = true;
    if(create_thread(func, arg)){
      interrupt_enable();
      return -1;
    }
    run_next_ready();
  }
  else { //if libinit was calle already, return -1
    interrupt_enable();
    return -1;
  }
  interrupt_enable();
}

int thread_create(thread_startfunc_t func, void *arg){
  interrupt_disable();
  if(create_thread(func, arg)){
    interrupt_enable();
    return -1;
  }
  interrupt_enable();
}

int thread_yield(void){
  interrupt_disable();
  /*
  if(ready.empty()){
    interrupt_enable();
    return 0;
    }*/

  ready.push(current_thread);
  run_next_ready();
  interrupt_enable();
}

static int inner_lock(unsigned int lock){
  if(locks.find(lock) == locks.end()){ //if lock doesn't exist create it
    lock_t *newLock;
    try{
      newLock = new lock_t;
    }
    catch (bad_alloc &bad){
      delete newLock;
      return -1;
    }
    newLock->label = lock;
    newLock->holder = current_thread;
    newLock->available = false;
    //newLock->waiters = new queue<thread_t>();
    //newLock->signalWaiters = new map< unsigned int, queue<thread_t> >();
    locks.insert(pair<unsigned int, lock_t*>(lock, newLock));
    current_thread->locks.insert(lock);
  }
  else if(current_thread->locks.find(lock) == current_thread->locks.end()){ //current thread doesn't have lock
    if(locks.find(lock)->second->available){ //but lock is available
      current_thread->locks.insert(lock);
      locks.find(lock)->second->available = false;

      //JEANNIE - update holder
      locks.find(lock)->second->holder = current_thread;

    }
    else { //lock is unavailable: add current thread to locks waiters queue
      locks.find(lock)->second->waiters.push(current_thread);
      run_next_ready();
      locks.find(lock)->second->holder = current_thread;
      current_thread->locks.insert(lock);
      locks.find(lock)->second->available = false;
    }
  }
  else if(current_thread->locks.find(lock) != current_thread->locks.end()){ //if current thread already has lock
    return -1;
  }
  //JEANNIE
  return 0;
}

int inner_unlock(unsigned int lock){
  if(current_thread->locks.find(lock) == current_thread->locks.end()){ //if current thread doesn't have lock
    return -1;
  }
  else if(locks.find(lock) != locks.end()) { //if lock exists
    if(locks.find(lock)->second->waiters.empty()){ //nobody waiting for lock
      if(locks.find(lock)->second->signalWaiters.empty()){ //nobody waiting for signal
	delete(locks.find(lock)->second);
	//delete locks[lock];
	locks.erase(lock);
	current_thread->locks.erase(current_thread->locks.find(lock)); //remove lock from current threads list of locks
      }
      else{ //nobody waiting for lock, but somebody waiting for signal
	current_thread->locks.erase(current_thread->locks.find(lock));
	locks.find(lock)->second->available = true;
      }
    }
    else { // else: waiter list is not empty
      //JEANNIE - need to handoff to waiting thread
      lock_t *thelock = (locks.find(lock)->second);
      thread_t *next = thelock->waiters.front();
      thelock->holder = next;
      thelock->available = false;

      ready.push(locks.find(lock)->second->waiters.front());
      locks.find(lock)->second->waiters.pop();
      //current_thread->locks.erase(current_thread->locks.find(lock)); //remove lock from current threads list of locks
      current_thread->locks.erase(lock);

      //JEANNIE - it's not available, you gave it to next thread
      //locks.find(lock)->second->available = true;

    }
  }
  else { //if lock doesn't exist
    return -1;
  }
  //JEANNIE
  return 0;
}

int thread_lock(unsigned int lock){
  interrupt_disable();
  int temp = inner_lock(lock);
  interrupt_enable();
  return temp;
}

int thread_unlock(unsigned int lock){
  interrupt_disable();
  int temp = inner_unlock(lock);
  interrupt_enable();
  return temp;
}

int thread_wait(unsigned int lock, unsigned int cond){
  interrupt_disable();

  if(locks.find(lock) == locks.end()){ //if lock doesn't exist
    interrupt_enable();
    return -1;
  }
  else if(current_thread->locks.find(lock) == current_thread->locks.end()){ //current thread doesn't have lock
    interrupt_enable();
    return -1;
  }
  else if(current_thread->locks.find(lock) != current_thread->locks.end()){ //if current thread has lock
    //if CV exists for this lock
    if(locks.find(lock)->second->signalWaiters.find(cond) != locks.find(lock)->second->signalWaiters.end()){
      //add current thread to waiting queue for lock's condition variable
      locks.find(lock)->second->signalWaiters.find(cond)->second.push(current_thread);

      //JEANNIE - doesn't the next 2 lines happen in unlock?
      locks.find(lock)->second->available = true;
      locks.find(lock)->second->holder = NULL;
      inner_unlock(lock);
      run_next_ready();
      inner_lock(lock);
      locks.find(lock)->second->available = false;
    }
    else { //CV doesn't exist, must create it and add it to signalWaiters
      queue<thread_t*> newSignalWaiters;
      newSignalWaiters.push(current_thread); //add itself to queue
      locks.find(lock)->second->signalWaiters.insert(pair< unsigned int, queue<thread_t*> >(cond, newSignalWaiters));
      locks.find(lock)->second->available = true;
      locks.find(lock)->second->holder = NULL;
      inner_unlock(lock);
      run_next_ready();
      inner_lock(lock);
      locks.find(lock)->second->available = false;
    }
  }
  interrupt_enable();
  //JEANNIE
  return 0;
}

int thread_signal(unsigned int lock, unsigned int cond){
  interrupt_disable();
  if(locks.find(lock) != locks.end()){ //if lock exists

    map< unsigned int, queue<thread_t*> > sigWaitMapHolder = locks.find(lock)->second->signalWaiters;
    if(sigWaitMapHolder.find(cond) != sigWaitMapHolder.end()){ //if condition exists
      if(!sigWaitMapHolder.find(cond)->second.empty()){ //if there is at least one waiting thread

	//JEANNIE - this is your main problem.  sigWaitMapHolder is a local variable, not a pointer
	//to the "real" map that you need to update
	ready.push(sigWaitMapHolder.find(cond)->second.front());
	sigWaitMapHolder.find(cond)->second.pop();

	//JEANNIE - pop from the *real* map (also, you don't need to keep using find)
	locks[lock]->signalWaiters[cond].pop();

      } //if there aren't any waiters, remove
    }
  }
  else { //if lock doesn't exist DONT RETURN IN ERROR, CREATE THE LOCK AND CV
    //    cout << "lock didn't exist" << endl;
  }
  interrupt_enable();
  //JEANNIE
  return 0;
}
//does for all threads on the waiting queue
int thread_broadcast(unsigned int lock, unsigned int cond){
  interrupt_disable();
  if(locks.find(lock)!= locks.end()){ //if lock exists
    map< unsigned int, queue<thread_t*> > sigWaitMapHolder = locks.find(lock)->second->signalWaiters;
    if(sigWaitMapHolder.find(cond) != sigWaitMapHolder.end()){ //if condition exists
      while(!sigWaitMapHolder.find(cond)->second.empty()){ //while there is at least one waiting thread
	ready.push(sigWaitMapHolder.find(cond)->second.front());
	sigWaitMapHolder.find(cond)->second.pop();
      }
    }
  }
  interrupt_enable();
  //JEANNIE
  return 0;
}
