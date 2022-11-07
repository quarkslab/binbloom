#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <getopt.h>
#include <ctype.h>
#include <pthread.h>

/* Include our libs. */
#include "addrtree.h"
#include "poi.h"
#include "helpers.h"
#include "common.h"
#include "memregion.h"
#include "log.h"
#include "functions.h"

#define	VER_MAJOR	2
#define VER_MINOR	1
#define VER_REV		0

uint8_t ptr_aligned=0;

/* Base address candidate structure. */
typedef struct {
    uint64_t address;
    int votes;
    int nb_pointers;
} base_address_candidate;

/* Score entry structure, used in analysis. */
typedef struct {
    uint64_t base_address;
    int votes;
    unsigned int score;
    int has_valid_array;
} score_entry_t;

/* Structure of parameters used in parallel computing. */
typedef struct {
    score_entry_t *p_scores;
    poi_t *p_poi_list;
    addrtree_node_t *p_candidates;
    arch_t arch;
    endianness_t endian;
    unsigned char *content;
    unsigned int ui_content_size;
    pthread_mutex_t *lock;
    int start;
    int count;
} parallel_params_t;

/* Globals */
arch_t g_target_arch;
endianness_t g_target_endian;
unsigned char *gp_content;
unsigned int g_content_size;
unsigned int g_chunk_size;

/* Globals used by find_base_address(). */
base_address_candidate *gp_ba_candidates;
int gp_ba_candidates_index;

poi_t g_poi_list;
addrtree_node_t *g_candidates=NULL;

uint64_t g_ptr_base;
uint64_t g_ptr_mask;

/* Memory alignment for candidates. */
uint64_t g_mem_alignment = 0x1000;
uint64_t g_mem_alignment_mask;


/* Globals used by find_best_match() */
uint64_t g_bm_address;
int g_bm_votes;
int g_bm_count;
int g_bm_total_votes;
int g_bm_kept;
int max_votes;
int g_bm_processed;

uint64_t g_max_address = 0xFFFFFFFFFFFFFFFF;
unsigned int g_max_score;

/* Main modes and info. */
static int g_verbose = LOGLEVEL_WARNING;
static int g_deepmode = 0;
static int g_show_help = 0;
static int g_nb_threads = 1;
static char *psz_functions_file = NULL;
static poi_t *g_symbols_list = NULL;

/* Mutex to handle multi-thread processing. */
pthread_mutex_t deep_lock = PTHREAD_MUTEX_INITIALIZER;


/**
 * @brief   Compute chunk size (used to optimize progress bar update)
 **/
void compute_chunk_size(void)
{
    if (g_content_size > 0)
    {
        g_chunk_size = g_content_size/1000;
	if (g_chunk_size == 0)
            g_chunk_size = 100;
    }
}


/**
 * @brief   Find text strings and add them into a given list of point of interests.
 * @param   p_poi_list        pointer to a list of point of interests
 * @param   ui_min_str_size   minimum size of strings
 **/

void index_poi_strings(poi_t *p_poi_list, unsigned int ui_min_size)
{
    unsigned int cursor;
    unsigned int count;
    unsigned int str_start_offset;
    int is_in_str;
    int nb_strings;

    cursor = 0;
    is_in_str = 0;
    str_start_offset = 0;
    count = 0;
    nb_strings = 0;

    while (cursor < g_content_size)
    {
        if (cursor % g_chunk_size == 0)
        {
            progress_bar(cursor, g_content_size, "Indexing strings ...");
        }
        if (!is_in_str)
        {
            if (isprint(gp_content[cursor]))
            {
                is_in_str = 1;
                str_start_offset = cursor;
                count = 1;
            }
        }
        else
        {
            if (!isprint(gp_content[cursor]))
            {
                is_in_str = 0;
                if (count >= ui_min_size)
                {
                    /* Add POI. */
                    poi_add(p_poi_list, str_start_offset, count, POI_STRING);
                    debug(
                        "found string of size %d at offset %016lx\n",
                        count,
                        str_start_offset
                    );
                    nb_strings++;
                }
            }
            else
                count++;
        }

        /* Go to next char. */
        cursor++;
    }
    progress_bar_done();
    logm("[i] %d strings indexed\n", nb_strings);
}


/**
 * @brief   Find potential pointers in file content, and add them to a given list of point of interests.
 * @param   p_poi_list          pointer to a list of points of interests
 * @param   u64_base_address    firmware base address to consider
 **/

void index_poi_pointers(poi_t *p_poi_list, uint64_t u64_base_address)
{
    unsigned int cursor=0;
    uint64_t value;
    memregion_type_t mem_type;
    poi_t *poi;

    while (cursor < g_content_size-get_arch_pointer_size(g_target_arch))
    {
        /* Read value as pointer. */
        value = read_pointer(g_target_arch, g_target_endian, gp_content, cursor);

        /* If a list of symbols have been provided, use it. */
        if ((g_symbols_list != NULL) && (memory_get_type(cursor) != REGION_CODE))
        {
            /* Check if this pointer points to an existing function. */
            poi = g_symbols_list->p_next;
            while (poi != NULL)
            {
                if (((value - u64_base_address) == poi->offset) && ((poi->type == POI_FUNCTION)))
                {
                    poi_add(p_poi_list, cursor, 1, POI_FUNCTION_POINTER);
                    debug("pointer %016lx points to a known function\n", value);
                }
                poi = poi->p_next;
            }
        }
        else if (memory_get_type(cursor) != REGION_CODE)
        {
            /* Is this a valid pointer ? */
            mem_type = memory_get_type(value - u64_base_address);
            if (
                (value >= u64_base_address) &&
                (value < (u64_base_address + g_content_size)) &&
                (value!=0) && 
                (mem_type!=REGION_UNKNOWN) && 
                (mem_type!=REGION_UNINIT_DATA)
            )
            {
                /* If a pointer points into a code section, set it as a function pointer. */
                if (mem_type == REGION_CODE)
                {
                    /* Add this value as a generic pointer to our PoIs (we don't know yet what it points to). */
                    poi_add(p_poi_list, cursor, 1, POI_FUNCTION_POINTER);
                    debug("pointer %016lx points to code, considering a function pointer\n", value);
                }
                else
                {
                    /* Add this value as a generic pointer to our PoIs (we don't know yet what it points to). */
                    poi_add(p_poi_list, cursor, 1, POI_GENERIC_POINTER);
                    debug("pointer %016lx points to initialized data, considering a generic pointer\n", value);
                }
            }
        }

        cursor += get_arch_pointer_size(g_target_arch);
    }
}


/**
 * @brief   Find arrays of pointers in the provided firmware file
 *
 * This function searches for arrays of successive pointers identified in a list
 * of point of interests.
 *
 * @param   p_pointer_arrays_list       pointer to a list of arrays of interest
 * @param   u64_base_address            firmware base address to consider
 **/

void index_poi_pointer_arrays(poi_t *p_pointer_arrays_list, poi_t *p_pointers_list, uint64_t u64_base_address)
{
    int i;
    int count=0;
    int in_array=0;
    poi_type_t pointer_type;
    poi_t *pointer;
    poi_t *array_start;
    uint64_t last_offset;
    int nb_pointers;

    nb_pointers = 0;
    pointer = p_pointers_list->p_next;
    while (pointer != NULL)
    {
        pointer = pointer->p_next;
        nb_pointers++;
    }

    /* Loop on pointers, and find a series of same pointers. */
    i = 0;
    pointer = p_pointers_list->p_next;
    while (pointer != NULL)
    {
        progress_bar(i, nb_pointers, "Indexing arrays ...");
        if (!in_array)
        {
            count = 1;
            array_start = pointer;
            last_offset = pointer->offset;
            pointer_type = pointer->type;

            in_array = 1;
        }
        else
        {
            /* Non-consecutive pointer found, end of array. */
            if ((pointer->offset != (last_offset + get_arch_pointer_size(g_target_arch))) || (pointer->type != pointer_type))
            {
                if (count > 4)
                {
                    debug(
                        "Found array of %d pointers @%016lx (type:%d).\n",
                        count,
                        array_start->offset,
                        pointer_type
                    );
                    poi_add(
                        p_pointer_arrays_list,
                        array_start->offset,
                        count,
                        POI_ARRAY_POINTER
                    );
                }
                in_array = 0;

            }
            else
            {
                count++;
                last_offset = pointer->offset;
            }
        }

        pointer = pointer->p_next;
    }
    progress_bar_done();
}


