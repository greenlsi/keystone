/* Default platform does nothing special here */
#include "../../../enclave.h"
#include <sbi/sbi_string.h>
#include <sbi/sbi_console.h>
#include "platform.h"
#include "hpm-util.h"
#include "sbi/riscv_encoding.h"

void HPM_counter_setup(){
  //Enable HPM counters access
  HPM_EN_USER();

  //HPM config event
  write_csr(mhpmevent3, 7);

  //HPM remove inhibit from counter 3
  uint64_t mcountinhibit_reg = read_csr(mcountinhibit);
  write_csr(mcountinhibit, mcountinhibit_reg & ~(1UL << 3)); 
}

unsigned long platform_init_global_once(){

  HPM_counter_setup();

  return SBI_ERR_SM_ENCLAVE_SUCCESS;
}

unsigned long platform_init_global(){
  return SBI_ERR_SM_ENCLAVE_SUCCESS;
}

void platform_init_enclave(struct enclave* enclave){
  return;
}

void platform_destroy_enclave(struct enclave* enclave){
  sbi_printf("Context switch Count = %d\r\n", enclave->ped.context_switch_count);
  sbi_printf("Interrupt Count = %d\r\n", enclave->ped.total_hpm);
  return;
}

unsigned long platform_create_enclave(struct enclave* enclave){
  enclave->ped.context_switch_count = 0;
  enclave->ped.initial_hpm = 0;
  enclave->ped.total_hpm = 0;
  return SBI_ERR_SM_ENCLAVE_SUCCESS;
}

void platform_switch_to_enclave(struct enclave* enclave){
  enclave->ped.initial_hpm = (unsigned int) HPM_READ(mhpmcounter3);
  return;
}

void platform_switch_from_enclave(struct enclave* enclave){
  enclave->ped.context_switch_count++;
  enclave->ped.total_hpm += HPM_READ(mhpmcounter3) - enclave->ped.initial_hpm;
  return;
}

uint64_t platform_random(){
#pragma message("Platform has no entropy source, this is unsafe. TEST ONLY")
  static uint64_t w = 0, s = 0xb5ad4eceda1ce2a9;

  unsigned long cycles;
  asm volatile ("rdcycle %0" : "=r" (cycles));

  // from Middle Square Weyl Sequence algorithm
  uint64_t x = cycles;
  x *= x;
  x += (w += s);
  return (x>>32) | (x<<32);
}

// Initialization functions
extern byte sanctum_sm_hash[MDSIZE];
extern byte sanctum_sm_signature[SIGNATURE_SIZE];
extern byte sanctum_sm_secret_key[PRIVATE_KEY_SIZE];
extern byte sanctum_sm_public_key[PUBLIC_KEY_SIZE];
extern byte sanctum_dev_public_key[PUBLIC_KEY_SIZE];

extern byte sm_hash[MDSIZE];
extern byte sm_signature[SIGNATURE_SIZE];
extern byte sm_public_key[PUBLIC_KEY_SIZE];
extern byte sm_private_key[PRIVATE_KEY_SIZE];
extern byte dev_public_key[PUBLIC_KEY_SIZE];

void sm_copy_key(void)
{
  sbi_memcpy(sm_hash, sanctum_sm_hash, MDSIZE);
  sbi_memcpy(sm_signature, sanctum_sm_signature, SIGNATURE_SIZE);
  sbi_memcpy(sm_public_key, sanctum_sm_public_key, PUBLIC_KEY_SIZE);
  sbi_memcpy(sm_private_key, sanctum_sm_secret_key, PRIVATE_KEY_SIZE);
  sbi_memcpy(dev_public_key, sanctum_dev_public_key, PUBLIC_KEY_SIZE);
}
