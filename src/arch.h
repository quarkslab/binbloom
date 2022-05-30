#pragma once

#include <stdlib.h>
#include <string.h>
#include "common.h"

typedef struct {
    char *psz_name;
    arch_t arch;
    double ent_uninit_data_min;
    double ent_uninit_data_max;
    double ent_data_min;
    double ent_data_max;
    double ent_code_min;
    double ent_code_max;
} arch_info_t;

arch_info_t *arch_get_info(char *psz_name);