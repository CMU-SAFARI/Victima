
#include "cache_cntlr.h"
#include "pwc.h"
#include "subsecond_time.h"
#include "memory_manager.h"
#include "pagetable_walker.h"
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <fstream>
#include "hashtable_baseline.h"


namespace ParametricDramDirectoryMSI{


    HashTablePTW::HashTablePTW(int _table_size_in_bits,int _small_page_size_in_bits,int _large_page_size_in_bits,int _small_page_percentage,
        Core* _core,ShmemPerfModel* _m_shmem_perf_model,PWC* pwc, bool pwc_enabled)
    :PageTableWalker(_core->getId(), 0, _m_shmem_perf_model, pwc, pwc_enabled),page_table_walks(0){

    
        this->table_size_in_bits=_table_size_in_bits;
        this->small_page_size_in_bits=_small_page_size_in_bits;
        this->large_page_size_in_bits=_large_page_size_in_bits;
        this->small_page_percentage=_small_page_percentage;
        this->core=_core;
        this->m_shmem_perf_model=_m_shmem_perf_model;  
        this->small_page_table=(entry *)malloc((UInt64)pow(2,table_size_in_bits)*sizeof(entry));

        for (UInt64 i = 0; i < (UInt64)pow(2,table_size_in_bits); i++)
        {
            this->small_page_table[i].empty=true;
            this->small_page_table[i].vpn=0;
            this->small_page_table[i].next_entry=NULL;
        }

        this->large_page_table=(entry *)malloc((UInt64)pow(2,table_size_in_bits)*sizeof(entry));
        for (UInt64 i = 0; i < (UInt64)pow(2,table_size_in_bits); i++)
        {
            this->large_page_table[i].empty=true;
            this->large_page_table[i].vpn=0;
            this->large_page_table[i].next_entry=NULL;
        }

        pagefaults_4KB = 0;
        pagefaults_2MB = 0;
        chain_per_entry_4KB = 0;
        chain_per_entry_2MB = 0;
        num_accesses_4KB = 0;
        num_accesses_2MB = 0;

        total_latency_4KB = SubsecondTime::Zero();
        total_latency_2MB = SubsecondTime::Zero();
        total_latency_page_fault = SubsecondTime::Zero();

        registerStatsMetric("hashtable_walker", core->getId(), "page_table_walks", &page_table_walks);
        registerStatsMetric("hashtable_walker", core->getId(), "page_faults_4KB", &pagefaults_4KB);
        registerStatsMetric("hashtable_walker", core->getId(), "page_faults_2MB", &pagefaults_2MB);
        registerStatsMetric("hashtable_walker", core->getId(), "conflicts_4KB", &chain_per_entry_4KB);
        registerStatsMetric("hashtable_walker", core->getId(), "conflicts_2MB", &chain_per_entry_2MB);
        registerStatsMetric("hashtable_walker", core->getId(), "accesses_4KB", &num_accesses_4KB);
        registerStatsMetric("hashtable_walker", core->getId(), "accesses_2MB", &num_accesses_2MB);
        registerStatsMetric("hashtable_walker", core->getId(), "total_latency_4KB", &total_latency_4KB);
        registerStatsMetric("hashtable_walker", core->getId(), "total_latency_2MB", &total_latency_2MB);
        registerStatsMetric("hashtable_walker", core->getId(), "total_latency_page_fault", &total_latency_page_fault);

    
    }

    int HashTablePTW::hash_function(IntPtr address){
        uint64 mask=0;
        for(int i=0 ;i<table_size_in_bits;i++){
            mask+=(int)pow(2,i);
        }
        char*  new_address = (char*) address;
        uint64 result=CityHash64((const char*)&address,8) & (mask);
        return result;
    }

    SubsecondTime HashTablePTW::access_cache(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache,
    CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, 
    UInt32 _data_length, bool modeled, bool count){

        SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);                
        IntPtr cache_address = ((IntPtr)(address)) & (~((64 - 1))); 
            cache->processMemOpFromCore(
                eip,
                lock_signal,
                Core::mem_op_t::READ,
                cache_address, 0,
                data_buf, data_length,
                modeled,
                count, CacheBlockInfo::block_type_t::PAGE_TABLE, SubsecondTime::Zero());

        SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);

        getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);
        mem_manager->tagCachesBlockType(address,CacheBlockInfo::block_type_t::PAGE_TABLE);
        
        return t_end-t_start;

    }


    int HashTablePTW::handle_page_fault(IntPtr address, int hash_function_result_4KB, int hash_function_result_2MB){

        int random_number=rand()%100;
        entry* page_table_entry;
        int page_size_in_bits;
        int allocated_size;
        if(random_number < small_page_percentage){
            pagefaults_4KB++;
            page_table_entry = &small_page_table[hash_function_result_4KB];
            page_size_in_bits=small_page_size_in_bits;
        }
        else{
            pagefaults_2MB++;
            page_table_entry = &large_page_table[hash_function_result_2MB];
            page_size_in_bits=large_page_size_in_bits;
        }

                 
        entry* current_entry_small = page_table_entry;

                 while(true){    
                    if (!(current_entry_small->empty) && (current_entry_small->next_entry == NULL)){
                        entry *_next_entry = (entry *) malloc (sizeof(entry));
                        _next_entry->empty=false;
                        _next_entry->vpn=address >> page_size_in_bits;
                        _next_entry->next_entry=NULL;
                        if(page_size_in_bits == small_page_size_in_bits) chain_per_entry_4KB++;
                        if(page_size_in_bits == large_page_size_in_bits) chain_per_entry_2MB++;

                        current_entry_small->next_entry=_next_entry;
                        break;
                    }
                    else if (!(current_entry_small->empty) && (current_entry_small->next_entry != NULL)){
                        current_entry_small=current_entry_small->next_entry;
                    }
                    else if (current_entry_small->empty){
                        current_entry_small->empty=false;
                        current_entry_small->vpn = address >> page_size_in_bits;
                        current_entry_small->next_entry= NULL;
                        break;
                    }
                 }
        
        return page_size_in_bits;
        

    }

    
    SubsecondTime HashTablePTW::init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache,
    CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, 
    UInt32 _data_length, bool modeled, bool count){
        
        page_table_walks++;
        SubsecondTime delay_4KB = SubsecondTime::Zero();
        SubsecondTime delay_2MB = SubsecondTime::Zero();

        int hash_function_result_4KB = hash_function(address >> small_page_size_in_bits);
        int hash_function_result_2MB = hash_function(address >> large_page_size_in_bits);

        entry *current_entry_small = &small_page_table[hash_function_result_4KB];
        entry *current_entry_large = &large_page_table[hash_function_result_2MB];

        bool found_it_4KB = false;
        bool found_it_2MB = false;


        int accesses_4KB = 0;

        while(true){

            delay_4KB+=access_cache(eip,address,shadow_cache,cache,lock_signal,_data_buf,_data_length,modeled,count);
            num_accesses_4KB++;

            if (!(current_entry_small->empty))
            {
                if(current_entry_small->vpn == (address >> small_page_size_in_bits))
                {
                    found_it_4KB = true;

                    break;
                }
                else{

                    if(current_entry_small->next_entry == NULL)
                        break;
                    else if(current_entry_small->next_entry != NULL){
                        current_entry_small=current_entry_small->next_entry;   
                    }
                }      
            }
            else break;
        }

        int accesses_2MB = 0;

        while(true){

            delay_2MB+=access_cache(eip,address,shadow_cache,cache,lock_signal,_data_buf,_data_length,modeled,count);
            num_accesses_2MB++;


            if (!(current_entry_large->empty))
            {
                if(current_entry_large->vpn == (address >> large_page_size_in_bits)){
                    found_it_2MB = true;
                    break;
                }
                else{

                    if(current_entry_large->next_entry == NULL)
                        break;
                    else if(current_entry_large->next_entry != NULL)
                        current_entry_large=current_entry_large->next_entry;   
                } 
            }
            else break;

        }

        if(found_it_4KB){
            total_latency_4KB+=delay_4KB;
            return delay_4KB;
        }
        else if(found_it_2MB){
            total_latency_2MB+=delay_2MB;
            return delay_2MB;
        }
        else{

            int pagesize = handle_page_fault(address,hash_function_result_4KB,hash_function_result_2MB);
            total_latency_page_fault = std::max(delay_4KB,delay_2MB);
            return std::max(delay_4KB,delay_2MB);
        }
        
        
    }
    bool HashTablePTW::isPageFault(IntPtr address){

        int hash_function_result_4KB = hash_function(address >> small_page_size_in_bits);
        int hash_function_result_2MB = hash_function(address >> large_page_size_in_bits);

        entry *current_entry_small = &small_page_table[hash_function_result_4KB];
        entry *current_entry_large = &large_page_table[hash_function_result_2MB];

        while(true){

            if(!(current_entry_small->empty))
            {
                if(current_entry_small->vpn == (address >> small_page_size_in_bits))
                {
                    return false;
                }
                else 
                {
                    if(current_entry_small->next_entry == NULL)
                        break;
                    else if(current_entry_small->next_entry != NULL)
                        current_entry_small=current_entry_small->next_entry;     
                }
            }
            else break;
        }
 
        while(true){
            
            if (!(current_entry_large->empty))
            {
                if(current_entry_large->vpn == (address >> large_page_size_in_bits)){
                    return false;
                }
                else
                {
                    if(current_entry_large->next_entry == NULL)
                        break;
                    else if(current_entry_large->next_entry != NULL)
                        current_entry_large=current_entry_large->next_entry;     
                }    
            }
            else break;
        }

        return true;
    }
        
    

    int HashTablePTW::init_walk_functional(IntPtr address){
            
            int hash_function_result_4KB = hash_function(address >> small_page_size_in_bits);
            int hash_function_result_2MB = hash_function(address >> large_page_size_in_bits);
    
            entry *current_entry_small = &small_page_table[hash_function_result_4KB];
            entry *current_entry_large = &large_page_table[hash_function_result_2MB];
    
            bool found_it_4KB = false;
            bool found_it_2MB = false;
    
            while(true){
    
                if (!(current_entry_small->empty))
                {
                    if(current_entry_small->vpn == (address >> small_page_size_in_bits))
                    {
                        found_it_4KB = true;
                        break;
                    }
                    else{

                        if(current_entry_small->next_entry == NULL)
                            break;
                        else if(current_entry_small->next_entry != NULL)
                            current_entry_small=current_entry_small->next_entry;   
                    }    
                }
                else break;
            }
    
            while(true){
                
                if (!(current_entry_large->empty))
                {
                    if(current_entry_large->vpn == (address >> large_page_size_in_bits)){
                        found_it_2MB = true;
                        break;       
                    }
                    else{
                        if(current_entry_large->next_entry == NULL)
                            break;
                        else if(current_entry_large->next_entry != NULL)
                            current_entry_large=current_entry_large->next_entry;   
                    }  
                }
                else break;
            }
    
            if(found_it_4KB){
                return small_page_size_in_bits;
            }
            else if(found_it_2MB){
                return large_page_size_in_bits;
            }
            else{
                return handle_page_fault(address,hash_function_result_4KB,hash_function_result_2MB);
            }
            
            
        }
        

}