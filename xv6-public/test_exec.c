#include "types.h"
#include "stat.h"
#include "user.h"


int main(int argc, char *argv[])
{

 char *str[] = {"getppid()", 0};

 exec(str[0], str);
 


 exit();

}



