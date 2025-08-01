//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "keystone.h"
#include "keystone-sbi.h"
#include "keystone_user.h"
#include <asm/sbi.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/kernel.h> 
#include <linux/io.h>     

// Acceso a memoria desde buffer
/* void *utm_kernel;
size_t utm_size;
uintptr_t utm_paddr;
void __iomem *mapped_utm; */

int __keystone_destroy_enclave(unsigned int ueid);

int keystone_create_enclave(struct file *filep, unsigned long arg)
{
  /* create parameters */
  struct keystone_ioctl_create_enclave *enclp = (struct keystone_ioctl_create_enclave *) arg;

  struct enclave *enclave;
  enclave = create_enclave(enclp->min_pages);

  if (enclave == NULL) {
    return -ENOMEM;
  }

  /* Pass base page table */
  enclp->epm_paddr = enclave->epm->pa;
  enclp->epm_size = enclave->epm->size;

  /* allocate UID */
  enclp->eid = enclave_idr_alloc(enclave);

  filep->private_data = (void *) enclp->eid;

  return 0;
}


int keystone_finalize_enclave(unsigned long arg)
{
  struct sbiret ret;
  struct enclave *enclave;
  struct utm *utm;
  struct keystone_sbi_create_t create_args;

  struct keystone_ioctl_create_enclave *enclp = (struct keystone_ioctl_create_enclave *) arg;

  enclave = get_enclave_by_id(enclp->eid);
  if(!enclave) {
    keystone_err("invalid enclave id\n");
    return -EINVAL;
  }

  enclave->is_init = false;

  /* SBI Call */
  create_args.epm_region.paddr = enclave->epm->pa;
  create_args.epm_region.size = enclave->epm->size;

  utm = enclave->utm;

  if (utm) {
    create_args.utm_region.paddr = __pa(utm->ptr);
    create_args.utm_region.size = utm->size;
  } else {
    create_args.utm_region.paddr = 0;
    create_args.utm_region.size = 0;
  }

  // physical addresses for runtime, user, and freemem
  create_args.runtime_paddr = enclp->runtime_paddr;
  create_args.user_paddr = enclp->user_paddr;
  create_args.free_paddr = enclp->free_paddr;
  create_args.free_requested = enclp->free_requested;

  ret = sbi_sm_create_enclave(&create_args);

  if (ret.error) {
    keystone_err("keystone_create_enclave: SBI call failed with error code %ld\n", ret.error);
    goto error_destroy_enclave;
  }

  enclave->eid = ret.value;

  return 0;

error_destroy_enclave:
  /* This can handle partial initialization failure */
  destroy_enclave(enclave);

  return -EINVAL;

}

int keystone_run_enclave(unsigned long data)
{
  struct sbiret ret;
  unsigned long ueid;
  struct enclave* enclave;
  struct keystone_ioctl_run_enclave *arg = (struct keystone_ioctl_run_enclave*) data;

  ueid = arg->eid;
  enclave = get_enclave_by_id(ueid);

  if (!enclave) {
    keystone_err("invalid enclave id\n");
    return -EINVAL;
  }

  if (enclave->eid < 0) {
    keystone_err("real enclave does not exist\n");
    return -EINVAL;
  }

  ret = sbi_sm_run_enclave(enclave->eid);
 
// Acceso a memoria desde buffer
  /* printk(KERN_INFO "Reading buffer from kernel: ");

  mapped_utm = ioremap(utm_paddr, utm_size);
  if (!mapped_utm) {
    pr_err("Error: ioremap failed\n");
    return -ENOMEM;
  }
  printk(KERN_INFO "mapped_utm: %p\n", mapped_utm);
  

  unsigned char h = readb(mapped_utm + 0x30);
  printk(KERN_INFO "Read byte at offset 0x30: %02x\n", h);
  h = h+1;
  writeb(h, mapped_utm + 0x30);

  
  #define READ_SIZE 240
  char buffer[READ_SIZE + 1];

  printk(KERN_INFO "UTM content (hex):\n");
  for (int i = 0; i < 2; i++) {
    memcpy_fromio(buffer, mapped_utm + i * READ_SIZE, READ_SIZE);
    buffer[READ_SIZE] = '\0';
    printk(KERN_INFO "---- Dump %d (offset 0x%x) ----\n", i, i * READ_SIZE);
    print_hex_dump(KERN_INFO, "  ", DUMP_PREFIX_OFFSET, 16, 1, buffer, READ_SIZE, false);
  } */

  arg->error = ret.error;
  arg->value = ret.value;

  return 0;
}

int utm_init_ioctl(struct file *filp, unsigned long arg)
{
  int ret = 0;
  struct utm *utm;
  struct enclave *enclave;
  struct keystone_ioctl_create_enclave *enclp = (struct keystone_ioctl_create_enclave *) arg;
  long long unsigned untrusted_size = enclp->utm_size;

  enclave = get_enclave_by_id(enclp->eid);

  if(!enclave) {
    keystone_err("invalid enclave id\n");
    return -EINVAL;
  }

  utm = kmalloc(sizeof(struct utm), GFP_KERNEL);
  if (!utm) {
    ret = -ENOMEM;
    return ret;
  }

  ret = utm_init(utm, untrusted_size);

  /* prepare for mmap */
  enclave->utm = utm;

  enclp->utm_paddr = __pa(utm->ptr);

// Acceso a memoria desde buffer
  /* utm_kernel = utm->ptr;
  utm_size = utm->size;
  utm_paddr = enclp->utm_paddr;

  printk(KERN_INFO "utm physical address 0x%lx\n", utm_paddr);
  printk(KERN_INFO "utm size %zu\n", utm_size);
 */
  return ret;
}


