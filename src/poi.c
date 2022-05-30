#include "poi.h"
#include <string.h>


/**
 * @brief   Initialize a list of point of interests
 * @param   p_poi_list  pointer to a list of POI
 **/

void poi_init(poi_t *p_poi_list)
{
    p_poi_list->p_next = NULL;
    p_poi_list->offset = 0;
    p_poi_list->count = 0;
    p_poi_list->signature = NULL;
    p_poi_list->type = POI_UNKNOWN;
    p_poi_list->nb_members = 0;
}

/**
 * @brief   Get last item of a list of point of interests.
 * @param   p_poi_list  pointer to a list of POI
 * @return  pointer to the last item of the provided list
 **/

poi_t *poi_list_get_last_item(poi_t *p_poi_list)
{
    poi_t *cursor = p_poi_list;

    /* Go to the last item of this list. */
    while (cursor->p_next != NULL)
        cursor = cursor->p_next;

    /* Last item found, return. */
    return cursor;
}


/**
 * @brief   Free list of POI
 * @param   p_poi_list  pointer to a list of POI
 **/

void poi_list_free(poi_t *p_poi_list)
{
    poi_t *next, *item;

    next = p_poi_list;
    while (next != NULL)
    {
        item = next;
        next = next->p_next;
        free(item);
    }
}


/**
 * @brief   Create an empty list of POI
 * @return  pointer to an allocated list
 **/

poi_t *poi_list(void)
{
    poi_t *item;

    item = (poi_t *)malloc(sizeof(poi_t));
    if (item != NULL)
    {
        item->p_next = NULL;
        item->offset = 0;
        item->count = 0;
        item->signature = NULL;
        item->type = POI_UNKNOWN;
        item->nb_members = 0;
    }
    
    return item;
}


/**
 * @brief   Append a POI to an existing list
 * @param   p_poi_list  pointer to the list to modify
 * @param   poi         pointer to the POI to add
 **/

void poi_list_append(poi_t *p_poi_list, poi_t *poi)
{
    poi_t *last_item;

    /* Go to the last item of this list. */
    last_item = poi_list_get_last_item(p_poi_list);

    /* Attach new POI. */
    last_item->p_next = poi;
    poi->p_next = NULL;
}

/**
 * @brief   Add a structure array as a new POI in a given POI list
 * @param   p_poi_list      pointer to a list of POI
 * @param   offset          structure offset
 * @param   count           number of structures in the array
 * @param   nb_members      number of members of the structure
 * @param   signature       structure signature
 * @return  0 on success, -1 otherwise
 **/

int poi_add_structure_array(poi_t *p_poi_list, uint64_t offset, int count, int nb_members, int *signature)
{
    poi_t *poi;
    int *p_signature;

    /* Does this offset already belong to a poi ?*/
    poi = p_poi_list->p_next;
    while (poi != NULL)
    {
        if (poi->offset == offset)
            return -1;
        poi = poi->p_next;
    }

    /* Allocate memory for point of interest. */
    poi = (poi_t *)malloc(sizeof(poi_t));
    if (poi != NULL)
    {
        /* Copy signature. */
        p_signature = (int *)malloc(nb_members*sizeof(int));
        if (p_signature == NULL)
        {
            /* Unable to allocate memory for signature, free poi and return. */
            free(poi);
            return -1;
        }
        else
        {
            /* Copy signature. */
            memcpy(p_signature, signature, nb_members*sizeof(int));
        }

        /* Populate POI. */
        poi->offset = offset;
        poi->type = POI_STRUCTURE_POINTER;
        poi->count = count;
        poi->signature = p_signature;
        poi->nb_members = nb_members;

        /* Insert at the end of the list. */
        poi_list_append(p_poi_list, poi);

        /* Success. */
        return 0;
    }
    else
    {
        /* Failure. */
        return -1;
    }
}


/**
 * @brief   Add a POI into a list
 * @param   p_poi_list      pointer to a list of POI
 * @param   offset          POI offset
 * @param   count           POI count (if array)
 * @param   type            POI type
 * @return  0 on success, -1 otherwise
 **/

int poi_add(poi_t *p_poi_list, uint64_t offset, int count, poi_type_t type)
{
    poi_t *poi;

    /* Allocate memory for point of interest. */
    poi = (poi_t *)malloc(sizeof(poi_t));
    if (poi != NULL)
    {
        /* Populate POI. */
        poi->offset = offset;
        poi->type = type;
        poi->count = count;
        poi->signature = NULL;
        poi->nb_members = 0;

        /* Insert at the end of the list. */
        poi_list_append(p_poi_list, poi);

        /* Success. */
        return 0;
    }
    else
    {
        /* Failure. */
        return -1;
    }
}


