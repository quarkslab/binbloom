#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "arch.h"
#include "helpers.h"

/* Minimum memory region size: 4k */
#define MEMORY_REGION_MIN_SIZE  1024

/* Memory region type. */
typedef enum {
    REGION_UNKNOWN,
    REGION_CODE,
    REGION_INIT_DATA,
    REGION_UNINIT_DATA
} memregion_type_t;


/* Memory region. */
typedef struct _memregion_t {

    /* Region offset and size. */
    uint64_t offset;
    uint64_t size;

    /* Entropy. */
    double entropy;

    /* Type of data. */
    memregion_type_t type;

    /* Next region. */
    struct _memregion_t *p_next;

} memregion_t;

/* Free all regions. */
void memregion_free_all(void);

/* Add region. */
int memregion_add(uint64_t offset, uint64_t size, double entropy, memregion_type_t type);

/* Enumeration. */
memregion_t *memregion_enum_first(void);
memregion_t *memregion_enum_next(memregion_t *p_item);

void memory_analyze(void *p_data, int size, char *arch);
memregion_type_t memory_get_type(uint64_t offset);
