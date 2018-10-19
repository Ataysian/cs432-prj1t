#include "thread.h"
//#include <assert.h>
#include <iostream>
#include <sys/time.h>
#include <sys/resource.h>
#define _XOPEN_SOURCE_EXTENDED 1

using namespace std;

void child(void *a) {

}

void parent(void *a) {
  struct rlimit limit;
  limit.rlim_cur = 512;
  limit.rlim_max = 1024;
  getrlimit(RLIMIT_AS, &limit);
  cout << limit.rlim_cur;
  limit.rlim_cur = 1024;
  setrlimit(RLIMIT_AS, &limit);

  for (int i=0; i<10000; i++) {
    if (thread_create((thread_startfunc_t) child, (void *) a)) {
      cout << "thread_create failed" << endl;
      exit(1);
    }

  }

}

int main() {
  struct rlimit limit;
  limit.rlim_cur = 512;
  limit.rlim_max = 1024;
  getrlimit(RLIMIT_AS, &limit);
  cout << limit.rlim_cur;
  void *a;
  //initialize thread library, run "parent" function, pass 5 as parameter to parent
  if (thread_libinit( (thread_startfunc_t) parent, (void *) a)) {
    cout << "thread_libinit failed" << endl;
    exit(1);
  }
}
