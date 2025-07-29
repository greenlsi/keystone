#include "app/syscall.h"
#include "edge/edge_common.h"
#include "app/eapp_utils.h"
#include <stdio.h>


#define OCALL_PRINT_BUFFER 1
#define OCALL_PRINT_VALUE 2 
#define OCALL_GET_STRING 3

unsigned long ocall_print_buffer(char *data, size_t data_len)
{
  unsigned long retval;

  ocall(OCALL_PRINT_BUFFER, data, data_len, &retval ,sizeof(unsigned long));

  return retval;
}

int main()
{
 // printf("hello, world!\n");

  struct edge_data retdata;
  ocall(OCALL_GET_STRING, NULL, 0, &retdata, sizeof(struct edge_data));

  
  for (unsigned long i = 1; i <= 10001; i++) {
    if (i % 5000 == 0) {
      ocall(OCALL_PRINT_VALUE, &i, sizeof(unsigned long), 0, 0);
    }
  }

  ocall_print_buffer("hello, world!\n", 14);

  EAPP_RETURN(0);
}
