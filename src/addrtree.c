#include "addrtree.h"

/**
 * @brief   Allocate an address tree node
 * @return  pointer to newly allocated address tree node, or NULL on error
 **/

addrtree_node_t *addrtree_node_alloc(void)
{
    addrtree_node_t *node;

    node = (addrtree_node_t *)malloc(sizeof(addrtree_node_t));
    if (node != NULL)
    {
        node->leaf = 1;
        node->votes = 1;
        node->nb_nodes = 0;
        for (int i=0;i<256;i++)
            node->subs[i] = NULL;
        return node;
    }
    else
        return NULL;
}


/**
 * @brief   Free address tree node
 * @param   p_node  Address tree node to free
 **/

void addrtree_node_free(addrtree_node_t *p_node)
{
    int k;

    if (p_node->leaf)
    {
        free(p_node);
    }
    else
    {
        for (k=0;k<256;k++)
        {
            if (p_node->subs[k] != NULL)
            {
                addrtree_node_free(p_node->subs[k]);
            }
        }
        free(p_node);
    }
}

/**
 * @brief   Register a new address into an address tree node
 * @param   p_root          pointer to the root node of the address tree node
 * @param   u64_address     address to register
 **/

void addrtree_register_address(addrtree_node_t *p_root, uint64_t u64_address)
{
    addrtree_node_t *p_node = p_root;
    addrtree_node_t *p_node_new;
    uint8_t byte;
    int i;

    for (i=64-8; i>=0; i-=8)
    {
        byte = (u64_address >> i)&0xff;
        if (p_node->subs[byte] != NULL)
        {
            p_node = p_node->subs[byte];
            if (p_node->leaf)
                p_node->votes++;
        }
        else
        {
            p_node_new = addrtree_node_alloc();
            p_node->subs[byte] = p_node_new;
            p_node->leaf = 0;
            p_node = p_node_new;
            p_root->nb_nodes++;
        }
    }
}

/**
 * @brief   Compute max vote for a given address tree node
 * @param   p_node  Address tree node
 * @return  maximum vote observed from the tree node
 **/

int addrtree_max_vote(addrtree_node_t *p_node)
{
    int max_vote,k,n;

    if (p_node->leaf)
    {
        max_vote = p_node->votes;
    }
    else
    {
        max_vote = 0;
        for (k=0;k<256;k++)
        {
            if (p_node->subs[k] != NULL)
            {
                n = addrtree_max_vote(p_node->subs[k]);
                if (n > max_vote)
                    max_vote = n;
            }
        }
    }

    return max_vote;
}


/**
 * @brief   Sum votes of all tree leaves
 * @param   p_node  pointer to root tree node
 **/

int addrtree_sum_vote(addrtree_node_t *p_node)
{
    int sum_vote,k;

    if (p_node->leaf)
    {
        sum_vote = p_node->votes;
    }
    else
    {
        sum_vote = 0;
        for (k=0;k<256;k++)
        {
            if (p_node->subs[k] != NULL)
            {
                sum_vote += addrtree_max_vote(p_node->subs[k]);
            }
        }
    }

    return sum_vote;
}


/**
 * @brief   Compute the average vote for a given tree
 * @param   p_node  Pointer to an address tree node
 * @return  average vote
 **/

double addrtree_avg_vote(addrtree_node_t *p_node)
{
    return addrtree_sum_vote(p_node)/(double)addrtree_count_nodes(p_node);
}


/**
 * @brief   Filter tree node based on threshold
 * @param   p_node              pointer to address tree node
 * @param   votes_threshold     minimum number of votes required
 **/

void addrtree_filter(addrtree_node_t *p_node, int votes_threshold)
{
    int k,leaves;

    if (!p_node->leaf)
    {
        /* Filter leaves. */
        for (k=0;k<256;k++)
        {
            if (p_node->subs[k] != NULL)
            {
                /* Is it a leaf below threshold ? */
                if ((p_node->subs[k]->leaf) && (p_node->subs[k]->votes < votes_threshold))
                {
                    /* yes, remove it. */
                    addrtree_node_free(p_node->subs[k]);
                    p_node->subs[k] = NULL;
                }
                else if (!p_node->subs[k]->leaf)
                {
                    /* Propagate. */
                    addrtree_filter(p_node->subs[k], votes_threshold);

                    /* Should we remove it ? */
                    if (p_node->subs[k]->leaf)
                    {
                        addrtree_node_free(p_node->subs[k]);
                        p_node->subs[k] = NULL;
                    }
                }
            }
        }

        /* Is this node now a leaf ? */
        leaves = 0;
        for (k=0;k<256;k++)
            if (p_node->subs[k] != NULL)
                leaves++;
        
        if (leaves == 0)
        {
            p_node->leaf = 1;
            p_node->votes = 0;
        }
    }
}


/**
 * @brief   Count number of nodes (leaves)
 * @param   p_node  pointer to address tree node
 * @return  number of nodes
 **/
int addrtree_count_nodes(addrtree_node_t *p_node)
{
    int k,nodes;

    if (p_node->leaf)
        nodes = 1;
    else
    {
        nodes = 0;
        for (k=0;k<256;k++)
        {
            if (p_node->subs[k] != NULL)
                nodes += addrtree_count_nodes(p_node->subs[k]);
        }
    }

    return nodes;
}


/**
 * @brief   Compute memory usage for a given tree node
 * @param   p_node  pointer to an address tree node
 * @return  memory size
 **/

unsigned int addrtree_get_memsize(addrtree_node_t *p_node)
{
    unsigned int nb_nodes = p_node->nb_nodes;/*addrtree_count_nodes(p_node)+1;*/
    return nb_nodes*sizeof(addrtree_node_t);
}


/**
 * @brief   Browse address tree node and call `callback` for each leaf
 * @param   p_node              pointer to an address tree node
 * @param   p_callback          pointer to a callback function that will be called for each node
 * @param   u64_base_address    base address to pass to callback function
 **/

void addrtree_browse(addrtree_node_t *p_node, FAddressTreeCallback p_callback, uint64_t u64_base_address)
{
    int k;

    if (p_node->leaf == 1)
    {
        p_callback(u64_base_address, p_node->votes);
    }
    else
    {
        /* Loop on current node. */
        for (k=0;k<256;k++)
        {
            if (p_node->subs[k] != NULL)
            {
                addrtree_browse(p_node->subs[k], p_callback, (u64_base_address<<8) | k);
            }
        }
    }
}
