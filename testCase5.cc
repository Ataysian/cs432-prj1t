//test case to exploit memory by creating hella threads

#include "thread.h"
#include <iostream>
using namespace std;

int count = 0;

void test(void *a){
}

void test2(void *a){
  cout << "trying to get lock 2" << endl;
  thread_wait(1, 0);
  thread_lock(1);
  cout << "got lock 2" << endl;
  cout << "unlocking 2" << endl;
  thread_unlock(1);
}

void initializer(void *a){
  cout << "in hurr" << endl;
  
  for(int i = 0; i < 1000000; i++){
    if(thread_create((thread_startfunc_t) test, (void *)a)==-1){
      cout << "returned -1" << endl;
      break;
    }
  }
}

int main(){
  thread_libinit((thread_startfunc_t) initializer, (void *)1);
  return 0;
}
