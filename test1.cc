#include "thread.h"
#include <iostream>
using namespace std;

int count = 0;

void test(void *a){
  cout << "mah nose" << endl;
  thread_lock(1);
  cout << "got lock 1" << endl;
  thread_yield();
  cout << "unlocking 1" << endl;
  thread_unlock(1);
}

void test2(void *a){
  cout << "trying to get lock 2" << endl;
  thread_lock(1);
  cout << "got lock 2" << endl;
  cout << "unlocking 2" << endl;
  thread_unlock(1);
}

void initializer(void *a){
  cout << "in hurr" << endl;
  thread_create((thread_startfunc_t) test, (void *)a);
  thread_create((thread_startfunc_t) test2, (void *)a);
}

int main(){
  thread_libinit((thread_startfunc_t) initializer, (void *)1);
  return 0;
}
