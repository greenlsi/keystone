#include <stdio.h>

int main()
{
  printf("hello, world!\n");

  // [runtime] non-handlable interrupt/exception at 0x105b8 on 0x0 (scause: 0x3)
  // int a = 10;
  // int b = 0;
  // int c = 0;

  // c = a/b;
  // printf("c = %d\n", c);

  // [runtime] page fault at 0x105c0 on 0xffffffffffffffff (scause: 0xf)
  int *ptr = 0xFFFFFFFFFFFFFFFFFFFF;
  *ptr = 0x1234;

  printf("hello, world!\n");
  
  return 0;
}