int keystone_destroy_enclave(struct file *filep, unsigned long arg)
{
  int ret;
  struct keystone_ioctl_create_enclave *enclp = (struct keystone_ioctl_create_enclave *) arg;
  unsigned long ueid = enclp->eid;

  ret = __keystone_destroy_enclave(ueid);
  if (!ret) {
    filep->private_data = NULL;
  }
  return ret;
}

int __keystone_destroy_enclave(unsigned int ueid)
{
  struct sbiret ret;
  struct enclave *enclave;
  enclave = get_enclave_by_id(ueid);

  if (!enclave) {
    keystone_err("invalid enclave id\n");
    return -EINVAL;
  }

  if (enclave->eid >= 0) {
    ret = sbi_sm_destroy_enclave(enclave->eid);
    if (ret.error) {
      keystone_err("fatal: cannot destroy enclave: SBI failed with error code %ld\n", ret.error);
      return -EINVAL;
    }
  } else {
    keystone_warn("keystone_destroy_enclave: skipping (enclave does not exist)\n");
  }


  destroy_enclave(enclave);
  enclave_idr_remove(ueid);

  return 0;
}

int keystone_resume_enclave(unsigned long data)
{
  struct sbiret ret;
  struct keystone_ioctl_run_enclave *arg = (struct keystone_ioctl_run_enclave*) data;
  unsigned long ueid = arg->eid;
  struct enclave* enclave;
  enclave = get_enclave_by_id(ueid);

  if (!enclave)
  {
    keystone_err("invalid enclave id\n");
    return -EINVAL;
  }

  if (enclave->eid < 0) {
    keystone_err("real enclave does not exist\n");
    return -EINVAL;
  }

  ret = sbi_sm_resume_enclave(enclave->eid);

// Acceso a memoria desde buffer
  /* unsigned char h = readb(mapped_utm);
  h = h+1;
  writeb(h, mapped_utm);

  #define READ_SIZE 240
  char buffer[READ_SIZE + 1];

  printk(KERN_INFO "UTM content (hex):\n");
  for (int i = 0; i < 2; i++) {
    memcpy_fromio(buffer, mapped_utm + i * READ_SIZE, READ_SIZE);
    buffer[READ_SIZE] = '\0';
    printk(KERN_INFO "---- Dump %d (offset 0x%x) ----\n", i, i * READ_SIZE);
    print_hex_dump(KERN_INFO, "  ", DUMP_PREFIX_OFFSET, 16, 1, buffer, READ_SIZE, false);
  }
 */
  arg->error = ret.error;
  arg->value = ret.value;

  return 0;
}

long keystone_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
  long ret;
  char data[512];

  size_t ioc_size;

  if (!arg)
    return -EINVAL;

  ioc_size = _IOC_SIZE(cmd);
  ioc_size = ioc_size > sizeof(data) ? sizeof(data) : ioc_size;

  if (copy_from_user(data,(void __user *) arg, ioc_size))
    return -EFAULT;

  switch (cmd) {
    case KEYSTONE_IOC_CREATE_ENCLAVE:
      ret = keystone_create_enclave(filep, (unsigned long) data);
      break;
    case KEYSTONE_IOC_FINALIZE_ENCLAVE:
      ret = keystone_finalize_enclave((unsigned long) data);
      break;
    case KEYSTONE_IOC_DESTROY_ENCLAVE:
      ret = keystone_destroy_enclave(filep, (unsigned long) data);
      break;
    case KEYSTONE_IOC_RUN_ENCLAVE:
      ret = keystone_run_enclave((unsigned long) data);
      break;
    case KEYSTONE_IOC_RESUME_ENCLAVE:
      ret = keystone_resume_enclave((unsigned long) data);
      break;
    /* Note that following commands could have been implemented as a part of ADD_PAGE ioctl.
     * However, there was a weird bug in compiler that generates a wrong control flow
     * that ends up with an illegal instruction if we combine switch-case and if statements.
     * We didn't identified the exact problem, so we'll have these until we figure out */
    case KEYSTONE_IOC_UTM_INIT:
      ret = utm_init_ioctl(filep, (unsigned long) data);
      break;
    default:
      return -ENOSYS;
  }

  if (copy_to_user((void __user*) arg, data, ioc_size))
    return -EFAULT;

  return ret;
}

int keystone_release(struct inode *inode, struct file *file) {
  unsigned long ueid = (unsigned long)(file->private_data);
  struct enclave *enclave;

  /* enclave has been already destroyed */
  if (!ueid) {
    return 0;
  }

  /* We need to send destroy enclave just the eid to close. */
  enclave = get_enclave_by_id(ueid);

  if (!enclave) {
    /* If eid is set to the invalid id, then we do not do anything. */
    return -EINVAL;
  }
  if (enclave->close_on_pexit) {
    return __keystone_destroy_enclave(ueid);
  }
  return 0;
}
