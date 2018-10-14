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
  thread_t *holder;
  queue<thread_t> waiters;
  map< unsigned int, queue<thread_t> > signalWaiters;
} lock_t;

queue<thread_t*> ready;
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

static int run_func(thread_t *t){
  interrupt_disable();
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
  if(ready.empty()){
    cout << "thread library exiting" << endl;
    exit;
  }else if{
    thread_t *temp = ready.front;
    ready.pop;
    setcontext(&temp->uc);
  }
  interrupt_enable;
}



int thread_libinit(thread_startfunc_t func, void *arg){
  interrupt_disable();
  create_thread(func, arg);
  interrupt_enable();
  run_func(ready.front);
}

int thread_create(thread_startfunc_t func, void *arg){
  interrupt_disable();
  create_thread(func, arg);
  interrupt_enable();
}

int thread_yield(void){
  interrupt_disable();
  if(ready.empty()){
    return;
  }
  
  thread_t *thread = new thread_t;
  getcontext(&thread->uc);

  thread->stack = new char [STACK_SIZE];
  thread->uc.uc_stack.ss_sp = thread->stack;
  thread->uc.uc_stack.ss_size = STACK_SIZE;
  thread->uc.uc_stack.ss_flags = 0;
  thread->uc.uc_link = NULL;
  
  makecontext(&thread->uc, (void (*) ()) func, 1, arg);

  ready.push(thread);
  thread_t *temp = ready.front();
  ready.pop();
  swapcontext(thread,temp);
  interrupt_enable();
}

int thread_lock(unsigned int lock){

}

int thread_unlock(unsigned int lock){

}

int thread_wait(unsigned int lock, unsigned int cond){

}

int thread_signal(unsigned int lock, unsigned int cond){

}

int thread_broadcast(unsigned int lock, unsigned int cond){

}
