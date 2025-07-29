//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include <edge_call.h>
#include <keystone.h>
#include <unistd.h>

/*---- Ocall function IDs ----*/

// Enclave to Host
#define OCALL_REQUEST_VALUE 0
#define OCALL_PRINT_STRING 1
#define OCALL_SEND_VALUE 2


unsigned long data_to_enclave = 0;
unsigned long data_from_enclave = 0;

unsigned long
print_string(char* str) {
  return printf("Enclave said: \"%s\"\n", str);
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

void
request_value_wrapper(void* buffer) {
  /* Parse and validate the incoming call data */
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t call_args;
  unsigned long ret_val;
  size_t arg_len;
  if (edge_call_args_ptr(edge_call, &call_args, &arg_len) != 0) {
    edge_call->return_data.call_status = CALL_STATUS_BAD_OFFSET;
    return;
  }

  printf("Sending value to enclave: %lu\n", data_to_enclave);
  ret_val = data_to_enclave;

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

void
send_value_wrapper(void* buffer) {
  /* Parse and validate the incoming call data */
  struct edge_call* edge_call = (struct edge_call*)buffer;
  uintptr_t call_args;
  size_t arg_len;
  if (edge_call_args_ptr(edge_call, &call_args, &arg_len) != 0) {
    edge_call->return_data.call_status = CALL_STATUS_BAD_OFFSET;
    return;
  }

  data_from_enclave = *(unsigned long*)call_args;
  printf("Received data from enclave: %lu\n", data_from_enclave);

  printf("Host received value from enclave: %lu\n", data_from_enclave);

  // Clear shared buffer's arg data
  memset((uint8_t*)buffer + edge_call->call_arg_offset, 0, edge_call->call_arg_size);

  /* Return to main */
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
  register_call(OCALL_REQUEST_VALUE, request_value_wrapper);
  register_call(OCALL_SEND_VALUE, send_value_wrapper);

  edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());


  data_to_enclave = 5;
  enclave.run();

  return 0;
}