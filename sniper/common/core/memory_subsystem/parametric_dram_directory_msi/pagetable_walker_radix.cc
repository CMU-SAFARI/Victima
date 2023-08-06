#include "pagetable_walker_radix.h"
#include "cache_cntlr.h"
#include "pwc.h"
#include "subsecond_time.h"
#include <math.h> 
#include <fstream>
#include <stdlib.h>
#include <time.h> 


namespace ParametricDramDirectoryMSI{

    int PageTableWalkerRadix::counter = 0;

    PageTableWalkerRadix::PageTableWalkerRadix(int number_of_levels, 
                                                   Core* _core, ShmemPerfModel* _m_shmem_perf_model, 
                                                   int *level_bit_indices,int *level_percentages, PWC* pwc, bool pwc_enabled,UtopiaCache* _shadow_cache)
    :PageTableWalker(_core->getId(), 0, _m_shmem_perf_model, pwc, pwc_enabled)
    {
        if(level_bit_indices!=NULL){
            this->shadow_cache=_shadow_cache;
            this->core =_core;
            this->m_shmem_perf_model=_m_shmem_perf_model;
            this->stats_radix.number_of_levels=number_of_levels;
            this->stats_radix.address_bit_indices=(int *)malloc((number_of_levels+1)*sizeof(int));
            this->stats_radix.hit_percentages=(int *)malloc((number_of_levels+1)*sizeof(int));
            for (int i = 0; i < number_of_levels+1; i++)
            {
                if(i<number_of_levels){
                    this->stats_radix.hit_percentages[i]=level_percentages[i];
                }
                this->stats_radix.address_bit_indices[i]=level_bit_indices[i];
            }
            this->starting_table=InitiateTablePtw((int)pow(2.0,(float)level_bit_indices[0]));
        }
        latency_per_level = new SubsecondTime[number_of_levels];
        String name = "ptw_radix_";
        name = name+std::to_string(counter).c_str();
        for (int i = 0; i < number_of_levels+1; i++){
            String metric_name = "page_level_latency_";
            String metric = metric_name+std::to_string(i).c_str();
            registerStatsMetric(name, core_id, metric, &latency_per_level[i]);        
        }
        
        counter++;

        
    }

    SubsecondTime PageTableWalkerRadix::init_walk(IntPtr eip, IntPtr address,
        UtopiaCache* shadow_cache,
        CacheCntlr *_cache,
        Core::lock_signal_t lock_signal,
        Byte* data_buf, UInt32 data_length,
        bool modeled, bool count){

            addresses.clear();
            cache = _cache;
            stats.page_walks++;

            uint64_t a1;
            int shift_bits=0;
            SubsecondTime total_latency;
            SubsecondTime t_start;
            SubsecondTime now = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);


            for (int i = stats_radix.number_of_levels; i >= 1; i--)
            {
                shift_bits+=stats_radix.address_bit_indices[i];
            }
            a1=((address>>shift_bits))&0x1ff;

            bool pwc_hit = false;

            if(page_walk_cache_enabled){ //@kanellok access page walk caches 

			    PWC::where_t pwc_where;

			    if(page_walk_cache_enabled)
                {
                    IntPtr pwc_address = (IntPtr)(&starting_table->entries[a1]);
                    pwc_where = pwc->lookup(pwc_address, t_start ,true, 1, count);
                    if( pwc_where == PWC::HIT ) pwc_hit = true; 

                }
           
            }

            if(pwc_hit == true){

                    total_latency = pwc->access_latency.getLatency(); 

            }
            else{

                    t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                    
                    IntPtr cache_address = ((IntPtr)(&starting_table->entries[a1])) & (~((64 - 1))); 

                    cache->processMemOpFromCore(
                        eip,
                        lock_signal,
                        Core::mem_op_t::READ,
                        cache_address, 0,
                        data_buf, data_length,
                        modeled,
                        count, CacheBlockInfo::block_type_t::PAGE_TABLE, SubsecondTime::Zero(),shadow_cache);
                   

                    addresses.push_back((IntPtr)(&starting_table->entries[a1]));

                    SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                    if(page_walk_cache_enabled)
                        total_latency = t_end - t_start + pwc->miss_latency.getLatency();
                    else
                        total_latency = t_end - t_start;

                    m_shmem_perf_model->setElapsedTime(ShmemPerfModel::_USER_THREAD,now);


                    mem_manager->tagCachesBlockType(cache_address,CacheBlockInfo::block_type_t::PAGE_TABLE);

                    latency_per_level[0] += total_latency;
                    
            }
		

