#include "mtasker.hh"
#include <iostream>
#include <sys/time.h>

using namespace std;

MTasker<> MT;
static int count = 0;
long start_time = 0;
long end_time = 0;

void printer(void *p)
{
  char c=(char)p;
  for(;;) {
	MT.yield(); //如果注释掉 则只打印出a 
	if (count == 1000000) {
	  struct timeval l_now = {0};
	  gettimeofday(&l_now, NULL);
	  end_time = ((long)l_now.tv_sec)*1000+(long)l_now.tv_usec/1000;
      std::cout<<"end_time: " << end_time<<std::endl;
	  std::cout<<count << " elapse " << end_time - start_time << std::endl;

	} else if (count == 0) {
	  struct timeval l_now = {0};
	  gettimeofday(&l_now, NULL);
	  start_time = ((long)l_now.tv_sec)*1000+(long)l_now.tv_usec/1000;
      std::cout<<"start_time: " << start_time<<std::endl;
	}
	count++;
  }

}


int main()
{
  MT.makeThread(printer,(void*)'a');
  MT.makeThread(printer,(void*)'b');

  for(;;) {
	while(MT.schedule()); // do everything we can do //开始遍历队列
  }
}
