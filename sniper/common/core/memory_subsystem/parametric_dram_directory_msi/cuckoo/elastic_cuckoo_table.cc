/*
 * Author: Dimitrios Skarlatos
 * Contact: skarlat2@illinois.edu - http://skarlat2.web.engr.illinois.edu/
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "elastic_cuckoo_table.h"
//#define DEBUG 1

#define HASH_SIZE 8
#define MAX_RETRIES 64
#define EXTEND 1.25

uint64_t hash_size(uint64_t size);

void create(uint32_t d, uint64_t size, cuckooTable_t *hashtable,
            char *hash_func) {
  size_t i, j;

  hashtable->d = d;
  strcpy(hashtable->hash_func, hash_func);
  if (strcmp(hashtable->hash_func, "blake2") == 0) {
    hashtable->hash_size = hash_size(size);
  } else if (strcmp(hashtable->hash_func, "city") == 0) {
    hashtable->hash_size = log2(size);
  } else {
    assert(1 == 0 && "Unknown hash function\n");
  }

  hashtable->size = size;
  hashtable->keys = (uint64_t *)malloc(d * sizeof(uint64_t));
  hashtable->num_elems = (uint64_t *)malloc(d * sizeof(uint64_t));
  hashtable->rehashed = (uint64_t *)malloc(d * sizeof(uint64_t));
  hashtable->util = (float *)malloc(d * sizeof(float));
  hashtable->occupancy = 0.0;


  printf("Creating a %u-ary Cuckoo hashtable\n", hashtable->d);
  printf("Hash function %s\n", hashtable->hash_func);
  printf("Total number of slots %lu\n", hashtable->size * d);
  printf("Hash-size %lu\n", hashtable->hash_size);


  for (i = 0; i < d; i++) {
    hashtable->num_elems[i] = 0;
    hashtable->rehashed[i] = 0;
    hashtable->util[i] = 0.0;
    for (j = 0; j < 10; j++) {
      _rdrand64_step((unsigned long long *)&hashtable->keys[i]);
    }
#ifdef DEBUG
    printf("Key[%lu] = %lu\n", i, hashtable->keys[i]);
#endif
  }

  hashtable->hashtable = (elem_t **)malloc(d * sizeof(elem_t *));
  for (i = 0; i < hashtable->d; i++) {
    hashtable->hashtable[i] =
        (elem_t *)malloc(hashtable->size * sizeof(elem_t));
    for (j = 0; j < hashtable->size; j++) {
      hashtable->hashtable[i][j].valid = 0;
      hashtable->hashtable[i][j].value = i;
    }
  }
}

void create_elastic(uint32_t d, uint64_t size, elasticCuckooTable_t *hashtable,
                    char *hash_func, float rehash_threshold, uint32_t scale,
                    uint32_t swaps, uint8_t priority) {
  hashtable->current = (cuckooTable_t *)malloc(sizeof(cuckooTable_t));
  hashtable->migrate = NULL;
  hashtable->rehash_threshold = rehash_threshold;
  hashtable->rehashing = 0;
  hashtable->d = d;
  hashtable->curr_size = size;
  if (strcmp(hash_func, "blake2") == 0) {
    hashtable->scale = 256;
  } else {
    hashtable->scale = scale;
  }
  hashtable->swaps = swaps;
  hashtable->priority = priority;
  hashtable->rehash_probes = 0;
  hashtable->rehash_elems = 0;
  strcpy(hashtable->hash_func, hash_func);
  create(d, size, hashtable->current, hash_func);
}

uint64_t rehash(elasticCuckooTable_t *hashtable, uint64_t swaps) {
  uint64_t i = 0, j = 0, retries = 0;
  uint32_t rehashed = 0;
  uint16_t nest = 0, new_nest = 0;
  cuckooTable_t *current = hashtable->current;
  elem_t move;

  do {
    _rdrand16_step(&nest);
    nest = nest % current->d;
  } while (current->rehashed[nest] == current->size);

  for (i = 0; i < swaps; i++) {
    if (current->rehashed[nest] < current->size) {
      if (current->hashtable[nest][current->rehashed[nest]].valid == 1) {
        move.valid = 1;
        move.value = current->hashtable[nest][current->rehashed[nest]].value;
        current->hashtable[nest][current->rehashed[nest]].valid = 0;
        current->rehashed[nest]++;
        current->num_elems[nest]--;
        if (hashtable->priority) {
          // this will end up trying first the new hashtable
          retries += insert_elastic(&move, hashtable, 1, nest);
        } else {
          // perform a random walk
          retries += insert_elastic(&move, hashtable, 0, 0);
        }
        hashtable->rehash_probes += retries;
        hashtable->rehash_elems++;
      } else {
        current->rehashed[nest]++;
      }
      for (j = 0; j < current->d; j++) {
        rehashed += current->rehashed[j];
      }
      if (rehashed == current->size * current->d) {
        break;
      }
      do {
        _rdrand16_step(&new_nest);
        new_nest = new_nest % current->d;
      } while (new_nest == nest && current->rehashed[nest] == current->size);
      nest = new_nest;
    }
  }
  update_occupancy(current);
  update_occupancy(hashtable->migrate);
  return retries;
}

uint64_t evaluate_elasticity(elasticCuckooTable_t *hashtable,
                             uint8_t complete) {
  uint64_t retries = 0;
  if (hashtable->current->occupancy > hashtable->rehash_threshold &&
      !hashtable->rehashing) {
    hashtable->rehashing = 1;
    hashtable->migrate = (cuckooTable_t *)malloc(sizeof(cuckooTable_t));
    create(hashtable->d, hashtable->curr_size * hashtable->scale,
           hashtable->migrate, hashtable->hash_func);
  }
  if (complete) {
    if (hashtable->rehashing) {
      while (hashtable->current->occupancy != 0) {
        retries += rehash(hashtable, hashtable->swaps);
      }
      hashtable->rehashing = 0;
      destroy(hashtable->current);
      hashtable->current = hashtable->migrate;
      hashtable->migrate = NULL;
      hashtable->curr_size *= hashtable->scale;
    }

  } else {
    if (hashtable->rehashing) {
      retries += rehash(hashtable, hashtable->swaps);
    }
    if (hashtable->current->occupancy == 0 && hashtable->rehashing) {
      hashtable->rehashing = 0;
      destroy(hashtable->current);
      hashtable->current = hashtable->migrate;
      hashtable->migrate = NULL;
      hashtable->curr_size *= hashtable->scale;
    }
  }

  return retries;
}

void destroy(cuckooTable_t *hashtable) {
  uint32_t i;
  free(hashtable->keys);
  free(hashtable->num_elems);
  free(hashtable->util);

  for (i = 0; i < hashtable->d; i++) {
    free(hashtable->hashtable[i]);
  }
  free(hashtable->hashtable);
}

void destroy_elastic(elasticCuckooTable_t *hashtable) {
  destroy(hashtable->current);
  if (hashtable->migrate != NULL) {
    destroy(hashtable->migrate);
  }
}

uint32_t insert_recursion(elem_t *elem, cuckooTable_t *hashtable, uint32_t nest,
                          uint32_t tries) {
  uint16_t new_nest;
  uint64_t hash = 0;

  tries++;

  hash = gen_hash(elem, hashtable, nest);
  elem_t tmp;
  tmp.valid = 0;
  if (hashtable->hashtable[nest][hash].valid == 1) {
    tmp.valid = hashtable->hashtable[nest][hash].valid;
    tmp.value = hashtable->hashtable[nest][hash].value;
    hashtable->num_elems[nest]--;
  }

  hashtable->hashtable[nest][hash].valid = 1;
  hashtable->hashtable[nest][hash].value = elem->value;
  hashtable->num_elems[nest]++;

  // need to allocate the replaced element
  if (tmp.valid) {

    do {
      _rdrand16_step(&new_nest);
      new_nest = new_nest % hashtable->d;
    } while (new_nest == nest);
    nest = new_nest;

    if (tries > MAX_RETRIES) {
      return tries;
    }
    return insert_recursion(&tmp, hashtable, nest, tries);
  }
  update_occupancy(hashtable);
  return tries;
}

uint32_t insert(elem_t *elem, cuckooTable_t *hashtable) {
  uint32_t tries = 0;
  uint16_t nest = 0, new_nest = 0;
  uint64_t hash = 0;
  elem_t old;

  do {
    _rdrand16_step(&new_nest);
    new_nest = new_nest % hashtable->d;
  } while (new_nest == nest);
  nest = new_nest;

  // try to insert until MAX_RETRIES insertion attempts
  for (tries = 0; tries < MAX_RETRIES; tries++) {

#ifdef DEBUG
    printf("Inserting element with value %llu, nest %u\n", elem->value, nest);
#endif

    hash = gen_hash(elem, hashtable, nest);

    old.valid = 0;
    // remove previous element if it exists
    if (hashtable->hashtable[nest][hash].valid == 1) {
      old.valid = hashtable->hashtable[nest][hash].valid;
      old.value = hashtable->hashtable[nest][hash].value;
      hashtable->num_elems[nest]--;
    }

    // insert new element
    hashtable->hashtable[nest][hash].valid = 1;
    hashtable->hashtable[nest][hash].value = elem->value;
    hashtable->num_elems[nest]++;

    // we removed an element and we have to put it back
    if (old.valid) {
      // copy old element
      elem->value = old.value;
      elem->valid = 1;

      // pick new nest to try
      do {
        _rdrand16_step(&new_nest);
        new_nest = new_nest % hashtable->d;
      } while (new_nest == nest);
      nest = new_nest;
    }
    // we are done
    else {
      break;
    }
  }
  update_occupancy(hashtable);
  // printf("Insert Tries %lu\n",tries+1);
  return tries + 1;
}

uint32_t insert_elastic(elem_t *elem, elasticCuckooTable_t *hashtable,
                        uint8_t bias, uint16_t bias_nest) {
  uint32_t tries = 0, current_inserts = 0, migrate_inserts = 0;
  uint16_t nest = 0, new_nest = 0;
  uint64_t hash = 0;
  elem_t old;
  cuckooTable_t *selectTable;

  if (bias) {
    nest = bias_nest;
  } else {
    _rdrand16_step(&nest);
    nest = nest % hashtable->current->d;
  }

  // try to insert until MAX_RETRIES insertion attempts
  for (tries = 0; tries < MAX_RETRIES; tries++) {

#ifdef DEBUG
    printf("Inserting element with value %llu, nest %u\n", elem->value, nest);
#endif

    hash = gen_hash(elem, hashtable->current, nest);

    if (hashtable->rehashing && hash < hashtable->current->rehashed[nest]) {
      hash = gen_hash(elem, hashtable->migrate, nest);
      selectTable = hashtable->migrate;
      migrate_inserts++;
    } else {
      selectTable = hashtable->current;
      current_inserts++;
    }

    old.valid = 0;
    // remove previous element if it exists
    if (selectTable->hashtable[nest][hash].valid == 1) {
      old.valid = selectTable->hashtable[nest][hash].valid;
      old.value = selectTable->hashtable[nest][hash].value;
      selectTable->num_elems[nest]--;
    }

    // insert new element
    selectTable->hashtable[nest][hash].valid = 1;
    selectTable->hashtable[nest][hash].value = elem->value;
    selectTable->num_elems[nest]++;

    // we removed an element and we have to put it back
    if (old.valid) {
      // copy old element
      elem->value = old.value;
      elem->valid = 1;

      // pick new nest to try
      do {
        _rdrand16_step(&new_nest);
        new_nest = new_nest % selectTable->d;
      } while (new_nest == nest);
      nest = new_nest;
    }
    // we are done
    else {
      break;
    }
  }
  if (migrate_inserts) {
    update_occupancy(hashtable->migrate);
  }
  if (current_inserts) {
    update_occupancy(hashtable->current);
  }
  return tries + 1;
}

void delete_normal (elem_t *elem, cuckooTable_t *hashtable) {
  uint32_t nest;
  uint64_t hash = 0;

  for (nest = 0; nest < hashtable->d; nest++) {
    hash = gen_hash(elem, hashtable, nest);

    if (hashtable->hashtable[nest][hash].valid == 1 &&
        hashtable->hashtable[nest][hash].value == elem->value) {

      hashtable->hashtable[nest][hash].valid = 0;
      hashtable->hashtable[nest][hash].value = 0;
      hashtable->num_elems[nest]--;
      update_occupancy(hashtable);
      return;
    }
  }
}

void delete_elastic(elem_t *elem, elasticCuckooTable_t *hashtable) {
  uint32_t nest = 0;
  uint64_t hash = 0;
  cuckooTable_t *selectTable;

  for (nest = 0; nest < hashtable->current->d; nest++) {
    hash = gen_hash(elem, hashtable->current, nest);

    if (hashtable->rehashing && hash < hashtable->current->rehashed[nest]) {
      hash = gen_hash(elem, hashtable->migrate, nest);
      selectTable = hashtable->migrate;
    } else {
      selectTable = hashtable->current;
    }

    if (selectTable->hashtable[nest][hash].valid == 1 &&
        selectTable->hashtable[nest][hash].value == elem->value) {
      selectTable->hashtable[nest][hash].valid = 0;
      selectTable->hashtable[nest][hash].value = 0;
      selectTable->num_elems[nest]--;
      update_occupancy(selectTable);
      return;
    }
  }
}

//now returns all the addresses it accessed
std::vector<elem_t> find(elem_t *elem, cuckooTable_t *hashtable) {
  uint32_t nest = 0;
  uint64_t hash = 0;

  bool found = false;
  std::vector<elem_t> accessedAddresses;
  for (nest = 0; nest < hashtable->d; nest++) {
    hash = gen_hash(elem, hashtable, nest);
    elem_t toAdd;
    toAdd.valid = 0;
    toAdd.value = (uint64_t)&hashtable->hashtable[nest][hash];

    // VPN: 0x000100

    accessedAddresses.push_back(toAdd);
    if (hashtable->hashtable[nest][hash].valid == 1 &&
        hashtable->hashtable[nest][hash].value == elem->value) {
          found = true;
          accessedAddresses.back().valid = 1;
    }
  }
  if(!found)
    insert(elem, hashtable);
  return accessedAddresses;
}


std::vector<elem_t> find_elastic_ptw(elem_t *elem, elasticCuckooTable_t *hashtable) {
  uint32_t nest = 0;
  uint64_t hash = 0;
  cuckooTable_t *selectTable;
  bool found = false;
  std::vector<elem_t> accessedAddresses;
  
  for (nest = 0; nest < hashtable->current->d; nest++) {
    hash = gen_hash(elem, hashtable->current, nest);
    //printf("Searching nest %d element in cuckoo \n", nest);
    if (hashtable->rehashing && hash < hashtable->current->rehashed[nest]) {
      hash = gen_hash(elem, hashtable->migrate, nest);
      selectTable = hashtable->migrate;
    } else {
      selectTable = hashtable->current;
    }
    elem_t toAdd;
    toAdd.valid = 0;
    toAdd.value = (uint64_t)&selectTable->hashtable[nest][hash];
    accessedAddresses.push_back(toAdd);
    if (selectTable->hashtable[nest][hash].valid == 1 &&
        selectTable->hashtable[nest][hash].value == elem->value) {
      found = true;
      //printf("Found element in cuckoo \n");
      accessedAddresses.back().valid = 1;
    }
  }

  return accessedAddresses;
}

elem_t *find_elastic(elem_t *elem, elasticCuckooTable_t *hashtable) {
  uint32_t nest = 0;
  uint64_t hash = 0;
  cuckooTable_t *selectTable;

  for (nest = 0; nest < hashtable->current->d; nest++) {
    hash = gen_hash(elem, hashtable->current, nest);

    if (hashtable->rehashing && hash < hashtable->current->rehashed[nest]) {
      hash = gen_hash(elem, hashtable->migrate, nest);
      selectTable = hashtable->migrate;
    } else {
      selectTable = hashtable->current;
    }

    if (selectTable->hashtable[nest][hash].valid == 1 &&
        selectTable->hashtable[nest][hash].value == elem->value) {
      return &selectTable->hashtable[nest][hash];
    }
  }
  return NULL;
}

uint64_t gen_hash(elem_t *elem, cuckooTable_t *hashtable, uint32_t nest) {
  uint64_t hash = 0;

#ifdef DEBUG
  for (i = 0; i < hashtable->d; i++) {
    if (strcmp(hashtable->hash_func, "blake2") == 0) {
      blake2b(&hash, hashtable->hash_size / 8, &elem->value, HASH_SIZE,
              &hashtable->keys[i], 8);
    } else if (strcmp(hashtable->hash_func, "city") == 0) {
      hash = CityHash64WithSeed(&elem->value, HASH_SIZE, hashtable->keys[i]);
      hash = hash & hmask((uint64_t)hashtable->hash_size);
    } else {
      assert(1 == 0 && "Unknown hash function\n");
    }
    printf("Hash %lu\n", hash);
  }
#endif

  if (strcmp(hashtable->hash_func, "blake2") == 0) {
    blake2b(&hash, hashtable->hash_size / 8, &elem->value, HASH_SIZE,
            &hashtable->keys[nest], 8);
      printf("Hash %lu\n", hash);
  } else if (strcmp(hashtable->hash_func, "city") == 0) {
    hash = CityHash64WithSeed((const char *)&elem->value, HASH_SIZE,
                              hashtable->keys[nest]);
    hash = hash & hmask((uint64_t)hashtable->hash_size);
  } else {
    assert(1 == 0 && "Unknown hash function\n");
  }
  if (hash > hashtable->size) {
    printf("Hash value %llu, size %llu\n", hash, hashtable->size);
    assert(1 == 0 && "Hash value is larger than index\n");
  }
  return hash;
}

void update_occupancy(cuckooTable_t *hashtable) {
  uint32_t i = 0;
  uint64_t total_elems = 0;
  for (i = 0; i < hashtable->d; i++) {
    hashtable->util[i] = hashtable->num_elems[i] / (float)hashtable->size;
    total_elems += hashtable->num_elems[i];
  }
  hashtable->occupancy = total_elems / (float)(hashtable->d * hashtable->size);
#ifdef DEBUG
  printf("Total elements: %lu\n", total_elems);
  printf("Occupancy: %f\n", hashtable->occupancy);
#endif
}

void printTable(cuckooTable_t *hashtable) {
  size_t i, j;
  for (i = 0; i < hashtable->d; i++) {
    for (j = 0; j < hashtable->size; j++) {
      printf("(%u,%llu) | ", hashtable->hashtable[i][j].valid,
             hashtable->hashtable[i][j].value);
    }
    printf("\n");
  }
}

uint64_t hash_size(uint64_t size) {
  uint64_t hash_size = 0;

  while (log2(size) > (float)hash_size) {
    hash_size += 8;
  }
#ifdef DEBUG
  printf("hashtable size = %lu, requested size = %lu\n", hash_size, size);
#endif
  return hash_size;
}

// void simple_example(uint32_t d, uint64_t size, char *hash_func, uint8 elastic,
//                     uint8_t oneshot, float rehash_threshold, uint8_t scale,
//                     uint8_t swaps, uint8_t priority) {

                      /*
  uint32_t i = 0, N = 512;
  uint64_t *test_values = NULL;
  elem_t new_elem;
  elasticCuckooTable_t elasticCuckooHT;
  cuckooTable_t cuckooHT;

  if (elastic) {
    create_elastic(d, size, &elasticCuckooHT, hash_func, rehash_threshold,
                   scale, swaps, priority);
  } else {
    create(d, size, &cuckooHT, hash_func);
  }
  test_values = (uint64_t *)malloc(N * sizeof(uint64_t));

  for (i = 0; i < N; i++) {
    test_values[i] = 0;
  }

  for (i = 0; i < N; i++) {
    new_elem.valid = 1;

    if (elastic) {
      do {
        _rdrand64_step((unsigned long long *)&new_elem.value);
      } while (find_elastic(&new_elem, &elasticCuckooHT) != NULL);

    } else {
      do {
        _rdrand64_step((unsigned long long *)&new_elem.value);
      } while (find(&new_elem, &cuckooHT) != NULL);
    }

    test_values[i] = new_elem.value;

    if (elastic) {
      insert_elastic(&new_elem, &elasticCuckooHT, 0, 0);
    } else {
      insert(&new_elem, &cuckooHT);
    }

    if (elastic) {
      evaluate_elasticity(&elasticCuckooHT, oneshot);
    }
  }

  if (elastic) {
    destroy_elastic(&elasticCuckooHT);
  } else {
    destroy(&cuckooHT);
  }
  */