/**
 * @brief   Displays structure declaration based on its signature.
 * @param   signature     structure signature
 * @param   nb_members    number of elements in the provided signature
 **/

void structure_disp_declaration(int *signature, int nb_members, arch_t arch)
{
    int i;
    int field_count=0;
    char *value_type = (arch==ARCH_32)?"uint32_t":"uint64_t";

    printf("struct {\n");
    for (i=0; i<nb_members; i++)
    {
        switch(signature[i])
        {
            case -1:
                printf("\t%s field_%d;\n", value_type, field_count++);
                break;

            case POI_STRING:
            case POI_STRING_POINTER:
                printf("\tchar *psz_field_%d;\n", field_count++);
                break;

            case POI_POINTER_POINTER:
            case POI_STRUCTURE_POINTER:
            case POI_GENERIC_POINTER:
                printf("\tvoid *p_field_%d;\n", field_count++);
                break;

            case POI_FUNCTION_POINTER:
                printf("\tcode *p_field_%d;\n", field_count++);
                break;

            case POI_DATA_POINTER:
                printf("\tdata *p_field_%d;\n", field_count++);
                break;

            case POI_UNINIT_DATA_POINTER:
                printf("\tvar *p_field_%d;\n", field_count++);
                break;

            default:
                printf("\t%s dw_%d;\n", value_type, field_count++);
                break;
        }
    }
    printf("}\n");
}


/**
 * @brief   Create signature for a given identified structure
 * @param   p_pointers_list     pointer to a list of pointers POI
 * @param   p_strings_list      pointer to a list of text strings POI
 * @param   u64_base_address    base address to consider
 * @param   nb_members          number of members of the structure
 * @param   p_struct_sign       output structure
 **/

void structure_create_signature(poi_t *p_pointers_list, poi_t *p_strings_list, uint64_t u64_base_address, uint64_t offset, int nb_members, int *p_struct_sign)
{
    int i;
    poi_t *item;
    uint64_t value;

    /* Fill signature. */
    for (i=0; i<nb_members; i++)
    {
        /* Read value. */
        value = read_pointer(g_target_arch, g_target_endian, gp_content, offset + i*get_arch_pointer_size(g_target_arch));

        /* Member is unknown by default. */
        p_struct_sign[i] = -1;

        /* Is the member a pointer onto one of our recovered pointers ? */
        item = p_pointers_list->p_next;
        while (item != NULL)
        {
            if ((item->offset + u64_base_address) == value)
            {
                if (item->type >= POI_GENERIC_POINTER)
                {
                    p_struct_sign[i] = POI_POINTER_POINTER;
                    break;
                }
            }
            else
            if (item->offset == (offset + i*get_arch_pointer_size(g_target_arch)))
            {
                /* Member IS a known pointer. */
                p_struct_sign[i] = item->type;
                break;
            }


            /* Go on with next item. */
            item = item->p_next;
        }

        /* Data is not a known pointer, check if it points to a string. */
        if (p_struct_sign[i] < 0)
        {
            item = p_strings_list->p_next;
            while (item != NULL)
            {
                if ((item->offset + u64_base_address) == value)
                {
                    /* It points to a string, add type. */
                    p_struct_sign[i] = item->type;
                    break;
                }

                /* Go on with next item. */
                item = item->p_next;
            }
        }
        
        /* Data is still unknown, consider as unknown data. */
        if (p_struct_sign[i] < 0)
        {
            /* Is it a versatile value ? */
            if ((value == 0) || (value == 0xFFFFFFFF) || (value == 0xFFFFFFFFFFFFFFFF))
            {
                /* Yes, declare it versatile. */
                p_struct_sign[i] = POI_NULLPTR_OR_VALUE;
            }
            else
                p_struct_sign[i] = POI_UNKNOWN;
        }
    }
}


/**
 * @brief   Find arrays of known structures
 * @param   p_struct_list       pointer to a list of arrays of structures (output)
 * @param   p_pointers_list     pointer to a list of pointers POI
 * @param   p_strings_list      pointer to a list of strings POI
 * @param   u64_base_address    firmware base address
 **/

void index_poi_structure_arrays(poi_t *p_struct_list, poi_t *p_pointers_list, poi_t *p_strings_list, uint64_t u64_base_address)
{
    int count, nb_members, i, j;
    int cursor, found, opt_count, opt_nb_members, min_offset;
    poi_t *poi, *poi2;
    int results[MAX_STRUCT_MEMBERS];
    int sign[MAX_STRUCT_MEMBERS];
    int nb_poi;

    /* Count POIs. */
    nb_poi = 0;
    poi = p_pointers_list->p_next;
    while (poi != NULL)
    {
        poi = poi->p_next;
        nb_poi++;
    }

    /**
     * Second try here, we are trying to identify repetitions in identified pointers:
     *
     * +0x00: <PTR> \
     * +0x04: DWORD |  Here we have +0x0C between two pointers. If it continues
     * +0x08: DWORD |  the same way below, with PTR each 0x0C bytes, then we have
     * +0x0C: <PTR> /  identified an array of structures of size 0xC.
     *
     * In fact, works better than all previous complex signature-based attempts \o/
     **/
    j = 0;
    min_offset = 0;
    poi = p_pointers_list->p_next;
    while (poi != NULL)
    {
        progress_bar(j, nb_poi, "Searching structures ...");

        /* 
         * For each pointer, we try to determine if it belongs to a structure that
         * belongs to an array of structures. We simply suppose the number of members
         * of that structure, count the number of successive valid pointers we get,
         * and keep the one that gives the longest chain.
         */
        if (poi->offset >= min_offset)
        {
            memset(results, 0, sizeof(int)*MAX_STRUCT_MEMBERS);
            for (nb_members = MAX_STRUCT_MEMBERS; nb_members > 1; nb_members--)
            {
                count = 0;
                found = 0;
                do
                {
                    /* Compute next pointer offset. */
                    cursor = poi->offset + count*(nb_members*get_arch_pointer_size(g_target_arch));

                    /* If next offset is in our dump, count consecutive matches. */
                    if (cursor < (g_content_size - get_arch_pointer_size(g_target_arch)))
                    {
                        found = 0;
                        poi2 = p_pointers_list->p_next;
                        while (poi2 != NULL)
                        {
                            /* if our cursor'th structure first member is a known POI of same type, consider it. */
                            if ((poi2->offset == cursor) && (poi2->type == poi->type))
                            {
                                /* Known POI, let's continue. */
                                found = 1;
                                break;
                            }

                            poi2 = poi2->p_next;
                        }
                        
                        if (found)
                            count++;
                    }
                    else
                        break;
                }
                while (found);
                results[nb_members-1] = count;
            }

            opt_count = -1;
            opt_nb_members = -1;
            for (i=0; i<MAX_STRUCT_MEMBERS; i++)
            {
                if (results[i] > opt_count)
                {
                    opt_nb_members = i+1;
                    opt_count = results[i];
                }
            }

            if ((opt_count > 3) && (opt_nb_members >= 2))
            {
                structure_create_signature(
                    p_pointers_list,
                    p_strings_list,
                    u64_base_address,
                    poi->offset,
                    opt_nb_members,
                    sign
                );

                /*
                 * Displaying structures is still messy  at the moment, we need
                 * to work on that for a future release.
                 *
                 * However, this is a good way to focus our research for UDS DB
                 * only on memory ranges that resembles a structure array =).
                 *
                 * structure_disp_declaration(sign, opt_nb_members, arch);
                 */
                
                /* Register structure. */
                poi_add_structure_array(
                    p_struct_list,
                    poi->offset,
                    opt_count,
                    opt_nb_members,
                    sign
                );
                debug("Found an array of structures (%d members, %d items) at offset %016lx\n", opt_nb_members, opt_count, poi->offset);

                /* Update min_offset */
                min_offset = poi->offset + opt_count*(opt_nb_members*get_arch_pointer_size(g_target_arch));
            }
        }

        poi = poi->p_next;
        j++;
    }
    progress_bar_done();   
}


