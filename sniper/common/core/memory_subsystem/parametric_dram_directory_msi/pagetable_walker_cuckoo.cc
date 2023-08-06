#include "pagetable_walker_cuckoo.h"
#include "allocation_manager.h"
#include "core_manager.h"
#include "simulator.h"
#include "config.hpp"


namespace ParametricDramDirectoryMSI{

    PageTableWalkerCuckoo::PageTableWalkerCuckoo(int core_id, int _page_size, ShmemPerfModel* _m_shmem_perf_model, PWC* _pwc, 
												bool _page_walk_cache_enabled, int d, char* hash_func, int size,
												float rehash_threshold, uint8_t scale,
                    							uint8_t swaps, uint8_t priority):
        PageTableWalker(core_id, _page_size, _m_shmem_perf_model, _pwc, _page_walk_cache_enabled)
    {
		std::cout << "Instantiating Cuckoo Page Table" << std::endl;
		create_elastic(d, size, &elasticCuckooHT_4KB, hash_func, rehash_threshold,
                   scale, swaps, priority); //4KB cuckoo
		create_elastic(d, size, &elasticCuckooHT_2MB, hash_func, rehash_threshold,
                   scale, swaps, priority); 
		String name =  "PTW_cuckoo";
		cuckoo_faults = 0;    
		cuckoo_latency = SubsecondTime::Zero();
		cuckoo_hits = 0;
		
		allocation_percentage_2MB = Sim()->getCfg()->getInt("perf_model/ptw_cuckoo/percentage_2MB");

		registerStatsMetric(name, core_id, "ptws", &stats.page_walks);
		registerStatsMetric(name, core_id, "hits", &cuckoo_hits);
		registerStatsMetric(name, core_id, "pagefaults", &cuckoo_faults);
		registerStatsMetric(name, core_id, "latency", &cuckoo_latency);


    }

    SubsecondTime PageTableWalkerCuckoo::init_walk(IntPtr _eip, IntPtr address, UtopiaCache* shadow_cache, CacheCntlr *_cache, Core::lock_signal_t _lock_signal, Byte* _data_buf, UInt32 _data_length, bool _modeled, bool _count)
	{
		// Setup the step walk functions
		stats.page_walks++;
		bzero(&current_lat, sizeof(current_lat));

		cache = _cache;
		modeled = _modeled;
		count = _count;
		eip = _eip;
		data_buf = _data_buf;
		vpn = address >> page_size;
		data_length = _data_length;
		lock_signal = _lock_signal;
		latency = SubsecondTime::Zero();
		
		//printf("Page Walk latency %ld \n",SubsecondTime::divideRounded(current_lat,period));

		accessTable(address);

		cuckoo_latency += latency;

		return latency;
	};

