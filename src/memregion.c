#include "memregion.h"

memregion_t *g_regions = NULL;

/**
 * Create memory region
 **/

memregion_t *memregion_create(uint64_t offset, uint64_t size, double entropy, memregion_type_t type)
{
    memregion_t *p_region;

    p_region = (memregion_t *)malloc(sizeof(memregion_t));
    if (p_region != NULL)
    {
        p_region->offset = offset;
        p_region->size = size;
        p_region->entropy = entropy;
        p_region->type = type;
        p_region->p_next = NULL;
    }

    return p_region;
}

void memregion_free_all(void)
{
    memregion_t *p_item, *p_next_item;

    p_item = memregion_enum_first();
    while (p_item != NULL)
    {
        p_next_item = memregion_enum_next(p_item);
        free(p_item);
        p_item = p_next_item;
    }
}

/**
 * Add memory region.
 **/

int memregion_add(uint64_t offset, uint64_t size, double entropy, memregion_type_t type)
{
    memregion_t *p_region;

    /* Create memory region. */
    p_region = memregion_create(offset, size, entropy, type);
    if (p_region != NULL)
    {
        if (g_regions != NULL)
            p_region->p_next = g_regions;
        g_regions = p_region;

        /* Success. */
        return 0;
    }

    /* Error. */
    return -1;
}

/**
 * Start enumeration.
 **/

memregion_t *memregion_enum_first(void)
{
    return g_regions;
}


/**
 * Continue enumeration.
 **/

memregion_t *memregion_enum_next(memregion_t *p_item)
{
    if (p_item != NULL)
        return p_item->p_next;
    else
        return NULL;
}

/**
 * Analyze a memory dump:
 *  - detect regions with same entropy
 *  - classify regions (code, initialized data, uninitialized data) based on architecture
 **/

void memory_analyze(void *p_data, int size, char *arch)
{
    int nsections, i;
    double ent;
    arch_info_t *p_arch;
    memregion_type_t prev_region_type = REGION_UNKNOWN;
    unsigned int region_start = 0;
    unsigned int region_size;

    p_arch = arch_get_info(arch);

    if (p_arch != NULL)
    {
        nsections = size/MEMORY_REGION_MIN_SIZE;
        for (i=0; i<nsections;i++)
        {
            ent = entropy((unsigned char *)p_data + i*MEMORY_REGION_MIN_SIZE, ((size - i*MEMORY_REGION_MIN_SIZE)<MEMORY_REGION_MIN_SIZE)?(size - i*MEMORY_REGION_MIN_SIZE):MEMORY_REGION_MIN_SIZE);
            if ((ent >= p_arch->ent_uninit_data_min) && (ent < p_arch->ent_uninit_data_max))
            {
                /* Do we have a new region ? */
                if (prev_region_type == REGION_UNKNOWN)
                {
                    /* Store offset and current size. */
                    region_start = i*MEMORY_REGION_MIN_SIZE;
                    region_size = MEMORY_REGION_MIN_SIZE;
                    prev_region_type = REGION_UNINIT_DATA;
                }
                else
                {
                    /* Are we at the edge of a new region ? */
                    if (prev_region_type != REGION_UNINIT_DATA)
                    {
                        /* Recompute entropy. */
                        ent = entropy((unsigned char *)p_data + region_start, region_size);

                        /* Register previous region. */
                        memregion_add(region_start, region_size, ent, prev_region_type);

                        /* Keep track of new region. */
                        region_start = i*MEMORY_REGION_MIN_SIZE;
                        region_size = MEMORY_REGION_MIN_SIZE;
                        prev_region_type = REGION_UNINIT_DATA;
                    }
                    else
                    {
                        /* Same region, add this one to the previous one. */
                        region_size += MEMORY_REGION_MIN_SIZE;
                    }
                }
            }
            else if ((ent >= p_arch->ent_data_min) && (ent < p_arch->ent_data_max))
            {
                /* Do we have a new region ? */
                if (prev_region_type == REGION_UNKNOWN)
                {
                    /* Store offset and current size. */
                    region_start = i*MEMORY_REGION_MIN_SIZE;
                    region_size = MEMORY_REGION_MIN_SIZE;
                    prev_region_type = REGION_INIT_DATA;
                }
                else
                {
                    /* Are we at the edge of a new region ? */
                    if (prev_region_type != REGION_INIT_DATA)
                    {
                        /* Recompute entropy. */
                        ent = entropy((unsigned char *)p_data + region_start, region_size);

                        /* Register previous region. */
                        memregion_add(region_start, region_size, ent, prev_region_type);

                        /* Keep track of new region. */
                        region_start = i*MEMORY_REGION_MIN_SIZE;
                        region_size = MEMORY_REGION_MIN_SIZE;
                        prev_region_type = REGION_INIT_DATA;
                    }
                    else
                    {
                        /* Same region, add this one to the previous one. */
                        region_size += MEMORY_REGION_MIN_SIZE;
                    }
                }
            }
            else if ((ent >= p_arch->ent_code_min) && (ent < p_arch->ent_code_max))
            {
                /* Do we have a new region ? */
                if (prev_region_type == REGION_UNKNOWN)
                {
                    /* Store offset and current size. */
                    region_start = i*MEMORY_REGION_MIN_SIZE;
                    region_size = MEMORY_REGION_MIN_SIZE;
                    prev_region_type = REGION_CODE;
                }
                else
                {
                    /* Are we at the edge of a new region ? */
                    if (prev_region_type != REGION_CODE)
                    {
                        /* Recompute entropy. */
                        ent = entropy((unsigned char *)p_data + region_start, region_size);

                        /* Register previous region. */
                        memregion_add(region_start, region_size, ent, prev_region_type);

                        /* Keep track of new region. */
                        region_start = i*MEMORY_REGION_MIN_SIZE;
                        region_size = MEMORY_REGION_MIN_SIZE;
                        prev_region_type = REGION_CODE;
                    }
                    else
                    {
                        /* Same region, add this one to the previous one. */
                        region_size += MEMORY_REGION_MIN_SIZE;
                    }
                }
            }
        }

        if (prev_region_type != REGION_UNKNOWN)
        {
            /* Recompute entropy. */
            ent = entropy((unsigned char *)p_data + region_start, region_size);

            /* Register previous region. */
            memregion_add(region_start, region_size, ent, prev_region_type);
        }
    }
}

memregion_type_t memory_get_type(uint64_t offset)
{
    memregion_t *p_region;

    p_region = memregion_enum_first();
    while(p_region != NULL)
    {
        /* Check if offset belong to this region. */
        if ( (offset >= (p_region->offset)) && (offset < (p_region->offset + p_region->size)) )
        {
            /* Return region type. */
            return p_region->type;
        }

        /* Next item. */
        p_region = memregion_enum_next(p_region);
    }

    /* Not found, type is unknown. */
    return REGION_UNKNOWN;
}