//}

/*
int main(int argc, char **argv) {
  uint32_t d = 4, size = 0, elastic = 0, oneshot = 0, scale = 0, swaps = 0;
  char hash_func[20];
  float threshold = 0;
  uint8_t priority = 0;

  assert(argc == 10 || argc == 5);
  d = strtol(argv[1], NULL, 10);
  if (d < 2) {
    printf("Number of ways required to be greater than 2\n");
  }
  size = strtol(argv[2], NULL, 10);
  if (strcmp(argv[3], "blake2") == 0) {
    strcpy(hash_func, "blake2");
  } else if (strcmp(argv[3], "city") == 0) {
    strcpy(hash_func, "city");
  } else {
    printf("Hash function not found\n");
    return 0;
  }

  if (strcmp(argv[4], "elastic") == 0) {
    elastic = 1;
  } else {
    elastic = 0;
  }

  if (elastic) {
    if (strcmp(argv[5], "oneshot") == 0) {
      oneshot = 1;
    } else {
      oneshot = 0;
    }
    threshold = strtof(argv[6], NULL);
    scale = strtol(argv[7], NULL, 10);
    swaps = strtol(argv[8], NULL, 10);
    priority = strtol(argv[9], NULL, 10);
  }

  simple_example(d, size, hash_func, elastic, oneshot, threshold, scale, swaps,
                 priority);

  return 0;
}

*/
