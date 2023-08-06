# Elastic Cuckoo Hashing
This repository contains a simple reference implementation of (elastic) cuckoo hashing. 

Please check our paper for details:

```
@inproceedings{Skarlatos:ElasticCuckooPageTables:ASPLOS20,
author = {Skarlatos, Dimitrios and Kokolis, Apostolos and Xu, Tianyin and Torrellas, Josep},
title = {Elastic Cuckoo Page Tables: Rethinking Virtual Memory Translation for Parallelism},
year = {2020},
publisher = {Association for Computing Machinery},
booktitle = {Proceedings of the Twenty-Fifth International Conference on Architectural Support for Programming Languages and Operating Systems},
location = {Lausanne, Switzerland},
series = {ASPLOS ’20}
}
```

[You can find the Elastic Cuckoo Page Tables paper here!](http://skarlat2.web.engr.illinois.edu/publications/cuckoo_asplos20.pdf)

## How to run

1) make
2) ./elastic_cuckoo ${#ways} ${size} ${hash_func} ${type} ${scheme} ${occupancy_thr} ${scale_factor} ${swaps} ${priority}

Parameters:
1) ways: number of ways/nests >2
2) size: size of each way
3) hash_func: string of the hash function name to be used {city,blake2}
4) type: type of cuckoo hashtable {elastic, regular}
5) scheme: resizing method for the elastic cuckoo hashtable {oneshot, dynamic}
6) occupancy_thr: threshold to trigger resizing 
7) swaps: number of elements to be rehashed
8) priority: bias a hashtable during insertion

## Hash functions

1) Blake2 from https://github.com/BLAKE2/BLAKE2
2) CityHash from https://github.com/google/cityhash

## Notes
The purpose of this code is to demonstrate some of the characteristics of (elastic) cuckoo hashing and it has not been optimized for performance.
