#include "page_table_walker_types.h"
#include "allocation_manager.h"
#include <math.h> 
#include <stdlib.h>
#include <time.h> 
using namespace ParametricDramDirectoryMSI;

ptw_table_entry* CreateNewPtwEntryAtLevel(int level,int number_of_levels,int *level_bit_indices,int *level_percentages,PageTableWalker *ptw, IntPtr address){

    ptw_table_entry* e=(ptw_table_entry*)malloc(sizeof(ptw_table_entry));
    
    if(level!=number_of_levels){
           
        if( (level == number_of_levels-1)){

            // Check if address is aligned at 2Mb

            int page_size = Sim()->getAllocationManager()->defaultPageFaultHandler();     
            if(page_size == 21 && (address % (int)pow(2.0,(int)level_bit_indices[level]) == 0) ){
                e->entry_type=ptw_table_entry_type::PTW_ADDRESS;
                ptw->stats.number_of_2MB_pages++;
                //std::cout << "Inserting 2MB: " << ptw->stats.number_of_2MB_pages << std::endl;
            }
            else{
                e->entry_type=ptw_table_entry_type::PTW_TABLE_POINTER;
                e->next_level_table=InitiateTablePtw((int)pow(2.0,(float)level_bit_indices[level]));
            }
        }
        else{
            e->entry_type=ptw_table_entry_type::PTW_TABLE_POINTER;
            e->next_level_table=InitiateTablePtw((int)pow(2.0,(float)level_bit_indices[level]));
        }
        
    }
    else{
        e->entry_type=ptw_table_entry_type::PTW_ADDRESS;
    }
    return e;
}

ptw_table* InitiateTablePtw(int size){
    
    ptw_table* t=(ptw_table*)malloc(sizeof(ptw_table));
    t->table_size=size;
    
    t->entries=(ptw_table_entry*)malloc(size*sizeof(ptw_table_entry));
    for (int i = 0; i < size; i++)
    {
        t->entries[i].entry_type=ptw_table_entry_type::PTW_NONE;
    }
    
    return t;
}