/**
 * @brief   Checks if a provided UDS RID is valid.
 * @param   value   RID to test
 * @return  1 if RID is valid, 0 otherwise
 **/

int is_valid_uds_rid(int value)
{
    return ((value == 0x10) ||
            (value == 0x11) ||
            (value == 0x14) ||
            (value == 0x19) ||
            ((value >= 0x27) && (value <=0x29)) || /* 3 */
            (value == 0x3E) ||
            ((value >= 0x83) && (value <= 0x87)) || /* 5 */
            ((value >= 0x22) && (value <= 0x24)) || /* 3 */
            (value == 0x2A) ||
            (value == 0x2C) ||
            (value == 0x2E) ||
            (value == 0x2F) ||
            (value == 0x31) ||
            ((value >= 0x34) && (value <= 0x38)) /* 5 */
    );

}


/**
 * @brief   Identify UDS database based on previously found structures
 * @param   p_struct_list       pointer to a list of previously found structures
 * @param   arch                target architecture
 * @param   endian              target endianness
 * @param   u64_base_address    firmware base address
 * @param   p_content           pointer to firmware content
 * @param   ui_content_size     size of firmware content
 **/

void identify_uds(poi_t *p_struct_list,  uint64_t u64_base_address)
{
    int k, n, count, start = 0;
    poi_t *p_struct;
    uint8_t udsdb[256] = {0};

    int best_udsdb_offset = -1;
    int best_udsdb_start = 0;
    int best_udsdb_size = 0;
    int in_uds_seq = 0;
    poi_t *best_udsdb_struct = NULL;

    /* 
     * We are looking for a structure member that matches an UDS ID
     * across multiple elements of a structure array.
     */
    p_struct = p_struct_list->p_next;
    while (p_struct != NULL)
    {
        /* We scan from offset 0 to structure-size-1. */
        for (n=0; n<p_struct->nb_members*get_arch_pointer_size(g_target_arch); n++)
        {

            /* TODO: We must look for the longest series of valid RIDs ! */
            in_uds_seq = 0;
            count = 0;
            for (k=0; k<p_struct->count; k++)
            {
                /* For each array item, takes the n-th byte and check if it is a valid UDS value. */
                if(is_valid_uds_rid(gp_content[p_struct->offset + k*p_struct->nb_members*get_arch_pointer_size(g_target_arch) + n]))
                {
                    if (!in_uds_seq)
                    {
                        memset(udsdb, 0, 0x3F);
                        in_uds_seq = 1;
                        start = k;
                        udsdb[gp_content[p_struct->offset + k*p_struct->nb_members*get_arch_pointer_size(g_target_arch) + n]] = 1;
                        count = 1;
                    }
                    else if (udsdb[gp_content[p_struct->offset + k*p_struct->nb_members*get_arch_pointer_size(g_target_arch) + n]] == 0)
                    {
                        udsdb[gp_content[p_struct->offset + k*p_struct->nb_members*get_arch_pointer_size(g_target_arch) + n]] = 1;
                        count++;
                    }
                    else
                    {
                        in_uds_seq = 0;
                        if (count > best_udsdb_size)
                        {
                            debug("Biggest UDS RID seq found so far: %d items in struct @%016lx (offset: %d, start: %d)\n", count, p_struct->offset, n, start);
                            best_udsdb_offset = n;
                            best_udsdb_size = count;
                            best_udsdb_struct = p_struct;
                            best_udsdb_start = start;
                        }
                    }
                }
                else
                {
                    in_uds_seq = 0;
                    if (count > best_udsdb_size)
                    {
                        debug("Biggest UDS RID seq found so far: %d items in struct @%016lx (offset: %d, start: %d)\n", count, p_struct->offset, n, start);
                        best_udsdb_offset = n;
                        best_udsdb_size = count;
                        best_udsdb_start = start;
                        best_udsdb_struct = p_struct;
                    }
                }   
            }
        }

        p_struct = p_struct->p_next;
    }
    progress_bar_done();

    if (g_target_arch == ARCH_64)
    {
        /* Show UDS DB location. */
        printf(
            "Most probable UDS DB is located at @%016lx, found %d different UDS RID\n",
            best_udsdb_struct->offset + u64_base_address + best_udsdb_offset+best_udsdb_start*best_udsdb_struct->nb_members*get_arch_pointer_size(g_target_arch),
            best_udsdb_size
        );

        /* Show structure. */
        printf("Identified structure:\n");
        structure_disp_declaration(best_udsdb_struct->signature, best_udsdb_struct->nb_members, g_target_arch);
    }
    else
    {
        printf(
            "Most probable UDS DB is located at @%08x, found %d different UDS RID\n",
            (uint32_t)(best_udsdb_struct->offset + u64_base_address + best_udsdb_offset+best_udsdb_start*best_udsdb_struct->nb_members*get_arch_pointer_size(g_target_arch)),
            best_udsdb_size
        );
        /* Show structure. */
        printf("Identified structure:\n");
        structure_disp_declaration(best_udsdb_struct->signature, best_udsdb_struct->nb_members, g_target_arch);
    }
}


/**
 * @brief Find and index functions in an unknown firmware
 * 
 * This function is called when no strings have been found 
 * 
 * @param p_poi_list: pointer to a list of point of interests (mostly arrays)
 **/

void index_functions(poi_t *p_poi_list)
{
    uint64_t ba_mask, final_mask=0x0, ptr_h;
    int nb_pointers, nb_pointers_prev;
    int i,j,k,z;
    unsigned int cursor;
    uint64_t value, v;
    addrtree_node_t *p_addr_tree = NULL;
    int in_ary = 0, nb_items=0;
    poi_t *poi;
    uint64_t max_code_addr = 0;
    memregion_t *region;

    /* Enumerate memory regions, find the greatest code section address. */
    region = memregion_enum_first();
    while (region != NULL)
    {
        if (region->type == REGION_CODE)
        {
            if (max_code_addr < (region->offset + region->size))
            {
                max_code_addr = (region->offset + region->size);
            }
        }

        region = memregion_enum_next(region);
    }    

    /* Compute lowest mask. */
    z = log2(max_code_addr);

    poi = p_poi_list->p_next;
    while (poi != NULL)
    {
        /* Check if it is an array of values. */
        if (poi->type == POI_ARRAY)
        {
            /* Check the highest mask that matches all these values. */
            k = poi->count;
            for (i=31; (i>(z-1)) && (k == poi->count) ; i--)
            {
                ba_mask = 0xffffffffffffffff << i;
                cursor = 0;
                nb_pointers = 0;

                k = 1;
                value = read_pointer(g_target_arch, g_target_endian, gp_content, poi->offset);
                if (memory_get_type(value & (~ba_mask)) == REGION_CODE)
                {
                    ptr_h = value & ba_mask;
                    for (j=1; j<poi->count; j++)
                    {
                        value = read_pointer(g_target_arch, g_target_endian, gp_content, poi->offset +j*get_arch_pointer_size(g_target_arch));
                        if ( ((value & ba_mask) != ptr_h) || (memory_get_type(value & (~ba_mask)) != REGION_CODE) )
                            break;
                        else
                            k++;
                    }
                }
            }

            /* ba_mask is our best shot for this array, register items as function pointers. */
            if (k == poi->count)
            {
                for (i=0; i<poi->count; i++)
                {
                    value = read_pointer(g_target_arch, g_target_endian, gp_content, poi->offset +i*get_arch_pointer_size(g_target_arch));
                    poi_add_unique(p_poi_list, value & (~ba_mask), -1, POI_FUNCTION);
                }
            }
        }

        poi = poi->p_next;
    }
}


/**
 * @brief   Find point of interests from a given firmware file
 * @param   p_poi_list      pointer to a list of point of interests (output)
 * @param   include_strings 1 to index strings, 0 to exclude them
 **/