            if(starting_table->entries[a1].entry_type==ptw_table_entry_type::PTW_NONE){
                starting_table->entries[a1]=*CreateNewPtwEntryAtLevel(1,stats_radix.number_of_levels,stats_radix.address_bit_indices,stats_radix.hit_percentages,this,address);
            }
            if(starting_table->entries[a1].entry_type==ptw_table_entry_type::PTW_ADDRESS){
                //std::cout<<std::hex<<address<<" - "<<std::hex<<a1<<" - "<<level<<" Address\n";
                latency_per_level[0] += total_latency;
                return total_latency;
            }

            
            SubsecondTime final_latency = total_latency+InitializeWalkRecursive(eip, address,2,starting_table->entries[a1].next_level_table,lock_signal,data_buf,data_length, modeled, count);
            
            m_shmem_perf_model->setElapsedTime(ShmemPerfModel::_USER_THREAD,now);
            UInt64 vpn = address >> init_walk_functional(address);
            track_per_page_ptw_latency(vpn,final_latency);


            return final_latency;

    }

    SubsecondTime PageTableWalkerRadix::InitializeWalkRecursive(IntPtr eip, uint64_t address,
        int level,ptw_table* new_table,
        Core::lock_signal_t lock_signal,
        Byte* data_buf, UInt32 data_length,
        bool modeled, bool count){

            uint64_t a1;
            int shift_bits=0;
            IntPtr pwc_address;
            SubsecondTime t_start;
            SubsecondTime total_latency;
            
            for (int i = stats_radix.number_of_levels; i >= level; i--)
            {
                shift_bits+=stats_radix.address_bit_indices[i];
            }

            a1=((address>>shift_bits))&0x1ff;

            bool pwc_hit = false;

            if(page_walk_cache_enabled){ //@kanellok access page walk caches 

			    PWC::where_t pwc_where;

			    if(page_walk_cache_enabled && level != (stats_radix.number_of_levels) )
                {
                    pwc_address = (IntPtr)(&new_table->entries[a1]);
                    pwc_where = pwc->lookup(pwc_address, t_start ,true, level, count);
                    if( pwc_where == PWC::HIT ) pwc_hit = true; 

                }
            }
		
            if(pwc_hit == true){

                    total_latency = pwc->access_latency.getLatency(); 

            }
            else{
                    
                    t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                    
                    IntPtr cache_address = ((IntPtr)(&new_table->entries[a1])) & (~((64 - 1))); 

                    cache->processMemOpFromCore(
                        eip, 
                        lock_signal,
                        Core::mem_op_t::READ,
                        cache_address, 0,
                        data_buf, data_length,
                        modeled,
                        count, CacheBlockInfo::block_type_t::PAGE_TABLE, SubsecondTime::Zero());

                    SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                    
                    if(page_walk_cache_enabled)
                        total_latency = t_end - t_start + pwc->miss_latency.getLatency();
                    else
                        total_latency = t_end - t_start;
                    
                    addresses.push_back((IntPtr)(&new_table->entries[a1]));

                    mem_manager->tagCachesBlockType(cache_address,CacheBlockInfo::block_type_t::PAGE_TABLE);
                    latency_per_level[level-1] += total_latency;
                    
             }
		    


            if(new_table->entries[a1].entry_type==ptw_table_entry_type::PTW_NONE){
                new_table->entries[a1]=*CreateNewPtwEntryAtLevel(level,stats_radix.number_of_levels,stats_radix.address_bit_indices,stats_radix.hit_percentages,this, address);
            }
            if(new_table->entries[a1].entry_type==ptw_table_entry_type::PTW_ADDRESS){
                //std::cout<<std::hex<<address<<" - "<<std::hex<<a1<<" - "<<level<<" Address\n";
                return total_latency;
            }
            
            
            
            return total_latency+InitializeWalkRecursive(eip,address,level+1,new_table->entries[a1].next_level_table,lock_signal,data_buf,data_length,modeled,count);
        
    }
    int PageTableWalkerRadix::init_walk_functional(IntPtr address){
        uint64_t a1;
        int shift_bits=0;
        //std::cout << "Address  = " << address << "Number of levels" << stats_radix.number_of_levels << std::endl;
        
        for (int i = stats_radix.number_of_levels; i >= 1; i--)
        {
            shift_bits+=stats_radix.address_bit_indices[i];
        }
        a1=((address>>shift_bits))&0x1ff;
        if(starting_table->entries[a1].entry_type==ptw_table_entry_type::PTW_NONE){
            starting_table->entries[a1]=*CreateNewPtwEntryAtLevel(1,stats_radix.number_of_levels,stats_radix.address_bit_indices,stats_radix.hit_percentages,this, address);
        }
        if(starting_table->entries[a1].entry_type==ptw_table_entry_type::PTW_ADDRESS){
            
                int page_size=0;
                for(int i=stats_radix.number_of_levels;i>0;i--){
                    page_size+=stats_radix.address_bit_indices[i];
                }
                return (int)(page_size);
        }
        return init_walk_recursive_functional(address,2,starting_table->entries[a1].next_level_table);
    }
    int PageTableWalkerRadix::init_walk_recursive_functional(uint64_t address,int level,ptw_table* new_table){
        uint64_t a1;
        int shift_bits=0;
       // std::cout << "Level  = " << level << std::endl;
        for (int i = stats_radix.number_of_levels; i >= level; i--)
        {
            shift_bits+=stats_radix.address_bit_indices[i];
        }
        a1=((address>>shift_bits))&0x1ff;

       // std::cout << "EntryType = " << new_table->entries[a1].entry_type << std::endl;

        if(new_table->entries[a1].entry_type==ptw_table_entry_type::PTW_NONE){
            new_table->entries[a1]=*CreateNewPtwEntryAtLevel(level,stats_radix.number_of_levels,stats_radix.address_bit_indices,stats_radix.hit_percentages,this, address);
        }
        if(new_table->entries[a1].entry_type==ptw_table_entry_type::PTW_ADDRESS){
            int page_size=0;
            for(int i=stats_radix.number_of_levels;i>level-1;i--){
                page_size+=stats_radix.address_bit_indices[i];
            }
            return (int)(page_size);
            
        }
        return init_walk_recursive_functional(address,level+1,new_table->entries[a1].next_level_table);
    }

    bool PageTableWalkerRadix::isPageFault(IntPtr address){
        uint64_t a1;
        int shift_bits=0;
        //std::cout << "Address  = " << address << "Number of levels" << stats_radix.number_of_levels << std::endl;
        
        for (int i = stats_radix.number_of_levels; i >= 1; i--)
        {
            shift_bits+=stats_radix.address_bit_indices[i];
        }
        a1=((address>>shift_bits))&0x1ff;

        if(starting_table->entries[a1].entry_type==ptw_table_entry_type::PTW_NONE){
            return true;
        }
        if(starting_table->entries[a1].entry_type==ptw_table_entry_type::PTW_ADDRESS){
            return false;
        }

        return isPageFaultHelper(address,2,starting_table->entries[a1].next_level_table);


    }

    bool PageTableWalkerRadix::isPageFaultHelper(uint64_t address,int level,ptw_table* new_table){

        uint64_t a1;
        int shift_bits=0;
       // std::cout << "Level  = " << level << std::endl;
        for (int i = stats_radix.number_of_levels; i >= level; i--)
        {
            shift_bits+=stats_radix.address_bit_indices[i];
        }
        a1=((address>>shift_bits))&0x1ff;

       // std::cout << "EntryType = " << new_table->entries[a1].entry_type << std::endl;

        if(new_table->entries[a1].entry_type==ptw_table_entry_type::PTW_NONE){
            return true;
        }
        if(new_table->entries[a1].entry_type==ptw_table_entry_type::PTW_ADDRESS){

            return false;
            
        }
        return isPageFaultHelper(address,level+1,new_table->entries[a1].next_level_table);

    }


}