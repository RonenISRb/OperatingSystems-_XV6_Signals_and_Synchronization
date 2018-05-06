#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
  int i;

  if(argc < 3){
    printf(2, "usage: kill pid...\n");
    exit();
  }
  for(i=1; i<argc; i+=2)
    kill(atoi(argv[i]), atoi(argv[i+1]));
  exit();
}