void index_poi(poi_t *p_poi_list, int include_strings)
{
    unsigned int cursor = 0;
    unsigned int ary_start_offset;
    int is_in_ary=0;
    int count = 0;
    uint64_t value;
    uint64_t prev_u64;

    /* First, index text strings. */
    if (include_strings)
    {
        index_poi_strings(p_poi_list, STR_MIN_SIZE);
    }

    /* Next, index arrays of similar values. */
    cursor = 0;
    is_in_ary = 0;
    ary_start_offset = 0;
    count = 0;

    while (cursor < g_content_size-get_arch_pointer_size(g_target_arch))
    {
        if (cursor % g_chunk_size == 0)
        {
            progress_bar(cursor, g_content_size-get_arch_pointer_size(g_target_arch), "Searching for PoIs...");
        }
        value = read_pointer(g_target_arch, g_target_endian, gp_content, cursor);

        if (!is_in_ary)
        {
            if (value!=0x0 && value!=((g_target_arch==ARCH_32)?0xffffffff:0xffffffffffffffff) /*&& (value>=g_ptr_base)*/)
            {
                ary_start_offset = cursor;
                is_in_ary = 1;
                count = 0;
            }
        }
        else
        {
            if (abs(value - prev_u64) > 0x1000)
            {
                is_in_ary = 0;
                if (count > 8)
                {
                    /* Add POI. */
                    poi_add(p_poi_list, ary_start_offset, count, POI_ARRAY);
                    debug("Found array of %d values at offset 0x%016lx\n", count, ary_start_offset);
                }

                count = 0;
            }
            else
            {
                count++;
            }
        }

        prev_u64 = value;

        /* Next item. */
        cursor += get_arch_pointer_size(g_target_arch);
    }

    /* Remove progress bar. */
    progress_bar_done();
}

/**
 * @brief   Find the best base address candidates from its votes.
 *
 * This function is a callback defined to count candidates and  keep track of
 * the best one based on votes.
 * 
 * @param   u64_address     candidate address
 * @param   n_votes         number of votes
 **/

void find_best_match(uint64_t u64_address, int n_votes)
{
    g_bm_count++;
    g_bm_total_votes += n_votes;
    if (n_votes > g_bm_votes)
    {
        g_bm_votes = n_votes;
        g_bm_address = u64_address;
    }
}


/**
 * @brief   Fill global base address candidates based on votes
 * @param   u64_address     candidate address
 * @param   n_votes         number of votes
 **/

void fill_best_matches(uint64_t u64_address, int n_votes)
{
    if (((max_votes > 1) && (n_votes > 1)) || (max_votes == 1))
    {
        gp_ba_candidates[gp_ba_candidates_index].address = u64_address;
        gp_ba_candidates[gp_ba_candidates_index].votes = n_votes;
        gp_ba_candidates[gp_ba_candidates_index++].nb_pointers = 0;
    }
}

/**
 * @brief   Compare candidates function
 * @param   a   pointer to the first base address candidate to compare
 * @param   b   pointer to the second base address candidate to compare
 * @return  <0 if `a` get more votes than `b`, 0 if same number of votes and >0 otherwise
 **/

int candidate_compare_func(const void *a,const void *b)
{
    base_address_candidate *c1 = (base_address_candidate *)a;
    base_address_candidate *c2 = (base_address_candidate *)b;

    /* Negative result if c1 votes is bigger than c2 votes. */
    return (c2->votes - c1->votes);
}


/**
 * @brief   Compare candidate scores function
 * @param   a   pointer to the first base address candidate to compare
 * @param   b   pointer to the second base address candidate to compare
 * @return  <0 if `a` get better score than `b`, 0 if same score and >0 otherwise
 **/

int score_compare_func(const void *a,const void *b)
{
    score_entry_t *s1 = (score_entry_t *)a;
    score_entry_t *s2 = (score_entry_t *)b;

    /* Compare scores. */
    return (s2->score - s1->score);
}


/**
 * @brief   Check if a pointer is aligned depending on target architecture
 * @param   u64_address     address to check
 * @param   arch            target architecture
 * @return  1 if aligned, 0 otherwise
 **/

uint8_t is_ptr_aligned(uint64_t u64_address, arch_t arch){
  if (ptr_aligned) {
    return u64_address % get_arch_pointer_size(arch) == 0; 
  }
  return 1; 
}


/**
 * @brief   Thread routine that performs the second step of base address assessment.
 *
 * This function runs in a thread because it requires a lot of CPU and therefore
 * we can split the set of values to test among multiple threads running this
 * function. We can use the full capacity of the host computer using this
 * parallelized computing.
 *
 * @param   args        pointer to a `parallel_params_t` structure.
 **/

void *parallel_refine_candidates(void *args) {
    int i,j;
    parallel_params_t *params = (parallel_params_t *)args;
    uint64_t delta, v;
    int found_one_valid_array=0;
    unsigned int array_score;
    poi_t *poi, *zap;
    int n_str_ptr;
    addrtree_node_t *p_array_values = NULL;
    poi_t *p_pointers_list;

    for (i=params->start; i<(params->start + params->count); i++)
    {
        progress_bar(g_bm_processed, g_bm_kept, "Refining ...");

        gp_ba_candidates[i].nb_pointers = 0;
        delta = gp_ba_candidates[i].address;
        if (params->arch == ARCH_64)
            info("Assessing candidate %016lx (%d votes) ...\n", delta, gp_ba_candidates[i].votes);
        else
            info("Assessing candidate %08x (%d votes) ...\n", (uint32_t)delta, gp_ba_candidates[i].votes);

        /*
            * Considering the tested base address, we try to determine if one of our detected array of
            * pointers may contain at least 30% of different values pointing to other known point of
            * interests. If so, we might have found the correct base address.
            */
        found_one_valid_array = 0;

        /* First, browse arrays and determine if they contain one or more valid pointers. */
        array_score = 1;
        poi = params->p_poi_list-> p_next;
        while (poi != NULL)
        {

            if (poi->type == POI_ARRAY)
            {
                p_array_values = addrtree_node_alloc();
                for (j=0; j<poi->count; j++)
                {
                    v = read_pointer(params->arch, params->endian, params->content, poi->offset + j*get_arch_pointer_size(params->arch));

                    zap = params->p_poi_list->p_next;
                    while (zap != NULL)
                    {
                        if (((zap->type == POI_STRING) || (zap->type == POI_ARRAY)) && (v == (zap->offset + delta)))
                        {
                            addrtree_register_address(p_array_values, v);
                            break;
                        }
                        zap = zap->p_next;
                    }
                }
                n_str_ptr = addrtree_count_nodes(p_array_values);
                addrtree_node_free(p_array_values);
                if ((n_str_ptr >= (poi->count/3)) && (poi->count >= 10))
                {
                    info("Found a valid array of pointers (%d valid pointers on %d)\n", n_str_ptr, poi->count);
                    found_one_valid_array = 1;
                }
                array_score += n_str_ptr;
            }
            
            poi = poi->p_next;
        }
        
        /* Second, find pointers based on entropy. */
        gp_ba_candidates[i].nb_pointers = 0;
        p_pointers_list = poi_list();
        index_poi_pointers(p_pointers_list, gp_ba_candidates[i].address);
        params->p_scores[i].base_address = gp_ba_candidates[i].address;
        params->p_scores[i].votes = gp_ba_candidates[i].votes;
        params->p_scores[i].score = poi_count(p_pointers_list) * gp_ba_candidates[i].votes * array_score;
        params->p_scores[i].has_valid_array = found_one_valid_array;

        info("  potential pointers found: %llu\n", poi_count(p_pointers_list));
        info("  computed score: %llu\n", params->p_scores[i].score);
        if (params->p_scores[i].score > g_max_score)
        {
            g_max_address = params->p_scores[i].base_address;
            g_max_score = params->p_scores[i].score;
        }
        
        pthread_mutex_lock(params->lock);
        g_bm_processed++;
        pthread_mutex_unlock(params->lock);

        /* Free list. */
        poi_list_free(p_pointers_list);
    }

    pthread_exit(EXIT_SUCCESS);
}


/**
 * @brief   Try to guess the firmware base address.
 * @param   p_poi_list      pointer to a list of point of interests
 * @param   p_candidates    pointer to a `addrtree_node_t` structure (address tree)
 **/

