#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"


int
main(int argc, char *argv[])
{
 int fd,r;
 char *path = "synctest";
 char data[2];
 data[0] = 'a';
 data[1] = 'b';

 fd = open(path, O_CREATE | O_RDWR);

 if(( r = write(fd, data, sizeof(data))) != sizeof(data)){
    printf(1,"fail\n");
    exit();
  }
  printf(1,"r : %d\n",get_log_num());


  sync();
  close(fd);

  exit();
}
