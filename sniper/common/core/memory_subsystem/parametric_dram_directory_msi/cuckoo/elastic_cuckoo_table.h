/*
 * Author: Dimitrios Skarlatos
 * Contact: skarlat2@illinois.edu - http://skarlat2.web.engr.illinois.edu/
*/

#ifndef ELASTIC_CUCKOO_TABLE_H
#define ELASTIC_CUCKOO_TABLE_H

#include "blake2-impl.h"
#include "blake2.h"
#include "city.h"
#include <assert.h>
#include <immintrin.h>
#include <math.h>
#include <vector>

#define uint64_t unsigned long long
#define uint32_t unsigned long

#define hsize(n) ((uint64_t)1 << (n))
#define hmask(n) (hsize(n) - 1)

typedef struct elem_t {
  uint8_t valid;
  uint64_t value;
} elem_t;

typedef struct cuckooTable_t {
  elem_t **hashtable;  // hashtable structure
  uint32_t d;          // d-ary cuckoo hashtable
  uint64_t hash_size;  // number of bits required for hashing
  char hash_func[20];  // hash function to be used
  uint64_t size;       // size per d way
  uint64_t *num_elems; // current # elements per d way
  uint64_t *keys;      // key per d way
  uint64_t *rehashed;  // current rehashed entries
  float *util;         // utilization per d way
  float occupancy;     // overall hashtable occupancy
} cuckooTable_t;

typedef struct elasticCuckooTable_t {
  cuckooTable_t *current; // current hashtable
  cuckooTable_t *migrate; // migrate hashtable
  float rehash_threshold; // rehash treshold
  uint8_t rehashing;      // flag if rehashing
  uint32_t d;             // d-ary cuckoo hashtable
  uint64_t curr_size;     // size per d hashtable of current
  uint32_t scale;         // scaling factor
  uint32_t swaps;         // number of swaps
  uint8_t priority;       // priority of table during rehashing
  uint64_t rehash_probes; // number of rehash probes
  uint64_t rehash_elems;  // number of rehash elements
  char hash_func[20];     // hash function to be used
} elasticCuckooTable_t;

/*
* create_elastic allocates an elastic cuckoo hashtable
* @d number of ways/nests
* @size size of each way/nest
* @hashtable the hashtable
* @hash_func name of the hash function
*/
void create(uint32_t d, uint64_t size, cuckooTable_t *hashtable,
            char *hash_func);
/*
* create_elastic allocates an elastic cuckoo hashtable
* @d number of ways/nests
* @size size of each way/nest
* @hashtable the hashtable
* @hash_func name of the hash function
* @rehash_treshold resizing threshold as a fraction
* @scale scaling factor during resizing
* @swaps number of swaps during rehashing
* @priority bias the rehashing inserts vs random
*/
void create_elastic(uint32_t d, uint64_t size, elasticCuckooTable_t *hashtable,
                    char *hash_func, float rehash_threshold, uint32_t scale,
                    uint32_t swaps, uint8_t priority);

/*
* rehash rehash elements in th elastic cuckoo hashtable
* @hashtable the elastic cuckoo hashtable
* @swaps the number of swaps to perform
* @return number of tries
*/
uint64_t rehash(elasticCuckooTable_t *hashtable, uint64_t swaps);

/*
* evaluate_elasticity evaluates the "elasticity" of the elastic cuckoo hashtable
* if a threhold of occupancy is passed reszing is initiated
* @hashtable elastic cuckoo hashtable to be evaluated
* @complete if a resize is triggered perform a complete or gradual resize
* @return number of retries if rehash was initiated
*/
uint64_t evaluate_elasticity(elasticCuckooTable_t *hashtable, uint8_t complete);

/*
* destroy de-allocate the cuckoo hashtable
* @hashtable the cuckoo hashtable to de-allocate
*/
void destroy(cuckooTable_t *hashtable);

/*
* destroy_elastic de-allocate the elastic cuckoo hashtable
* @hashtable the elastic cuckoo hashtable to de-allocate
*/
void destroy_elastic(elasticCuckooTable_t *hashtable);

/*
* insert try to insert an element in the cuckoo hashtable with recursion
* @elem element to insert
* @hashtable cuckoo hashtable to be updated
* @nest nest/way to insert
* @tries number of tries before aborting
*/
uint32_t insert_recursion(elem_t *elem, cuckooTable_t *hashtable, uint32_t nest,
                          uint32_t tries);

std::vector<elem_t> find_elastic_ptw(elem_t *elem, elasticCuckooTable_t *hashtable);
/*
* insert try to insert an element in the cuckoo hashtable
* @elem element to insert
* @hashtable cuckoo hashtable to be updated
*/
uint32_t insert(elem_t *elem, cuckooTable_t *hashtable);

/*
* insert_elastic try to insert an element in the elastic cuckoo hashtable
* @elem element to insert
* @hashtable elasticCuckoo hashtable to be updated
* @bias enable to selected the bias_nest
* @bias_nest when bias is enabled select @bias_nest as the first try
*/
uint32_t insert_elastic(elem_t *elem, elasticCuckooTable_t *hashtable,
                        uint8_t bias, uint16_t bias_nest);

/*
* find find an element in the cuckoo hashtable
* @elem element to search for
* @hashtable cuckoo hashtable to search in
*/
std::vector<elem_t> find(elem_t *elem, cuckooTable_t *hashtable);

/*
* find_elastic find an element in the elastic cuckoo hashtable
* @elem element to search for
* @hashtable elasticCuckoo hashtable to search in
*/
elem_t *find_elastic(elem_t *elem, elasticCuckooTable_t *hashtable);

/*
* delete marks an element invalid from the cuckoo hashtable
* @elem element to be marked invalid (if found)
* @hashtable cuckoo hashtable to update
*/
void delete_normal (elem_t *elem, cuckooTable_t *hashtable);

/*
* delete_elastic marks an element invalid from the elastic cuckoo hashtable
* @elem element to be marked invalid (if found)
* @hashtable elasticCuckoo hashtable to update
*/
void delete_elastic(elem_t *elem, elasticCuckooTable_t *hashtable);

/*
* update_occupancy updates the occupancy of the hashtable
* @hashtable the cuckoo hashtable of which the occupancy will be updated
*/
void update_occupancy(cuckooTable_t *hashtable);

/*
* gen_hash generates a hash index
* @elem used to generate the hash
* @hashtable use the hash function defined in the hashtable
* @nest the nest/way for which a hash is generated
*/
uint64_t gen_hash(elem_t *elem, cuckooTable_t *hashtable, uint32_t nest);

/*
* printTable prints is a helper functions that prints the hashtable
* @hashtable is a cuckoo hashtable
*/
void printTable(cuckooTable_t *hashtable);
#endif
