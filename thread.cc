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
  thread_t *holder;
  queue<thread_t> waiters;
  map< unsigned int, queue<thread_t> > signalWaiters;
} lock_t;

queue<thread_t> ready;


static int create_thread(thread_startfunc_t func, void *arg){
  thread_t *thread = new thread_t;
  ucontext_t* ucontext_ptr = &(thread->uc);
  getcontext(ucontext_ptr);
  thread->stack = new char [STACK_SIZE];
  
}




int thread_libinit(thread_startfunc_t func, void *arg){
  interrupt_disable();
  create_thread(func, arg);
  interrupt_enable();
}

int thread_create(thread_startfunc_t func, void *arg){
  interrupt_disable();
  
  interrupt_enable();
}

int thread_yield(void){

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
