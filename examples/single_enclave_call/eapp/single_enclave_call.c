//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "eapp_utils.h"
#include "string.h"
#include "edge_call.h"
#include <syscall.h>
#include "malloc.h"

/*---- Ocall function IDs ----*/

// Enclave to Host
#define OCALL_REQUEST_VALUE 0
#define OCALL_PRINT_STRING 1
#define OCALL_SEND_VALUE 2

void ulong_to_str(unsigned long value, char* buf) {
    char tmp[21]; // Enough for 64-bit unsigned long
    int i = 0;
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (value > 0) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }
    // Reverse the string
    for (int j = 0; j < i; j++) {
        buf[j] = tmp[i - j - 1];
    }
    buf[i] = '\0';
}

unsigned long inc_value(unsigned long value) {
    return value + 1;
}

unsigned long red_value(unsigned long value) {
    return value - 1;
}

unsigned long ocall_print_string(char* string){
  unsigned long retval;
  ocall(OCALL_PRINT_STRING, string, strlen(string)+1, &retval ,sizeof(unsigned long));
  return retval;
}

unsigned long ocall_request_value(void) {
  unsigned long retval;
  ocall(OCALL_REQUEST_VALUE, NULL, 0, &retval, sizeof(unsigned long));
  char buf[64];
  ulong_to_str(retval, buf);
  ocall_print_string(buf);
  return retval;
}

void ocall_send_value(unsigned long value) {
  ocall(OCALL_SEND_VALUE, &value, sizeof(value), NULL, 0);
  char buf[64];
  ulong_to_str(value, buf);
  ocall_print_string(buf);
  return;
}

int main(){

  unsigned long data = ocall_request_value();

  data = inc_value(data);

  ocall_print_string("Incremented value:");
  char buf[64];
  ulong_to_str(data, buf);
  ocall_print_string(buf);

  ocall_send_value(data);
  
  EAPP_RETURN(0);

  return 0;
}