void compute_candidates(
    poi_t *p_poi_list,
    addrtree_node_t *p_candidates
)
{
    poi_t *poi;
    unsigned int cursor;
    uint64_t delta;
    uint64_t v;
    uint64_t freespace;
    int count;
    uint64_t max_address = 0xFFFFFFFFFFFFFFFF;
    int i,j,z;
    int nb_candidates = 0;
    unsigned int memsize;
    score_entry_t *p_scores;
    pthread_t *p_threads = NULL;
    parallel_params_t *p_threads_params = NULL;
    int b_has_str = 0;

    poi = p_poi_list->p_next;
    while (poi != NULL)
    {
        if ((poi->type == POI_STRING) && !b_has_str)
        {
            b_has_str = 1;
        }

        poi = poi->p_next;
        nb_candidates++;
    }

    /* Loop on found POIs. */
    i = 0;
    poi = p_poi_list->p_next;
    if (poi != NULL)
    {
        while ((poi != NULL))
        {
            progress_bar(i, nb_candidates, "Analyzing ...");
            for (cursor=0; cursor<g_content_size; cursor+=((g_target_arch==ARCH_32)?4:8))
            {
                v = read_pointer(g_target_arch, g_target_endian, gp_content, cursor);

                /* Candidate pointer must not be made of ASCII. */
                /* Add heuristic because pointer should be aligned on 4bytes/8bytes 
                 * if v % get_arch_pointer_size(arch) != 0 --> not aligned 
                 * */
                if ((v & g_mem_alignment_mask) == (poi->offset & g_mem_alignment_mask) && 
                    !is_ascii_ptr(v, g_target_arch) && 
                    is_ptr_aligned(v,g_target_arch))
                {
                    /* If PoI is a string, we expect a pointer on its first character. */
                    if ( ((b_has_str == 1) && (poi->type == POI_STRING)) || ((b_has_str == 0) && (poi->type == POI_FUNCTION)) )
                    //if ( ((b_has_str == 1) && (poi->type == POI_STRING)) || (poi->type == POI_FUNCTION) )
                    {
                        if (v>=poi->offset)
                        {
                            delta = (v - poi->offset);

                            freespace = ( ((g_target_arch==ARCH_32)?0xffffffff:0xffffffffffffffff) - delta) + 1;
                            if (freespace >= g_content_size)
                            {
                                /* register candidate. */
                                addrtree_register_address(p_candidates, (uint64_t)delta);
                            }
                        }
                    }
                }
            }

            /* Does the memory used exceed our limited space ? */
            memsize = addrtree_get_memsize(p_candidates);
            if (memsize>MAX_MEM_AMOUNT)
            {
                memsize = addrtree_get_memsize(p_candidates);
                info("[mem] Memory tree is too big (%d bytes), reducing...\r\n", memsize);
                max_votes = addrtree_max_vote(p_candidates);
                addrtree_filter(p_candidates, max_votes/2);   
                memsize = addrtree_get_memsize(p_candidates);
                info("[mem] Memory tree reduced to %d bytes\r\n", memsize);
            }

            poi = poi->p_next;
            i++;
        }
        progress_bar_done();

        /* Loop on candidates, keep the best one. */
        g_bm_votes = -1;
        g_bm_total_votes = 0;
        g_bm_count=0;
        addrtree_browse(p_candidates, find_best_match, 0);

        logm("[i] Found %d base addresses to test\n", g_bm_count);

        /*
         * Best match address corresponds to the address for which we identified
         * the biggest numbers of alleged pointers. The best match is not always
         * the correct base address, so we just display it here and try to assess
         * other candidates in case we missed the correct base address.
         */
        
        gp_ba_candidates = (base_address_candidate *)malloc(sizeof(base_address_candidate) * g_bm_count);
        if (gp_ba_candidates != NULL)
        {
            max_votes = addrtree_max_vote(p_candidates);
            g_bm_kept = 0;
            gp_ba_candidates_index = 0;
            addrtree_browse(p_candidates, fill_best_matches, 0);
            info("tree browsed\n");

            if (g_target_arch == ARCH_64)
                info("Best match for base address is %016lx (%d votes)\n", g_bm_address, g_bm_votes);
            else
                info("Best match for base address is %08x (%d votes)\n", g_bm_address, g_bm_votes);

            /* Sort candidates array. */
            qsort(gp_ba_candidates, g_bm_count, sizeof(base_address_candidate), candidate_compare_func);

            debug("Found %d candidates !\n", gp_ba_candidates_index);
            for (i=0; i<gp_ba_candidates_index; i++)
            {
                debug("Found candidate address %016lx (votes: %d, position: %d)\n", gp_ba_candidates[i].address, gp_ba_candidates[i].votes, i+1);
            }

            if (!g_deepmode)
            {
                for (i=max_votes;i>=0;i--)
                {
                    g_bm_kept = 0;
                    for (j=0;j<g_bm_count;j++)
                    {
                        if (gp_ba_candidates[j].votes >= i)
                        {
                            g_bm_kept++;
                        }
                    }
                    if (g_bm_kept>=30)
                    {
                        max_votes = i+1;
                        break;
                    }
                }
            }
            else
            {
                g_bm_kept = gp_ba_candidates_index;
                max_votes = 0;
            }            
            info("Keep %d candidates with max vote=%d\n", g_bm_kept, max_votes);

            /*
             * Loop on candidate base addresses and check if arrays of values may
             * point to known point of interests (text strings or other arrays).
             * 
             * This method gives good results when at least one array contains a
             * list of pointers to text strings.
             */

            /* Allocate memory for our score table. */
            p_scores = (score_entry_t*)malloc(sizeof(score_entry_t) * g_bm_kept);
            

            z=0;
            if (p_scores != NULL)
            {
                memset(p_scores, 0, sizeof(score_entry_t)*g_bm_kept);

                /* Compute the number of candidates each thread is going to check. */
                z = g_bm_kept / g_nb_threads;

                /* Allocate some space to store the threads id. */
                p_threads = (pthread_t *)malloc(sizeof(pthread_t) * g_nb_threads);
                p_threads_params = (parallel_params_t *)malloc(sizeof(parallel_params_t) * g_nb_threads);
                if ((p_threads != NULL) && (p_threads_params != NULL))
                {
                    memset(p_threads, 0, sizeof(pthread_t) * g_nb_threads);
                    memset(p_threads_params, 0, sizeof(parallel_params_t) * g_nb_threads);

                    g_bm_processed = 0;

                    info("Starting %d threads ...\n", g_nb_threads);

                    /* Create `g_nb_threads`. */
                    for (i=0; i<g_nb_threads; i++)
                    {
                        p_threads_params[i].p_scores = p_scores;
                        p_threads_params[i].p_poi_list = p_poi_list;
                        p_threads_params[i].p_candidates = p_candidates;
                        p_threads_params[i].arch = g_target_arch;
                        p_threads_params[i].endian = g_target_endian;
                        p_threads_params[i].content = gp_content;
                        p_threads_params[i].ui_content_size = g_content_size;
                        p_threads_params[i].lock = &deep_lock;
                        p_threads_params[i].start = i*z;
                        p_threads_params[i].count = z;
                        if (i == (g_nb_threads - 1))
                        {
                            if ((i*z + z) < g_bm_kept)
                            {
                                p_threads_params[i].count = g_bm_kept -   p_threads_params[i].start;
                            }
                        }
                        info("Thread #%d will cover %d to %d\n", i, p_threads_params[i].start, p_threads_params[i].start+p_threads_params[i].count);

                        pthread_create(
                            &p_threads[i],
                            NULL,
                            parallel_refine_candidates,
                            (void *)&p_threads_params[i]
                        );
                    }

                    /* Wait for these threads to finish. */
                    for (i=0; i<g_nb_threads; i++)
                    {
                        pthread_join(p_threads[i], NULL);
                    }
                    progress_bar_done();

                    max_address = g_max_address;

                    /* Free pthreads. */
                    free(p_threads);
                    free(p_threads_params);
                }
                else
                {
                    error("Cannot allocate memory for multi-threaded search.");
                }

                info("Best match based on pointers count: %016lx\n", max_address);

                /* Check if we have a single candidate with valid array. */
                count = 0;
                for (i=0; i<g_bm_kept; i++)
                {
                    if (p_scores[i].has_valid_array > 0)
                    {
                        count++;
                    }
                }

                if (count == 1)
                {
                    for (i=0; i<g_bm_kept; i++)
                    {
                        if (p_scores[i].has_valid_array > 0)
                        {
                            max_address = p_scores[i].base_address;
                            break;
                        }
                    }

                    /* Display 100% matching address. */
                    if (g_target_arch == ARCH_64)
                        printf("[i] Base address found (valid array): 0x%016lx.\n", max_address);
                    else
                        printf("[i] Base address found (valid array): 0x%08x.\n", (uint32_t)max_address);
                }
                else
                {
                    /* Check if g_bm_address == max_address. */
                    if (g_bm_address == max_address)
                    {
                        /* Display 100% matching address. */
                        if (g_target_arch == ARCH_64)
                            printf("[i] Base address found: 0x%016lx.\n", g_bm_address);
                        else
                            printf("[i] Base address found: 0x%08x.\n", (uint32_t)g_bm_address);
                    }
                    else if (max_address != 0xFFFFFFFFFFFFFFFF)
                    {   
                        if (g_target_arch == ARCH_64)
                            printf("[i] Base address seems to be 0x%016lx (not sure).\n", max_address);
                        else
                            printf("[i] Base address seems to be 0x%08x (not sure).\n", (uint32_t)max_address);
                    }
                    else
                    {
                        if (g_target_arch == ARCH_64)
                            printf("[i] Base address seems to be 0x%016lx (not sure).\n", g_bm_address);
                        else
                            printf("[i] Base address seems to be 0x%08x (not sure).\n", (uint32_t)g_bm_address);
                    }
                }

                /* Sort remaining candidates. */
                qsort(p_scores, g_bm_kept, sizeof(score_entry_t), score_compare_func);

                /* Tell the user he/she should use the -m/--more to get all the candidates. */
                if ((nb_candidates > 0) && (g_bm_kept > 1))
                {
                    printf(" More base addresses to consider (just in case):\n");
                    for (i=0; i<((g_bm_kept>30)?30:g_bm_kept); i++)
                    {
                        if ((p_scores[i].base_address != max_address) && (p_scores[i].score > 0))
                        {
                            if (g_target_arch == ARCH_64)
                                printf("  0x%016lx (%f)\n", p_scores[i].base_address, (float)p_scores[i].score/p_scores[0].score);
                            else
                                printf("  0x%08x (%.02f)\n", (uint32_t)p_scores[i].base_address, (float)p_scores[i].score/p_scores[0].score);
                        }
                    }
                }
            }
            else
            {
                error("Cannot evaluate, low memory !\n");
            }

            /* Free scores. */
            free(p_scores);
        }

    }
    else
        error("No point of interests found, cannot deduce loading address.");
}

