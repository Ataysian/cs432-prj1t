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
thread_t* current_thread;
thread_t* last_thread;
map<unsigned int, lock_t*> locks;
bool libinitCalled = false;
bool first = true;

bool cleanup_req = false; //on isle four
thread_t *tobe_deleted;

static int run_next_ready(){
  interrupt_disable();
  if(ready.empty()){
    cout << "Thread library exiting." << endl;
    exit;
  }

  last_thread = current_thread;
  current_thread = ready.front();
  ready.pop();
  cout << "got here3" << endl;
  cout << ready.size() << endl;
  if(first){ //if first time (libinit)
    first = false;
    setcontext(&current_thread->uc);
  }
  else {
    swapcontext(&last_thread->uc, &current_thread->uc);
    if(cleanup_req){
      delete(tobe_deleted);
      cleanup_req = false;
    }
    interrupt_enable();
  }
}

static int start_thread(thread_startfunc_t func, void *arg){
  interrupt_enable();
  //calling function
  (*func)(arg);
  interrupt_disable();
  //deleting thread
  tobe_deleted = current_thread;
  cleanup_req = true;
  interrupt_enable();
  run_next_ready();
}

static int create_thread(thread_startfunc_t func, void *arg){
  thread_t *thread = new thread_t;
  getcontext(&thread->uc);

  thread->stack = new char [STACK_SIZE];
  thread->uc.uc_stack.ss_sp = thread->stack;
  thread->uc.uc_stack.ss_size = STACK_SIZE;
  thread->uc.uc_stack.ss_flags = 0;
  thread->uc.uc_link = NULL;
  
  makecontext(&thread->uc, (void (*) ()) start_thread, 2, func, arg);
  ready.push(thread);
}

int thread_libinit(thread_startfunc_t func, void *arg){
  interrupt_disable();
  if(!libinitCalled){
    libinitCalled = true;
    create_thread(func, arg);
    interrupt_enable();
    run_next_ready();
  }
  else {
    interrupt_enable();
    return -1;
  }
}

int thread_create(thread_startfunc_t func, void *arg){
  interrupt_disable();
  create_thread(func, arg);
  interrupt_enable();
}

int thread_yield(void){
  interrupt_disable();
  if(ready.empty()){
    interrupt_enable();
    return 0;
  }
  
  ready.push(current_thread);
  interrupt_enable();
  run_next_ready();
}

int thread_lock(unsigned int lock){
  interrupt_disable();
  if(locks.find(lock) == locks.end()){ //if lock doesn't exist create it
    lock_t *newLock = new lock_t;
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
    }
    else {
      locks.find(lock)->second->waiters.push(current_thread); //lock is unavailable: add current thread to locks waiters queue
      interrupt_enable();
      run_next_ready();
      interrupt_disable();
      locks.find(lock)->second->holder = current_thread;
      current_thread->locks.insert(lock);
      locks.find(lock)->second->available = false;
    }
  }
  else if(current_thread->locks.find(lock) != current_thread->locks.end()){ //if current thread already has lock
    interrupt_enable();
    return -1;
  }
  interrupt_enable();
}

int thread_unlock(unsigned int lock){
  interrupt_disable();
  if(current_thread->locks.find(lock) == current_thread->locks.end()){ //if current thread doesn't have lock
    interrupt_enable();
    return -1;
  }
  else if(locks.find(lock) != locks.end()) { //if lock exists
    if(locks.find(lock)->second->waiters.empty() && locks.find(lock)->second->signalWaiters.empty()){ //nobody waiting
      delete(locks.find(lock)->second);
      locks.erase(lock);
      current_thread->locks.erase(current_thread->locks.find(lock)); //remove lock from current threads list of locks
    }	
    else { // else: waiter list is not empty
      ready.push(locks.find(lock)->second->waiters.front());
      locks.find(lock)->second->waiters.pop();
      current_thread->locks.erase(current_thread->locks.find(lock)); //remove lock from current threads list of locks
      locks.find(lock)->second->available = true;
    }
  }
  else { //if lock doesn't exist
    interrupt_enable();
    return -1;
  }
  interrupt_enable();
}

int thread_wait(unsigned int lock, unsigned int cond){
  interrupt_disable();
  
  if(locks.find(lock) == locks.end()){ //if lock doesn't exist return -1
    interrupt_enable();
    return -1;
  }
  else if(current_thread->locks.find(lock) == current_thread->locks.end()){ //current thread doesn't have lock
    return -1;
  }
  else if(current_thread->locks.find(lock) != current_thread->locks.end()){ //if current thread has lock
    //if CV exists for this lock
    if(locks.find(lock)->second->signalWaiters.find(cond) != locks.find(lock)->second->signalWaiters.end()){
      //add current thread to waiting queue for lock's condition variable
      locks.find(lock)->second->signalWaiters.find(cond)->second.push(current_thread);
      //to lock or not to lock
      locks.find(lock)->second->available = true;
      interrupt_enable();
      run_next_ready();
      interrupt_disable();
      locks.find(lock)->second->available = false;
    }
    else { //CV doesn't exist, must create it and add it to signalWaiters
      queue<thread_t*> newSignalWaiters;
      newSignalWaiters.push(current_thread); //add itself to queue
      locks.find(lock)->second->signalWaiters.insert(pair< unsigned int, queue<thread_t*> >(cond, newSignalWaiters));
      locks.find(lock)->second->available = true;
      cout << "got here2" << endl;
      interrupt_enable();
      run_next_ready();
      interrupt_disable();
      cout << "got here4" << endl;
      locks.find(lock)->second->available = false;
    }
  }
  
  interrupt_enable();
}

int thread_signal(unsigned int lock, unsigned int cond){
  interrupt_disable();
  if(locks.find(lock) != locks.end()){ //if lock exist
    map< unsigned int, queue<thread_t*> > sigWaitMapHolder = locks.find(lock)->second->signalWaiters;
    if(sigWaitMapHolder.find(cond) != sigWaitMapHolder.end()){ //if condition exists
      cout << "got here 100 | " << cond << endl;
      if(!sigWaitMapHolder.find(cond)->second.empty()){ //if there is at least one waiting thread
      ready.push(sigWaitMapHolder.find(cond)->second.front());
      sigWaitMapHolder.find(cond)->second.pop();
      } //if there aren't any waiters, remove
    } //if condition doesn't exist, do nothing
  }
  else { //if lock doesn't exist
    interrupt_enable();
    return -1;
  }
  interrupt_enable();
}

int thread_broadcast(unsigned int lock, unsigned int cond){
  interrupt_disable();

  interrupt_enable();
}
