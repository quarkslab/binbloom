#include "arch.h"

/**
 * List of predefined entropy threshold per architecture. This feature
 * is not actually used but will be in a future release.
 **/

arch_info_t g_arch_info[] = {

    /* Default architecture. */
    {
        "default",
        ARCH_32,
        0.0,    /* uninitialized data entropy min */
        0.05,   /* uninitialized data entropy max */
        0.05,   /* data entropy min */
        0.6,    /* data entropy max */
        0.6,    /* code entropy min */
        0.9     /* code entropy max */
    },

    /* End of architecture info. */
    {
        NULL,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        0.0
    }
};


/**
 * @brief   Get architecture entropy info.
 * @param   psz_name    target architecture name
 * @return  pointer to a `arch_info_t` structure corresponding to the provided architecture, or NULL.
 **/

arch_info_t *arch_get_info(char *psz_name)
{
    int i=0;

    while (g_arch_info[i].psz_name != NULL)
    {
        if (!strcmp(psz_name, g_arch_info[i].psz_name))
        {
            /* Success. */
            return &g_arch_info[i];
        }
    }

    /* Error. */
    return NULL;
}