/**
 * @brief   Endianness detection
 *
 * Since we don't know what a pointer looks like, we scan the whole firmware and
 * extract 32-bit or 64-bit values (depending on architecture), and keep the most
 * significant bytes for LE-decoded and BE-decoded values.
 *
 * If we are interpreting LE values as BE (or vice versa), then the MSBs will vary
 * a lot and all the values will be spread among the entire space of possibilities.
 * Therefore, if we count the number of values sharing the same MSBs, we will detect
 * a specific MSB with a greater count when decoding with correct endianness.
 *
 * The idea is to compute and compare these counts in order to determine the supposed
 * endianness. We use a memory address tree to keep track of these counts without
 * eating up too much memory, and filter it regularly to save memory. By doing so,
 * we will end up with two memory address trees representing the different MSBs
 * used in the firmware, and then compare the counts of each root branches.
 *
 * @param   u64_pointer_base    pointer base value
 * @param   u64_pointer_mask    pointer mask
 **/

endianness_t detect_endianness(uint64_t *u64_pointer_base, uint64_t *u64_pointer_mask)
{
    endianness_t endian = ENDIAN_UNKNOWN;
    unsigned int i;
    int chunk_size;
    int nbits,max_le,max_be,n,m,j;
    int msb_le = 0, msb_be = 0;
    uint64_t le_ptr_base;
    uint64_t be_ptr_base;
    int max_votes;
    uint64_t address,mask, address_be;
    addrtree_node_t *p_candidates_le, *p_candidates_be, *s_candidates_le, *s_candidates_be, *p_zap;

    /* Compute chunk size (used to update progress bar). */
    chunk_size = (g_content_size / 100);
    if (chunk_size == 0)
        chunk_size = 10;

    /* Compute MSB mask. */
    nbits = log10(g_content_size)/log10(2);
    mask = (0xffffffffffffffff << (nbits-1));

    /* Parse the firmware. */
    p_candidates_le = addrtree_node_alloc();
    p_candidates_be = addrtree_node_alloc();
    addrtree_register_address(p_candidates_le, 0);
    addrtree_register_address(p_candidates_be, 0);

    for (i=0; i<g_content_size - get_arch_pointer_size(g_target_arch); i++)
    {
        if (i % chunk_size == 0)
        {
            progress_bar(i, (g_content_size - get_arch_pointer_size(g_target_arch)), "Guessing endianness ...");
        }

        /* Read LE and BE address from firmware at offset i. */
        address = read_pointer(g_target_arch, ENDIAN_LE, gp_content, i);
        address_be = read_pointer(g_target_arch, ENDIAN_BE, gp_content, i);

        /* Register masked addresses (LE and BE). */
        if ((address != 0x0) && ((address%4)==0))
            addrtree_register_address(p_candidates_le, address&mask);
        if ((address_be != 0x0) && ((address_be%4)==0))
            addrtree_register_address(p_candidates_be, address_be&mask);

        /* Cleanup memory each 0x10000 iterations. */
        if ((i%0x10000)==0)
        {
            max_votes = addrtree_max_vote(p_candidates_le);
            addrtree_filter(p_candidates_le, max_votes/2);   
            
            max_votes = addrtree_max_vote(p_candidates_be);
            addrtree_filter(p_candidates_be, max_votes/2);
        }

    }
    progress_bar_done();

    /*
      If arch is ARCH_32, addresses are stored on the last 4 bytes, so we need
      to skip the first 4 bytes.

      We need to keep track of the address tree root nodes because `p_candidates_le` and
      `p_candidates_be` may be updated if current architecture is ARCH_32.
    */
    s_candidates_le = p_candidates_le;
    s_candidates_be = p_candidates_be;

    if (g_target_arch == ARCH_32)
    {
        for (i=0;i<4;i++)
        {
            p_candidates_le = p_candidates_le->subs[0];
            p_candidates_be = p_candidates_be->subs[0];
        }
    }

    /* Compute max counts for LE and BE. */
    max_le = 0;
    max_be = 0;
    for (i=0;i<256;i++)
    {
        if (p_candidates_le->subs[i]!=NULL)
        {
            n = addrtree_max_vote(p_candidates_le->subs[i]);
            if (n>max_le)
                max_le = n;
        }
        
        if (p_candidates_be->subs[i]!=NULL)
        {
            n = addrtree_max_vote(p_candidates_be->subs[i]);
            if (n>max_be)
                max_be=n;
        }
    }

    debug("Max number of pointers if LE: %d\n", max_le);
    debug("Max number of pointers if BE: %d\n", max_be);

    /* Search for LE MSB bytes (2). */
    le_ptr_base = 0;
    p_zap = p_candidates_le;
    for (i=0;i<get_arch_pointer_size(g_target_arch)/2; i++)
    {
        n=0;
        for (j=0;j<256;j++)
        {
            if (p_zap->subs[j] != NULL)
            {
                m = addrtree_max_vote(p_zap->subs[j]);
                if (m>n)
                {
                    msb_le = j;
                    n = m;
                }
            }
        }

        le_ptr_base = (le_ptr_base << 8) | msb_le;
        p_zap = p_zap->subs[msb_le];
    }

    /* Search for BE MSB bytes (2). */
    be_ptr_base = 0;
    p_zap = p_candidates_be;
    for (i=0;i<get_arch_pointer_size(g_target_arch)/2; i++)
    {
        n=0;
        for (j=0;j<256;j++)
        {
            if (p_zap->subs[j] != NULL)
            {
                m = addrtree_max_vote(p_zap->subs[j]);
                if (m>n)
                {
                    msb_be = j;
                    n = m;
                }
            }
        }

        be_ptr_base = (be_ptr_base << 8) | msb_be;
        p_zap = p_zap->subs[msb_be];
    }


    /* Deduce the architecture. */
    if (max_be>max_le)
    {
        *u64_pointer_base = be_ptr_base << 16;
        *u64_pointer_mask = (g_target_arch == ARCH_32)?0xffff0000:0xffff000000000000;

        if (g_target_arch == ARCH_32)
            debug("Pointer base: %08x\n", *u64_pointer_base & 0xffffffff);
        else
            debug("Pointer base: %016lx\n", *u64_pointer_base);

        endian = ENDIAN_BE;
    }
    else
    {
        *u64_pointer_base = le_ptr_base << 48;
        *u64_pointer_mask = (g_target_arch == ARCH_32)?0xffff0000:0xffff000000000000;

        if (g_target_arch == ARCH_32)
            debug("Pointer base: %08x\n", *u64_pointer_base & 0xffffffff);
        else
            debug("Pointer base: %016lx\n", *u64_pointer_base);

        endian = ENDIAN_LE;
    }

    /* Free address tree nodes. */
    addrtree_node_free(s_candidates_le);
    addrtree_node_free(s_candidates_be);

    /* Return endianness. */
    return endian;
}


