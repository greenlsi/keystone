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
#define OCALL_PRINT_STRING 1
#define OCALL_REQUEST_ENCLAVE_FUNC 2

// Host to Enclave
#define EAPP_STOP_ENCLAVE 0
#define EAPP_INC_VALUE 1
#define EAPP_RED_VALUE 2

typedef struct {
  unsigned int func_id;
  unsigned int token;  
} enclave_func_t;

typedef struct {
    unsigned int token;
    unsigned long return_data;
} data_token_t;

unsigned long return_data = 0;

unsigned int token = 0x12345678;

// CHANGE TO rt_util_getrandom FROM runtime/util/rt_util.c TO GET RANDOM FROM SBI, ENDS UP CALLING PLATFORM_RANDOM (NEED TO IMPLEMENT A SECURE HW RANDOM SOURCE)
/* Update this to generate valid entropy*/
unsigned int random_byte(unsigned int i) {
  return 0xac + (0xdd ^ i);
}



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

void ocall_request_enclave_func(void) {
  char buf[64];
  uint8_t buffer[sizeof(enclave_func_t) + sizeof(unsigned long)];

  token = random_byte(token);

  data_token_t send_struct;
  send_struct.token = token;
  send_struct.return_data = return_data;

  ocall(OCALL_REQUEST_ENCLAVE_FUNC, &send_struct, sizeof(send_struct), buffer, sizeof(buffer));

  enclave_func_t* func_ret = (enclave_func_t*)buffer;
  unsigned long received_value = *(unsigned long*)((uint8_t*)buffer + sizeof(enclave_func_t));

  if (func_ret->token != token) {
    ocall_print_string("Token not valid!");
    // exit enclave?
    // EAPP_RETURN(0);
    return;
  }

  switch (func_ret->func_id) {
    case EAPP_STOP_ENCLAVE:
      ocall_print_string("Stopping enclave...");
      EAPP_RETURN(0);
      break;
    case EAPP_INC_VALUE:
      return_data = inc_value(received_value);
      ocall_print_string("Incremented value:");
      ulong_to_str(return_data, buf);
      ocall_print_string(buf);
      break;
    case EAPP_RED_VALUE:
      return_data = red_value(received_value);
      ocall_print_string("Decremented value:");
      ulong_to_str(return_data, buf);
      ocall_print_string(buf);
      break;
    default:
      ocall_print_string("Default case, returning value:");
      ulong_to_str(return_data, buf);
      ocall_print_string(buf);
      break;
  }
  return;
} 

int main(){

  while (1) {
    ocall_request_enclave_func();
  }

  return 0;
}