    void PageTableWalkerCuckoo::accessTable(IntPtr address)
    {
		 elem_t elem_4KB;
		 elem_4KB.valid = 1;
		 elem_4KB.value = address >> 15; // We can assume 8 PTE entries/hash set
		 std::vector<elem_t> accessedAddresses_4KB = find_elastic_ptw(&elem_4KB, &elasticCuckooHT_4KB);

		 elem_t elem_2MB;
		 elem_2MB.valid = 1;
		 elem_2MB.value = address >> 24; // We can assume 8 PTE entries/hash set
		 std::vector<elem_t> accessedAddresses_2MB = find_elastic_ptw(&elem_2MB, &elasticCuckooHT_2MB);



		bool found = false;
		bool found4KB = false;
		bool found2MB = false;

		int nest_hit =0;


		for(elem_t elem: accessedAddresses_4KB){
			
			if(elem.valid)
			{
				found = true;
				found4KB = true;
				cuckoo_hits++;
				break;
			}
		}

		for(elem_t elem: accessedAddresses_2MB){
			
			if(elem.valid)
			{
				found = true;
				found2MB = true;
				cuckoo_hits++;
				break;
			}
		}

		if(!found){

			int temp = Sim()->getAllocationManager()->defaultPageFaultHandler();
			
			if( temp == 21 && (address % (int)pow(2,21) == 0)){
				insert_elastic(&elem_2MB,&elasticCuckooHT_2MB,0,0);
				evaluate_elasticity(&elasticCuckooHT_2MB,0);
			 	stats.number_of_2MB_pages++;
				std::cout << "Inserting 2MB: " << stats.number_of_2MB_pages << std::endl;
			
			}
			else{
				insert_elastic(&elem_4KB,&elasticCuckooHT_4KB,0,0);
				evaluate_elasticity(&elasticCuckooHT_4KB,0);

			}

  		}

		if(!found) cuckoo_faults++;

		SubsecondTime maxLat = SubsecondTime::Zero();
		SubsecondTime final_latency = SubsecondTime::Zero();

		SubsecondTime now = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
		
		//std::cout << "Starting cuckoo request" << std::endl;
		if(found4KB || !found){
			for(elem_t addr: accessedAddresses_4KB)
			{
				SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
				IntPtr cache_address = addr.value & (~((64 - 1))); 
				
				cache->processMemOpFromCore(
					eip,
					lock_signal,
					Core::mem_op_t::READ,
					cache_address, 0,
					data_buf, data_length,
					modeled,
					count,CacheBlockInfo::block_type_t::PAGE_TABLE, SubsecondTime::Zero());

					if(nuca){

						nuca->markTranslationMetadata(addr.value,CacheBlockInfo::block_type_t::PAGE_TABLE);
						mem_manager->getCache(MemComponent::component_t::L1_DCACHE)->markMetadata(addr.value,CacheBlockInfo::block_type_t::PAGE_TABLE);
            			mem_manager->getCache(MemComponent::component_t::L2_CACHE)->markMetadata(addr.value,CacheBlockInfo::block_type_t::PAGE_TABLE);
					}

				SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
				getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD , now);


				ComponentLatency hash_latency = ComponentLatency(Sim()->getCoreManager()->getCoreFromID(core_id)->getDvfsDomain(),0); // Hash-function latency

				ComponentLatency pwc_latency = ComponentLatency(Sim()->getCoreManager()->getCoreFromID(core_id)->getDvfsDomain(),0); // We assume that the cache always finds the 4KB way or the 2MB way 

				SubsecondTime period = SubsecondTime::SEC() / (2.6*1000000000);
				UInt64 cycles = SubsecondTime::divideRounded(t_end - t_start , period);
				
				//std::cout << "Request of cuckoo 4KB latency = " << cycles << std::endl;	
				
				if(!found && ( (t_end - t_start+ hash_latency.getLatency()+pwc_latency.getLatency() ) > maxLat))
					maxLat = (t_end - t_start)+hash_latency.getLatency()+pwc_latency.getLatency();
				
				if(found && addr.valid){
					final_latency =  t_end - t_start+hash_latency.getLatency()+pwc_latency.getLatency();
				}
			}
		}
		
		if(found2MB || !found){
			for(elem_t addr: accessedAddresses_2MB)
			{
				SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
				IntPtr cache_address = addr.value & (~((64 - 1))); 
				
				cache->processMemOpFromCore(
					eip,
					lock_signal,
					Core::mem_op_t::READ,
					cache_address, 0,
					data_buf, data_length,
					modeled,
					count,CacheBlockInfo::block_type_t::PAGE_TABLE, SubsecondTime::Zero());

					if(nuca){

						nuca->markTranslationMetadata(addr.value,CacheBlockInfo::block_type_t::PAGE_TABLE);
						mem_manager->getCache(MemComponent::component_t::L1_DCACHE)->markMetadata(addr.value,CacheBlockInfo::block_type_t::PAGE_TABLE);
            			mem_manager->getCache(MemComponent::component_t::L2_CACHE)->markMetadata(addr.value,CacheBlockInfo::block_type_t::PAGE_TABLE);
					}

				SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);	
				getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD , now);

				ComponentLatency hash_latency = ComponentLatency(Sim()->getCoreManager()->getCoreFromID(core_id)->getDvfsDomain(),0); // Hash-function latency