/**
 * @brief   Find base address based on firmware file, architecture and endianness.
 * @param   psz_filename        path to firmware file
 * @param   arch                target architecture
 * @param   endianness          target endianness
 **/

void find_base_address(char *psz_filename)
{
    FILE *f_file;
    poi_t *poi;
    int nb_strings;

    /* Initialize our list. */
    poi_init(&g_poi_list);
    
    /* Open file and get size. */
    f_file = fopen(psz_filename, "rb");
    if (f_file == NULL)
    {
        /* Failed to open file. */
        printf("[!] Cannot access file '%s'\r\n", psz_filename);
    }
    else
    {
        /* Set file cursor to the end. */
        fseek(f_file, 0, SEEK_END);

        /* Get file size. */
        g_content_size = ftell(f_file);

        /* File size must be at least the size of the target architecture's pointer size. */
        if (g_content_size >= get_arch_pointer_size(g_target_arch))
        {
            /* Determine file chunk size. */
            compute_chunk_size();

            /* Go back to the beginning of this file. */
            fseek(f_file, 0, SEEK_SET);

            /* Allocate enough memory to store content. */
            gp_content = (unsigned char *)malloc(g_content_size);
            if (gp_content == NULL)
            {
                printf("[!] Cannot allocate memory for file %s (%d bytes is too large)\r\n", psz_filename, g_content_size);
            }
            else
            {
                /* Read file content. */
                fread(gp_content, g_content_size, 1, f_file);
                printf("[i] File read (%d bytes)\r\n", g_content_size);

                /* Analyze entropy. */
                memory_analyze(gp_content, g_content_size, "default");

                if (g_target_endian != ENDIAN_UNKNOWN)
                { 
                    /* Detect endianness, pointer base and mask. */
                    //detect_endianness(arch, p_file_content, ui_file_size, &g_ptr_base, &g_ptr_mask);

                    /* Force endianness. */
                    g_target_endian = g_target_endian;
                }
                else
                {
                    /* Endianness is unknown, try to guess it. */
                    g_target_endian = detect_endianness(&g_ptr_base, &g_ptr_mask);
                }

                if (g_target_endian != ENDIAN_UNKNOWN)
                {
                    printf("[i] Endianness is %s\r\n", (g_target_endian==ENDIAN_LE)?"LE":"BE");
                    
                    if (g_symbols_list == NULL)
                    {
                        /* Search points of interest. */
                        index_poi(&g_poi_list, 1);

                        index_functions(&g_poi_list);

                        g_candidates = addrtree_node_alloc();
                        compute_candidates(&g_poi_list, g_candidates);
                    }
                    else
                    {
                        /* Index strings. */
                        index_poi(g_symbols_list, 1);

                        g_candidates = addrtree_node_alloc();
                        compute_candidates(g_symbols_list, g_candidates);
                    }                
                }
                else
                {
                    printf("[!] Unable to detect endianness :X\r\n");
                }
            }
        }
        else
        {
            printf("[!] Input file must be at least %d bytes.\r\n", get_arch_pointer_size(g_target_arch));
        }
    }
}


/**
 * @brief   Find coherent data inside a firmware file
 * @param   psz_filename        path to firmware file
 * @param   u64_base_address    firmware base address
 **/

void find_coherent_data(char *psz_filename, uint64_t u64_base_address)
{
    FILE *f_file;
    unsigned int ui_file_size;
    unsigned char *p_file_content;
    uint64_t value;
    memregion_type_t mem_type;

    poi_t *next, *next2;
    poi_t p_pointers_list;
    poi_t p_strings_list;
    poi_t p_structs_list;
    poi_t p_assets_list;
    poi_t p_sorted_pointers;
    poi_t p_pointer_arrays_list;

    poi_init(&p_pointers_list);
    poi_init(&p_strings_list);
    poi_init(&p_structs_list);
    poi_init(&p_assets_list);
    poi_init(&p_pointer_arrays_list);
    poi_init(&p_sorted_pointers);

    /* Open file and get size. */
    f_file = fopen(psz_filename, "rb");
    if (f_file == NULL)
    {
        /* Failed to open file. */
        printf("[!] Cannot access file '%s'\r\n", psz_filename);
    }
    else
    {
        /* Set file cursor to the end. */
        fseek(f_file, 0, SEEK_END);

        /* Get file size. */
        g_content_size = ftell(f_file);

	/* Determine chunk size. */
	compute_chunk_size();

        /* Go back to the beginning of this file. */
        fseek(f_file, 0, SEEK_SET);

        /* Allocate enough memory to store content. */
        gp_content = (unsigned char *)malloc(g_content_size);
        if (gp_content == NULL)
        {
            printf("[!] Cannot allocate memory for file %s (%d bytes is too large)\r\n", psz_filename, g_content_size);
        }
        else
        {
            /* Read file content. */
            fread(gp_content, g_content_size, 1, f_file);

            /* Step 0 - Analyze entropy. */
            memory_analyze(gp_content, g_content_size, "default");
            
            /* Step 1 - Index strings. */
            index_poi_strings(&p_strings_list, STR_MIN_SIZE);

            /* Step 2 - Index pointers. */
            index_poi_pointers(&p_pointers_list, u64_base_address);

            /* Step 3 - Filter out pointers that point to strings. */
            next = &p_pointers_list;
            while (next != NULL)
            {
                value = read_pointer(g_target_arch, g_target_endian, gp_content, next->offset);

                /* Check if the pointed value is in our strings PoIs. */
                next2 = &p_strings_list;
                while (next2 != NULL)
                {
                    if (value == (next2->offset + u64_base_address))
                    {
                        /* Mark this POI as a pointer to a string. */
                        //printf("%016lx points to '%s'\n", next->offset + base_address, p_file_content + next2->offset);
                        next->type = POI_STRING_POINTER;
                        break;
                    }

                    next2 = next2->p_next;
                }
                next = next->p_next;
            }

            /* Step 4 - Filter out pointers that point to functions, data and uninitialized data. */
            next = &p_pointers_list;
            while (next != NULL)
            {
                if (next->type >= POI_GENERIC_POINTER)
                {
                    /* Check if we can have a valid function. */
                    value = read_pointer(g_target_arch, g_target_endian, gp_content, next->offset);
                    mem_type = memory_get_type(value - u64_base_address);

                    switch(mem_type)
                    {
                        case REGION_CODE:
                            {
                                next->type = POI_FUNCTION_POINTER;
                            }
                            break;

                        case REGION_INIT_DATA:
                            {
                                /* This pointer points to some data. */
                                next->type = POI_DATA_POINTER;
                            }
                            break;

                        case REGION_UNINIT_DATA:
                            {
                                /* This pointer points to some uninitialized data. */
                                next->type = POI_UNINIT_DATA_POINTER;
                            }
                            break;

                        default:
                            break;
                    }
                }

                next = next->p_next;
            }

            /* Loop for arrays of pointers of same type. */
            index_poi_pointer_arrays(
                &p_pointer_arrays_list,
                &p_pointers_list,
                u64_base_address
            );

            /* Add various pointers. */
            next = p_pointers_list.p_next;
            while (next != NULL)
            {
                poi_add_unique_sorted(&p_sorted_pointers, next);
                next = next->p_next;
            }

            /* Add pointers to arrays. */
            next = p_pointer_arrays_list.p_next;
            while (next != NULL)
            {
                poi_add_unique_sorted(&p_sorted_pointers, next);
                next = next->p_next;
            }           

            /* Step 6 - Index structures arrays. */
            index_poi_structure_arrays(
                &p_structs_list,
                &p_sorted_pointers,
                &p_strings_list,
                u64_base_address
            );

            /* Step 7 - Look for UDS database \o/ */
            identify_uds(&p_structs_list, u64_base_address);
        }
    }
}


