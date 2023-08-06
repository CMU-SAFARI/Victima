#ifndef PTWT_H
#define PTWT_H

#include<iostream>
#include "pagetable_walker.h"

using namespace ParametricDramDirectoryMSI;
enum ptw_table_entry_type{
    PTW_ADDRESS,
    PTW_TABLE_POINTER,
    PTW_NONE,
    PTW_NUMBER_OF_ENTRY_TYPES
};

struct ptw_table_entry {
    ptw_table_entry_type entry_type;
    struct ptw_table* next_level_table;
};
struct ptw_table{
    struct ptw_table_entry* entries;
    int table_size;
};

ptw_table* InitiateTablePtw(int size);
ptw_table_entry* CreateNewPtwEntryAtLevel(int level,int number_of_levels,int *level_indices,int *level_percentages,PageTableWalker *ptw, IntPtr address);

#endif 