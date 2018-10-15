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
  thread_startfunc_t func;
  void *arg;
} thread_t;

typedef struct {
  unsigned int label;
  thread_t *holder;
  queue<thread_t*> waiters;
  map< unsigned int, queue<thread_t> > signalWaiters; //maps CVs to waiters
} lock_t;

queue<thread_t*> ready;
thread_t* current_thread;
map<unsigned int, lock_t*> locks;
//thread_t *tobe_deleted;

static int create_thread(thread_startfunc_t func, void *arg){
  thread_t *thread = new thread_t;
  getcontext(&thread->uc);

  thread->func = func;
  thread->arg = arg;
  thread->stack = new char [STACK_SIZE];
  thread->uc.uc_stack.ss_sp = thread->stack;
  thread->uc.uc_stack.ss_size = STACK_SIZE;
  thread->uc.uc_stack.ss_flags = 0;
  thread->uc.uc_link = NULL;
  
  makecontext(&thread->uc, (void (*) ()) func, 1, arg);
  ready.push(thread);  
}

static int run_next_ready(){
  if(ready.empty()){
    cout << "thread library exiting" << endl;
    exit;
  } else {
    thread_t *temp = ready.front();
    ready.pop();
    swapcontext(&current_thread->uc, &temp->uc);
  }
}

static int run_func(thread_t *t){
  interrupt_disable();
  current_thread = t;
  /*if(*tobe_deleted != null){
    delete tobe_deleted;
    }*/
  interrupt_enable();
  
  (t->func)(t->arg);

  interrupt_disable();
  /*if(*tobe_deleted != null){
    delete tobe_delted;
    tobe_delted = t;
    }*/
  delete t;
  run_next_ready();
  interrupt_enable;
}


int thread_libinit(thread_startfunc_t func, void *arg){
  interrupt_disable();
  create_thread(func, arg);
  interrupt_enable();
  run_func(ready.front());
}

int thread_create(thread_startfunc_t func, void *arg){
  interrupt_disable();
  create_thread(func, arg);
  interrupt_enable();
}

int thread_yield(void){
  interrupt_disable();
  if(ready.empty()){
    return 0;
  }
 
  ready.push(current_thread);
  thread_t *temp = ready.front();
  ready.pop();
  swapcontext(&current_thread->uc,&temp->uc);
  interrupt_enable();
}

int thread_lock(unsigned int lock){
  interrupt_disable();
  if(locks.find(lock) == locks.end()){ //if lock doesn't exist create it
    lock_t *newLock = new lock_t;
    newLock->label = lock;
    newLock->holder = current_thread;
    //newLock->waiters = new queue<thread_t>();
    //newLock->signalWaiters = new map< unsigned int, queue<thread_t> >();
    locks.insert(pair<unsigned int, lock_t*>(lock, newLock));
    current_thread->locks.insert(lock);
  }
  else if(current_thread->locks.find(lock) == current_thread->locks.end()){ //current thread doesn't have lock
    locks.find(lock)->second->waiters.push(current_thread); //add current thread to locks waiters queue
    run_next_ready();
    locks.find(lock)->second->holder = current_thread;
    current_thread->locks.insert(lock);
  }
  interrupt_enable();
}

int thread_unlock(unsigned int lock){
  if(current_thread->locks.find(lock) == current_thread->locks.find(lock).end()){ //if current thread doesn't have lock
    cout << "FAK U" << endl;
  }
  else if(locks.find(lock) != locks.end()) { //if lock exists
    if(locks.find(lock)->second->waiters.empty() && locks.find(lock)->second->signalWaiters.empty()){ // nobody waiting
      delete(locks.find(lock)->second);
      locks.erase(lock);
    }	
    else { // else: waiter list is not empty
      ready.push(locks.find(lock)->second->waiters.first());
      locks.find(lock)->second->waiters.pop();
    }
  }
  else { //if lock doesn't exist
    
  }
}

int thread_wait(unsigned int lock, unsigned int cond){

}

int thread_signal(unsigned int lock, unsigned int cond){

}

int thread_broadcast(unsigned int lock, unsigned int cond){

}