/**
 * @brief   Add unique POI to a list
 * @param   p_poi_list      pointer to a list of POI
 * @param   offset          POI offset
 * @param   count           POI count (if array)
 * @param   type            POI type
 * @return  0 on success, -1 otherwise
 **/

int poi_add_unique(poi_t *p_poi_list, uint64_t offset, int count, poi_type_t type)
{
    poi_t *poi;

    /* Does this offset already belong to a poi ?*/
    poi = p_poi_list->p_next;
    while (poi != NULL)
    {
        if (poi->offset == offset)
            return 0;
        poi = poi->p_next;
    }

    /* Allocate memory for point of interest. */
    poi = (poi_t *)malloc(sizeof(poi_t));
    if (poi != NULL)
    {
        /* Populate POI. */
        poi->offset = offset;
        poi->type = type;
        poi->count = count;
        poi->signature = NULL;
        poi->nb_members = 0;

        /* Insert at the end of the list. */
        poi_list_append(p_poi_list, poi);

        /* Success. */
        return 0;
    }
    else
    {
        /* Failure. */
        return -1;
    }
}


/**
 * @brief   Check if an address belongs to a known POI
 * @param   p_poi_list      pointer to a list of POI
 * @param   arch            target architecture
 * @param   address         Address to check
 * @param   offset          offset (if required)
 * @return  1 if found, 0 otherwise
 **/

int is_in_poi(poi_t *p_poi_list, arch_t arch, uint64_t address, uint64_t offset)
{
    poi_t *p_poi;
    int arch_size = (arch == ARCH_32)?4:8;

    p_poi = p_poi_list->p_next;
    while (p_poi != NULL)
    {
        switch (p_poi->type)
        {
            case POI_STRING:
                {
                    if (address == (p_poi->offset + offset))
                    {
                        return 1;
                    }
                }
                break;

            case POI_ARRAY:
                {
                    if ((address >= (p_poi->offset + offset)) && (address<(p_poi->offset + offset + p_poi->count*arch_size)))
                    {
                        return 1;
                    }
                }
                break;

            default:
                break;
        }

        p_poi = p_poi->p_next;
    }
    
    return 0;
}


/**
 * @brief   Add a unique POI and sort POI list
 * @param   p_poi_list      pointer to a list of POI
 * @param   p_poi           pointer to a POI to add
 * @return  0 on success, -1 otherwise
 **/

int poi_add_unique_sorted(poi_t *p_poi_list, poi_t *p_poi)
{
    poi_t *poi, *prev_poi, *p_new_poi;

    /* Does this offset already belong to a poi ?*/
    prev_poi = p_poi_list;
    poi = p_poi_list->p_next;
    while (poi != NULL)
    {
        /* Offset already present, exit. */
        if (poi->offset == p_poi->offset)
            return 0;
        
        /* If current POI offset is greater, insert before. */
        if (poi->offset > p_poi->offset)
        {

            /* Allocate memory for point of interest. */
            p_new_poi = (poi_t *)malloc(sizeof(poi_t));
            if (p_new_poi != NULL)
            {
                /* Populate POI. */
                p_new_poi->offset = p_poi->offset;
                p_new_poi->type = p_poi->type;
                p_new_poi->count = p_poi->count;
                p_new_poi->signature = NULL;
                p_new_poi->nb_members = 0;
                
                /* Link. */
                p_new_poi->p_next = poi;
                prev_poi->p_next = p_poi;
                
                /* Success. */
                return 0;
            }
            else
            {
                /* Failure. */
                return -1;
            }
        }

        /* Go to next POI. */
        prev_poi = poi;
        poi = poi->p_next;
    }

    /* Allocate memory for point of interest. */
    p_new_poi = (poi_t *)malloc(sizeof(poi_t));
    if (p_new_poi != NULL)
    {
        /* Populate POI. */
        p_new_poi->offset = p_poi->offset;
        p_new_poi->type = p_poi->type;
        p_new_poi->count = p_poi->count;
        p_new_poi->signature = NULL;
        p_new_poi->nb_members = 0;
        
        /* Link. */
        p_new_poi->p_next = NULL;
        prev_poi->p_next = p_poi;
        
        /* Success. */
        return 0;
    }
    else
    {
        /* Failure. */
        return -1;
    }
}

/**
 * @brief   Count number of POI in a given list
 * @param   p_poi_list  pointer to a list of POI
 * @return  number of POI
 **/
unsigned int poi_count(poi_t *p_poi_list)
{
    unsigned int count = 0;
    poi_t *poi;

    /* Does this offset already belong to a poi ?*/
    poi = p_poi_list->p_next;
    while (poi != NULL)
    {
        count++;
        poi = poi->p_next;
    }

    return count;
}