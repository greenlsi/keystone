
#ifndef _PLATFORM_H_
#define _PLATFORM_H_

// No special data needed for default platform
struct platform_enclave_data{
  int context_switch_count;
  int total_hpm;
  int initial_hpm;
};

// Enclave configuration
#define ENCL_MAX                16
#define ENCLAVE_REGIONS_MAX     8

// SM configuration
#define SMM_BASE                0x80000000
#define SMM_SIZE                0x100000

// PMP configuration
#define PMP_N_REG               4
#define PMP_MAX_N_REGION        16

// CPU configuration
#define MAX_HARTS               2


// Initialization functions
void sm_copy_key(void);

#ifndef read_csr
#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })
#endif

#ifndef write_csr
#define write_csr(reg, val) ({ \
  asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })
#endif

#endif // _PLATFORM_H_
