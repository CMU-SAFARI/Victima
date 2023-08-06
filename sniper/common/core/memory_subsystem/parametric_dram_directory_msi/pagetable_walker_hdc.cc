
#include "cache_cntlr.h"
#include "pwc.h"
#include "subsecond_time.h"
#include "config.hpp"
#include "simulator.h"
#include "memory_manager.h"
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <fstream>
#include "pagetable_walker_hdc.h"

namespace ParametricDramDirectoryMSI{

    PageTableHashDontCache::PageTableHashDontCache(int _table_size_in_bits,int _small_page_size_in_bits,int _large_page_size_in_bits,int _small_page_percentage,
        Core* _core,ShmemPerfModel* _m_shmem_perf_model,PWC* pwc, bool pwc_enabled)
    :PageTableWalker(_core->getId(), 0, _m_shmem_perf_model, pwc, pwc_enabled){



        this->table_size_in_bits=_table_size_in_bits;
        this->small_page_size_in_bits=_small_page_size_in_bits;
        this->large_page_size_in_bits=_large_page_size_in_bits;
        this->small_page_percentage=_small_page_percentage;
        this->core=_core;
        this->m_shmem_perf_model=_m_shmem_perf_model;
        this->small_page_table=(entry *)malloc((UInt64)pow(2,table_size_in_bits)*sizeof(entry));

        this->pagefaults_4KB = 0;
        this->pagefaults_2MB = 0;
        this->chain_per_entry_4KB = 0;
        this->chain_per_entry_2MB = 0;
        this->num_accesses_4KB = 0;
        this->num_accesses_2MB = 0;

        total_latency_4KB = SubsecondTime::Zero();
        total_latency_2MB = SubsecondTime::Zero();

        std::cout << "Allocating " << (UInt64)pow(2,table_size_in_bits) << " entries" << std::endl;
        
        for (UInt64 i = 0; i < (UInt64)pow(2,table_size_in_bits); i++)
        {
            this->small_page_table[i].empty=true;
            this->small_page_table[i].vpn=0;
            this->small_page_table[i].ptes[0]=0;
            this->small_page_table[i].ptes[1]=0;
            this->small_page_table[i].ptes[2]=0;
            this->small_page_table[i].ptes[3]=0;
        }

        this->large_page_table=(entry *)malloc((UInt64)pow(2,table_size_in_bits)*sizeof(entry));
        for (UInt64 i = 0; i < (UInt64)pow(2,table_size_in_bits); i++)
        {
            this->large_page_table[i].empty=true;
            this->large_page_table[i].vpn=0;
            this->large_page_table[i].ptes[0]=0;
            this->large_page_table[i].ptes[1]=0;
            this->large_page_table[i].ptes[2]=0;
            this->large_page_table[i].ptes[3]=0;
        }

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

    int PageTableHashDontCache::hash_function(IntPtr address){
        uint64 mask=0;
        for(int i=0 ;i<table_size_in_bits;i++){
            mask+=(int)pow(2,i);
        }
        char*  new_address = (char*) address;
        uint64 result=CityHash64((const char*)&address,8) & (mask);
        return result;
    }

    SubsecondTime PageTableHashDontCache::access_cache(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache,
    CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* data_buf, 
    UInt32 data_length, bool modeled, bool count){

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



    int PageTableHashDontCache::handle_page_fault(IntPtr address, int hash_function_result_4KB, int hash_function_result_2MB){

        int random_number=rand()%100;
        entry* page_table_entry;
        int page_size_in_bits;
        int allocated_size;
        bool allocated_2MB=false;
        bool allocated_4KB=false;

        if(random_number < small_page_percentage){
            allocated_4KB = true;
            pagefaults_4KB++;
            page_table_entry = &small_page_table[hash_function_result_4KB];
            page_size_in_bits=small_page_size_in_bits;
        }
        else{
            allocated_2MB = true;
            pagefaults_2MB++;
            page_table_entry = &large_page_table[hash_function_result_2MB];
            page_size_in_bits=large_page_size_in_bits;
        }

                 
        entry* current_entry = page_table_entry;

        while(true){    

            if (!(current_entry->empty) ){

                    if(allocated_4KB){
                        chain_per_entry_4KB++;
                        hash_function_result_4KB++;
                        page_table_entry = &small_page_table[hash_function_result_4KB];
                    }
                    else{
                        chain_per_entry_2MB++;
                        hash_function_result_2MB++;
                        page_table_entry = &large_page_table[hash_function_result_2MB];
                    }

            }
            else if (current_entry->empty){

                        current_entry->empty=false;
                        current_entry->vpn = address >> page_size_in_bits;
                        break;
            }

        }

        return page_size_in_bits;

    }
    
    SubsecondTime PageTableHashDontCache::init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache,
    CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* data_buf, 
    UInt32 data_length, bool modeled, bool count){


        page_table_walks++;

        int hash_function_result_4KB = hash_function(address >> small_page_size_in_bits);
        int hash_function_result_2MB = hash_function(address >> large_page_size_in_bits);
    
        int initial_hash_function_result_4KB = hash_function_result_4KB;
        int initial_hash_function_result_2MB = hash_function_result_2MB;

        entry *current_entry_small = &small_page_table[hash_function_result_4KB];
        entry *current_entry_large = &large_page_table[hash_function_result_2MB];

        bool found_it_4KB = false;
        bool found_it_2MB = false;

        SubsecondTime delay_4KB = SubsecondTime::Zero();
        SubsecondTime delay_2MB = SubsecondTime::Zero();

        //std::cout << "Walk address: " << address << std::endl;

      
        while(true){
           //std::cout << "4KB walk" << std::endl;

            delay_4KB+= access_cache(eip,address,shadow_cache,cache,lock_signal,data_buf,data_length,modeled,count);
            num_accesses_4KB++;
            if(current_entry_small->empty){
                //std::cout << "4KB page fault" << std::endl;
                break;
            }
            else if(current_entry_small->vpn == (address >> small_page_size_in_bits)){
                //std::cout << "4KB found it" << std::endl;
                found_it_4KB = true;
                break;
            }
            else{
                //std::cout << "4KB chain" << std::endl;
                hash_function_result_4KB++;
                current_entry_small = &small_page_table[hash_function_result_4KB];
            }
        }
        

        while(true){

            delay_2MB+= access_cache(eip,address,shadow_cache,cache,lock_signal,data_buf,data_length,modeled,count);
            num_accesses_2MB++;

            if(current_entry_large->empty){
                break;
            }
            else if(current_entry_large->vpn == (address >> large_page_size_in_bits)){
                found_it_2MB = true;
                break;
            }
            else{
                hash_function_result_2MB++;
                current_entry_large = &large_page_table[hash_function_result_2MB];
            }
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

            handle_page_fault(address,initial_hash_function_result_4KB,initial_hash_function_result_2MB);
            total_latency_page_fault = std::max(delay_4KB,delay_2MB);
            return std::max(delay_4KB,delay_2MB);
        }

        
        
    }
    bool PageTableHashDontCache::isPageFault(IntPtr address){
   
        int hash_function_result_4KB = hash_function(address >> small_page_size_in_bits);
        int hash_function_result_2MB = hash_function(address >> large_page_size_in_bits);
    
        entry *current_entry_small = &small_page_table[hash_function_result_4KB];
        entry *current_entry_large = &large_page_table[hash_function_result_2MB];

        bool found_it_4KB = false;
        bool found_it_2MB = false;
      
        while(true){

            if(current_entry_small->empty){
                break;
            }
            else if(current_entry_small->vpn == (address >> small_page_size_in_bits)){
                found_it_4KB = true;
                break;
            }
            else{
                hash_function_result_4KB++;
                current_entry_small = &small_page_table[hash_function_result_4KB];
            }
        }

        while(true){
            if(current_entry_large->empty){
                break;
            }
            else if(current_entry_large->vpn == (address >> large_page_size_in_bits)){
                found_it_2MB = true;
                break;
            }
            else{
                hash_function_result_2MB++;
                current_entry_large = &large_page_table[hash_function_result_2MB];
            }
        }

        if (found_it_4KB){
            return false;
        }
        else if (found_it_2MB){
            return false;
        }
        else{
            return true;
        }



    }


    int PageTableHashDontCache::init_walk_functional(IntPtr address){

            int hash_function_result_4KB = hash_function(address >> small_page_size_in_bits);
            int hash_function_result_2MB = hash_function(address >> large_page_size_in_bits);

            int initial_hash_function_result_4KB = hash_function_result_4KB;
            int initial_hash_function_result_2MB = hash_function_result_2MB;
            
            entry *current_entry_small = &small_page_table[hash_function_result_4KB];
            entry *current_entry_large = &large_page_table[hash_function_result_2MB];

            bool found_it_4KB = false;
            bool found_it_2MB = false;
      
            while(true){
                if(current_entry_small->empty){
                    break;
                }
                else if(current_entry_small->vpn == (address >> small_page_size_in_bits)){
                    found_it_4KB = true;
                    break;
                }
                else{
                    hash_function_result_4KB++;
                    current_entry_small = &small_page_table[hash_function_result_4KB];
                }
            }


            while(true){
                if(current_entry_large->empty){
                    break;
                }
                else if(current_entry_large->vpn == (address >> large_page_size_in_bits)){
                    found_it_2MB = true;
                    break;
                }
                else{
                    hash_function_result_2MB++;
                    current_entry_large = &large_page_table[hash_function_result_2MB];
                }
            }
    

            if (found_it_4KB){
                return small_page_size_in_bits;
            }
            else if (found_it_2MB){
                return large_page_size_in_bits;
            }
            else{
                return handle_page_fault(address, initial_hash_function_result_4KB,initial_hash_function_result_2MB);
            }
    }   

}