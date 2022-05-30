#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <byteswap.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>

#include "common.h"

int is_ascii_ptr(uint64_t address, arch_t arch);
int get_arch_pointer_size(arch_t arch);
uint64_t read_pointer(arch_t arch, endianness_t endian, unsigned char *p_content, unsigned int offset);
double entropy(unsigned char *p_data, int size);

void progress_bar(int current, int max, char *desc);
void progress_bar_done(void);
