#pragma once

#include <stdlib.h>

#include "helpers.h"

typedef enum {
    POI_UNKNOWN,
    POI_STRING,
    POI_ARRAY,
    POI_STRUCTURE,
    POI_FUNCTION,

    /* These points to interresting data. */
    POI_GENERIC_POINTER,
    POI_DATA_POINTER,
    POI_UNINIT_DATA_POINTER,
    POI_FUNCTION_POINTER,
    POI_ARRAY_POINTER,
    POI_STRING_POINTER,
    POI_POINTER_POINTER,
    POI_STRUCTURE_POINTER,
    POI_STRUCT_ARRAY_POINTER,

    /* These represents versatile data. */
    POI_NULLPTR_OR_VALUE
} poi_type_t;

typedef struct _poi_t {

    /* Next item. */
    struct _poi_t *p_next;

    /* By default, consider a 64-bit offset. */
    uint64_t offset;

    /* Size. */
    int count;

    /* Used for structures. */
    int *signature;
    int nb_members;

    /* Type. */
    poi_type_t type;

} poi_t;

void poi_init(poi_t *p_poi_list);
poi_t *poi_list(void);
void poi_list_free(poi_t *p_poi_list);
void poi_list_append(poi_t *p_poi_list, poi_t *poi);
poi_t *poi_list_get_last_item(poi_t *p_poi_list);
int poi_add(poi_t *p_poi_list, uint64_t offset, int count, poi_type_t type);
int poi_add_unique(poi_t *p_poi_list, uint64_t offset, int count, poi_type_t type);
int poi_add_unique_sorted(poi_t *p_poi_list, poi_t *p_poi);
int poi_add_structure_array(poi_t *p_poi_list, uint64_t offset, int count, int nb_members, int *signature);
int is_in_poi(poi_t *p_poi_list, arch_t arch, uint64_t address, uint64_t offset);
unsigned int poi_count(poi_t *p_poi_list);
