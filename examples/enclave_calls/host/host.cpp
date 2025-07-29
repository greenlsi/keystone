//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include <edge_call.h>
#include <keystone.h>
#include <unistd.h>

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

enclave_func_t enclave_func;
unsigned long data;

data_token_t data_from_enclave;

/*
 * Called by host before returning execution to the enclave to set the requested
 * function ID and data to be passed to the enclave with enclave_func and data.
 */
void set_func_data(unsigned int func_id, unsigned long data_to_enclave, Keystone::Enclave* enclave) {
  enclave_func.func_id = func_id;
  enclave_func.token = data_from_enclave.token;
  data = data_to_enclave;

  printf("Sending value to enclave: %lu\n", data);

  struct edge_call* edge_call = (struct edge_call*)enclave->getSharedBuffer();

  size_t total_size = sizeof(enclave_func_t) + sizeof(data);
  uint8_t* ret_buffer = (uint8_t*)malloc(total_size);
  memcpy(ret_buffer, &enclave_func, sizeof(enclave_func_t));
  memcpy(ret_buffer + sizeof(enclave_func_t), &data, sizeof(data));

  /* Setup return data from the ocall function */
  uintptr_t data_section = edge_call_data_ptr();
  memcpy((void*)data_section, ret_buffer, total_size);

  if (edge_call_setup_ret(edge_call, (void*)data_section, total_size)) {
    edge_call->return_data.call_status = CALL_STATUS_BAD_PTR;
  } else {
    edge_call->return_data.call_status = CALL_STATUS_OK;
  }

  return;
}

unsigned long
print_string(char* str) {
  return printf("Enclave said: \"%s\"\n", str);
}

/*
 * Function called by the enclave to request a function call
 * from the host. The returned data from previous calls is 
 * stored in returned_data.
 */
void
request_enclave_func(void* buffer) {
  /* Parse and validate the incoming call data */
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t call_args;
  size_t arg_len;
  if (edge_call_args_ptr(edge_call, &call_args, &arg_len) != 0) {
    edge_call->return_data.call_status = CALL_STATUS_BAD_OFFSET;
    return;
  }

  data_from_enclave = *(data_token_t*)call_args;
  printf("Received data from enclave: %lu\n", data_from_enclave.return_data);
  printf("Received token from enclave: %u\n", data_from_enclave.token);

  // Clear shared buffer's arg data and token
  memset((uint8_t*)buffer + edge_call->call_arg_offset, 0, edge_call->call_arg_size);

  /* Return to main */
  return;
}

void
print_string_wrapper(void* buffer) {
  /* Parse and validate the incoming call data */
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t call_args;
  unsigned long ret_val;
  size_t arg_len;
  if (edge_call_args_ptr(edge_call, &call_args, &arg_len) != 0) {
    edge_call->return_data.call_status = CALL_STATUS_BAD_OFFSET;
    return;
  }

  /* Pass the arguments from the eapp to the exported ocall function */
  ret_val = print_string((char*)call_args);

  /* Setup return data from the ocall function */
  uintptr_t data_section = edge_call_data_ptr();
  memcpy((void*)data_section, &ret_val, sizeof(unsigned long));
  if (edge_call_setup_ret(
          edge_call, (void*)data_section, sizeof(unsigned long))) {
    edge_call->return_data.call_status = CALL_STATUS_BAD_PTR;
  } else {
    edge_call->return_data.call_status = CALL_STATUS_OK;
  }

  /* This will now eventually return control to the enclave */
  return;
}

int
main(int argc, char** argv) {

  Keystone::Enclave enclave;
  Keystone::Params params;

  params.setFreeMemSize(6*1024 * 1024);
  params.setUntrustedSize(4*1024 * 1024);

  enclave.init(argv[1], argv[2], argv[3], params);

  enclave.registerOcallDispatch(incoming_call_dispatch);

  /* We must specifically register functions we want to export to the
     enclave. */
  register_call(OCALL_PRINT_STRING, print_string_wrapper);
  register_call(OCALL_REQUEST_ENCLAVE_FUNC, request_enclave_func);

  edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

  uintptr_t encl_ret;
  enclave.runUntilRequest(&encl_ret);
  
  unsigned long message = 5;
  set_func_data(EAPP_INC_VALUE, message, &enclave);
  enclave.resumeUntilRequest(&encl_ret);

  message = data_from_enclave.return_data;
  set_func_data(EAPP_INC_VALUE, message, &enclave);
  enclave.resumeUntilRequest(&encl_ret);

  // while(access("/tmp/exit_enclave.flag", F_OK) != 0){
  //   sleep(1);
  // }
  
  message = data_from_enclave.return_data;
  set_func_data(EAPP_RED_VALUE, message, &enclave);
  enclave.resumeUntilRequest(&encl_ret);
 
  // Stop the enclave
  set_func_data(EAPP_STOP_ENCLAVE, NULL, &enclave);
  enclave.resumeUntilRequest(&encl_ret);

  return 0;
}