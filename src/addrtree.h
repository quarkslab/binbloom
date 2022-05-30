/**
 * Memory Address Tree
 *
 * Memory Address Tree is a tree-based structure designed to store 64-bit addresses
 * used by binbloom to perform its statistical analysis. It allows binbloom to
 * manage 32-bit and 64-bit addresses and their associated votes (or counts),
 * to count the number of addresses starting with a specific prefix very easily,
 * and in a o(n) complexity since we don't need to parse a huge list.
 *
 * Its internal structure looks like this:
 *
 * [root node (votes=0)]
 * | 00 -> NULL
 * | 01 --------->[child node (votes=0)]
 * | <...>        | 00 ------------->[child node (votes=1)]
 * | FF -> NULL   |                  | 00 -> NULL
 *                |                  | 01 -> NULL
 *                | <...>            | <...>
 *                | FF -> NULL       | FF -> NULL
 *
 * Each node consists of an array of subnodes indexed on its address nibble, meaning
 * we only need 8 nodes to represent a 64-bit address.
 **/

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


typedef void (*FAddressTreeCallback)(uint64_t address, int votes);

typedef struct _addrtree_node_t {
    int votes;
    int leaf;
    int nb_nodes;
    struct _addrtree_node_t *subs[256];
} addrtree_node_t;

void addrtree_browse(addrtree_node_t *p_node, FAddressTreeCallback p_callback, uint64_t base_address);
void addrtree_register_address(addrtree_node_t *p_root, uint64_t address);
addrtree_node_t *addrtree_node_alloc(void);
void addrtree_node_free(addrtree_node_t *p_node);
int addrtree_max_vote(addrtree_node_t *p_node);
int addrtree_sum_vote(addrtree_node_t *p_node);
void addrtree_filter(addrtree_node_t *p_node, int votes_threshold);
unsigned int addrtree_get_memsize(addrtree_node_t *p_node);
int addrtree_count_nodes(addrtree_node_t *p_node);
double addrtree_avg_vote(addrtree_node_t *p_node);
