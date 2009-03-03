#include <stdlib.h>
int main(int argc, const char **argv) {
  int c = atoi(argv[1]);
  int r;
  switch(c){
    case 0:
      r = 9;
      break;
    case 1:
      r = 8;
      break;
    case 2:
      r = 7;
      break;
    case 3:
      r = 6;
      break;
    case 4:
      r = 5;
      break;
    case 5:
      r = 4;
      break;
    case 6:
      r = 3;
      break;
  }
  return c+r;
}