				ComponentLatency pwc_latency = ComponentLatency(Sim()->getCoreManager()->getCoreFromID(core_id)->getDvfsDomain(),0); // We assume that the cache always finds the 4KB way or the 2MB way 

				SubsecondTime period = SubsecondTime::SEC() / (2.6*1000000000);
				UInt64 cycles = SubsecondTime::divideRounded(t_end - t_start + hash_latency.getLatency()+pwc_latency.getLatency(), period);
			//	std::cout << "Request of cuckoo 2MB latency = " << cycles << std::endl;
				
				if(!found && ( (t_end - t_start+ hash_latency.getLatency()+pwc_latency.getLatency() ) > maxLat))
					maxLat = (t_end - t_start)+hash_latency.getLatency()+pwc_latency.getLatency();
				
				if(found && addr.valid){
					final_latency =  t_end - t_start+hash_latency.getLatency()+pwc_latency.getLatency();

				}
			}
		}


		current_lat = maxLat;

		if(found){
			SubsecondTime period = SubsecondTime::SEC() / (2.6*1000000000);
			UInt64 cycles = SubsecondTime::divideRounded(final_latency, period);
			//std::cout << "Found with final latency = " << cycles << std::endl;
			latency = final_latency;

		}
		else{
		//	std::cout << "Not found with final latency = " << maxLat << std::endl;
			latency = maxLat;
		
		}



    }
	int PageTableWalkerCuckoo::init_walk_functional(IntPtr address){

		elem_t elem_4KB;
		 elem_4KB.valid = 1;
		 elem_4KB.value = address >> 15;
		 std::vector<elem_t> accessedAddresses_4KB = find_elastic_ptw(&elem_4KB, &elasticCuckooHT_4KB);

		 elem_t elem_2MB;
		 elem_2MB.valid = 1;
		 elem_2MB.value = address >> 24;
		 std::vector<elem_t> accessedAddresses_2MB = find_elastic_ptw(&elem_2MB, &elasticCuckooHT_2MB);

		bool found = false;

		for(elem_t elem: accessedAddresses_4KB){
			
			if(elem.valid)
			{
				found = true;
				return 12;
			}
		}

		for(elem_t elem: accessedAddresses_2MB){
			
			if(elem.valid)
			{
				found = true;
				return 21;
			}
		}

			if(!found){

				int temp = Sim()->getAllocationManager()->defaultPageFaultHandler();

				if( temp ==21 && (address % (int)pow(2,21) == 0) ){

					insert_elastic(&elem_2MB,&elasticCuckooHT_2MB,0,0);
					evaluate_elasticity(&elasticCuckooHT_2MB,0);
					stats.number_of_2MB_pages++;
					std::cout << "Inserting 2MB: " << stats.number_of_2MB_pages << std::endl;

				}
				else{

					insert_elastic(&elem_4KB,&elasticCuckooHT_4KB,0,0);
					evaluate_elasticity(&elasticCuckooHT_4KB,0);


				}
  		}

		return found;
	}


	bool PageTableWalkerCuckoo::isPageFault(IntPtr address){

		elem_t elem_4KB;
		 elem_4KB.valid = 1;
		 elem_4KB.value = address >> 15;
		 std::vector<elem_t> accessedAddresses_4KB = find_elastic_ptw(&elem_4KB, &elasticCuckooHT_4KB);

		 elem_t elem_2MB;
		 elem_2MB.valid = 1;
		 elem_2MB.value = address >> 24;
		 std::vector<elem_t> accessedAddresses_2MB = find_elastic_ptw(&elem_2MB, &elasticCuckooHT_2MB);



		bool found = false;

		for(elem_t elem: accessedAddresses_4KB){
			
			if(elem.valid)
			{
				found = true;
				break;
			}
		}

		for(elem_t elem: accessedAddresses_2MB){
			
			if(elem.valid)
			{
				found = true;
				break;
			}
		}
		return found;
	}



}