/**
 * @brief   Displays a small help screen.
 * @param   program_name    Program name to display
 **/

void print_usage(char *program_name)
{
    printf("Quarkslab's Binbloom - Raw firmware analysis tool - version %d.%d.%d\n\n", VER_MAJOR, VER_MINOR, VER_REV);
    printf("Binbloom searches for endianness, base addresses and UDS structures from raw firmware files.\n\n");
    printf(" Usage: %s [options] firmware_file\n", program_name);
    printf("\t-a (--arch)\t\tSpecify target architecture, must be 32 or 64 (default: 32).\n");
    printf("\t-b (--base)\t\tSpecify base address to use for UDS structures search (optional).\n");
    printf("\t-e (--endian)\t\tSpecify the endianness of the provided file, must be 'le' (little endian) or 'be' (big endian) (optional).\n");
    printf("\t-m (--align)\t\tSpecify base address alignment (default: 0x1000).\n");
    printf("\t-d (--deep)\t\tEnable deep search (very slow)\n");
    printf("\t-t (--threads)\t\tNumber of threads to use (default: 1)\n");
    printf("\t-v (--verbose)\t\tEnable verbose mode.\n");
    printf("\t-h (--help)\t\tShow this help\n");
    printf("\n");
    printf("Examples:\n\n");
    printf("- Find the endianness and possible base address for an unknown 32-bit architecture firmware:\n");
    printf("\t%s -a 32 test_firmware.bin\n\n", program_name);
    printf("- Find the base address knowing the endianness:\n");
    printf("\t%s -a 32 -e le test_firmware.bin\n\n", program_name);
    printf("- Find possible UDS database knowing the base address:\n");
    printf("\t%s -a 32 -e le -b 0x1000 test_firmware.bin\n", program_name);
}


/**
 * @brief   Main routine
 * @param   argc    number of arguments passed to this program
 * @param   argv    array of pointers to arguments
 **/

int main(int argc, char **argv)
{
    int option_index = 0;
    int opt;
    uint64_t base_address = DEFAULT_BASE_ADDRESS;
    char *psz_firmware_path;
    
    g_target_arch = ARCH_32;
    g_target_endian = ENDIAN_UNKNOWN;
    g_mem_alignment_mask = g_mem_alignment - 1;

    static struct option long_options[] = {
        {
            "align", required_argument, 0, 'm'
        },
        {
            "arch", required_argument, 0, 'a'
        },
        {
            "base", required_argument, 0, 'b'
        },
        {
            "endian", required_argument, 0, 'e'
        },
        {
            "verbose", no_argument, 0, 'v'
        },
        {
            "deep", no_argument, 0, 'd'
        },
        {
            "functions", required_argument, 0, 'f'
        },
        {
            "threads", required_argument, 0, 't'
        },
        {
            "help", no_argument, 0, 'h'
        },
        {
            0,0,0,0
        }
    };

    while (1)
    {
        opt = getopt_long(argc, argv, "a:b:m:e:t:f:vdh", long_options, &option_index);
        if (opt == -1)
            break;

        switch(opt)
        {
            case 'a':
                {
                    /* Processed argument. Arch must be '32' or '64'. */
                    if (!strncmp(optarg, "32", 3))
                    {
                        g_target_arch = ARCH_32;
                        printf("[i] 32-bit architecture selected.\n");
                    }
                    else if (!strncmp(optarg, "64", 3))
                    {
                        g_target_arch = ARCH_64;
                        printf("[i] 64-bit architecture selected.\n");
                    }
                    else
                    {
                        warning("-a option (arch) must be '32' or '64', considering 32-bit architecture.\n");
                    }
                }
                break;

            case 'b':
                {
                    /* Does the provided base address start with 0x ? */
                    if (!strncmp(optarg, "0x", 2) || !strncmp(optarg, "0X", 2))
                    {
                        /* Parse address as hex. */
                        base_address = strtol(&optarg[2], NULL ,16);
                    }
                    else
                    {
                        /* Consider value as hex. */
                        base_address = strtol(optarg, NULL, 16);
                    }
                    printf("[i] Base address 0x%016lx provided.\n", base_address);
                }
                break;

            case 't':
                {
                    g_nb_threads = atoi(optarg);
                    if (g_nb_threads == 0)
                        g_nb_threads = 1;
                    else if (sysconf(_SC_NPROCESSORS_ONLN) < g_nb_threads)
                    {
                        g_nb_threads = sysconf(_SC_NPROCESSORS_ONLN);
                        warning("You asked for %d threads but your machine has %d max usable cores, number of threads changed to %d", g_nb_threads, sysconf(_SC_NPROCESSORS_ONLN), sysconf(_SC_NPROCESSORS_ONLN));
                    }
                }
                break;

            case 'e':
                {
                    /* Process endianness. Expect 'le' or 'be' as parameter. */
                    if ((optarg[0]=='l' || optarg[0]=='L') && (optarg[1]=='e' || optarg[1]=='E') && optarg[2]==0)
                    {
                        g_target_endian = ENDIAN_LE;
                        printf("[i] Selected little-endian architecture.\n");
                    }
                    else if ((optarg[0]=='b' || optarg[0]=='B') && (optarg[1]=='e' || optarg[1]=='E') && optarg[2]==0)
                    {
                        g_target_endian = ENDIAN_BE;
                        printf("[i] Selected big-endian architecture.\n");
                    }
                    else
                    {
                        warning("Wrong endianness value provided. Considering little-endian.\n");
                        g_target_endian = ENDIAN_LE;
                    }
                }
                break;

            case 'f':
                {
                    psz_functions_file = optarg;
                }
                break;

            case 'v':
                {
                    g_verbose++;
                }
                break;

            case 'd':
                {
                    g_deepmode = 1;
                }
                break;

            case 'h':
                {
                    g_show_help = 1;
                }
                break;

            case 'l': 
                {
                  ptr_aligned = 1;
                }
                break;

            case 'm':
                {
                    /* Handle Hex and decimal values. */
                    if ((strlen(optarg) >= 2) && (!strncmp(optarg, "0x", 2) || !strncmp(optarg, "0X", 2)))
                    {
                        g_mem_alignment = strtol(optarg, NULL, 16);
                    }
                    else
                    {
                        g_mem_alignment = atoi(optarg);
                    }

                    g_mem_alignment_mask = g_mem_alignment - 1;
                    printf("[i] Selected memory alignment: 0x%016lx\n", g_mem_alignment);
                    printf("[i] Memory alignment mask is : 0x%016lx\n", g_mem_alignment_mask);
                }
                break;

            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    set_log_level(g_verbose);

    if (g_show_help)
    {
        print_usage(argv[0]);
        return -1;
    }
    else
    {
        if (optind < argc)
        {
            /* Get firmware file path. */
            psz_firmware_path = argv[optind];

            /* Parse functions from file if provided. */
            if (psz_functions_file != NULL)
            {
                g_symbols_list = poi_list();
                read_poi_from_file(psz_functions_file, g_symbols_list);
            }

            if (base_address != DEFAULT_BASE_ADDRESS)
            {
                /* Search coherent data. */
                find_coherent_data(psz_firmware_path, base_address);
            }
            else
                find_base_address(psz_firmware_path);
        }
        else
        {
            error("Please provide a firmware file to analyse.\n");
            print_usage(argv[0]);
        }
    }
    return 0;
}
