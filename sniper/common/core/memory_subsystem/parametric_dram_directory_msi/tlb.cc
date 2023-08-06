#include "tlb.h"
#include "stats.h"
#include "config.hpp"
#include <cmath>
#include "cache_cntlr.h"
#include <iostream>
#include <utility>
#include "memory_manager.h"
#include "core_manager.h"
#include "cache_set.h"
#include "cache_base.h"
#include "utils.h"
#include "log.h"
#include "rng.h"
#include "address_home_lookup.h"
#include "fault_injection.h"
#include "memory_manager.h"

//#define DEBUG_TLB
//#define TLB_STATS

namespace ParametricDramDirectoryMSI
{

  UInt32 TLB::SIM_PAGE_SHIFT;
  IntPtr TLB::SIM_PAGE_SIZE;
  IntPtr TLB::SIM_PAGE_MASK;


  ParametricDramDirectoryMSI::MemoryManager *TLB::m_manager = NULL;

  

  TLB::TLB(String name, String cfgname, core_id_t core_id, ShmemPerfModel* _m_shmem_perf_model, UInt32 num_entries,UInt32 pagesize, UInt32 associativity, TLB *next_level, bool _utopia_enabled, bool _track_misses, bool _track_accesses, int* page_size_list, int page_sizes, PageTableWalker*  _ptw)
    : m_size(num_entries)
    , m_core_id(core_id)
    , m_associativity(associativity)
    , m_cache(name + "_cache", 
            cfgname,  
            core_id, num_entries / associativity, 
            associativity, (1L << pagesize), "lru", 
            CacheBase::PR_L1_CACHE, CacheBase::HASH_MASK,
            NULL,
            NULL, true, page_size_list, page_sizes)
    , m_next_level(next_level)
    , m_shmem_perf_model(_m_shmem_perf_model)
    , m_access(0)
    , m_miss(0)
    , l1_tlb_cache_hit(0)
    , l2_tlb_cache_hit(0) 
    , victima_alloc_on_eviction(0)
    , victima_alloc_on_ptw(0)
    , total_potm_latency(SubsecondTime::Zero())
    , nuca_tlb_cache_hit(0)
    , m_tlb_address_saved_cnt(0)
    , utopia_enabled(_utopia_enabled)
    , track_L2TLBmiss(_track_misses)
    , track_Accesses(_track_accesses)
  {
    LOG_ASSERT_ERROR((num_entries / associativity) * associativity == num_entries, "Invalid TLB configuration: num_entries(%d) must be a multiple of the associativity(%d)", num_entries, associativity);

    registerStatsMetric(name, core_id, "access", &m_access);
    registerStatsMetric(name, core_id, "eviction", &m_eviction);
    registerStatsMetric(name, core_id, "miss", &m_miss);

    is_dtlb = (name == "dtlb");
    is_nested = (name == "nested_tlb");
    is_stlb = (name == "stlb");
    is_itlb = (name == "itlb");
    is_potm = (name == "potm_tlb");


    m_num_entries = num_entries;          
    ptw = _ptw;

    //Victima Parameters
    victima_enabled = Sim()->getCfg()->getBool("perf_model/tlb/victima") ;
    victimize_on_ptw = Sim()->getCfg()->getBool("perf_model/victima/victimize_on_ptw");

    //Part of Memory TLB [ISCA 2017] 
    potm_enabled = Sim()->getCfg()->getBool("perf_model/tlb/potm_enabled");


    if(potm_enabled && !(m_next_level)){
      registerStatsMetric(name, core_id, "potm_latency", &total_potm_latency); // @kanellok @tlb_address_access 
    }
    
    /* @kanellok UTOPIA related parameters */
    if(utopia_enabled){
      m_utopia =  Sim()->getUtopia();
      m_utr_4KB = m_utopia->getUtr(0);
      m_utr_2MB = m_utopia->getUtr(1);
    }

    if(is_dtlb && m_next_level){
        
          registerStatsMetric(name, core_id, "L2_TLB_miss_per_page", &tlb_address_access, true); // @kanellok @tlb_address_access 
                                                                                         // does not matter here as we process the metrics at the end
    }

    if(!m_next_level){
          registerStatsMetric(name, core_id, "victima_alloc_on_ptw", &victima_alloc_on_ptw); // @kanellok @tlb_address_access                                                                                   
          registerStatsMetric(name, core_id, "victima_alloc_on_eviction", &victima_alloc_on_eviction); // @kanellok @tlb_address_access                                                                                   
          registerStatsMetric(name, core_id, "l1_tlb_cache_hit", &l1_tlb_cache_hit); // @kanellok @tlb_address_access 
          registerStatsMetric(name, core_id, "l2_tlb_cache_hit", &l2_tlb_cache_hit); // @kanellok @tlb_address_access 
          registerStatsMetric(name, core_id, "nuca_tlb_cache_hit", &nuca_tlb_cache_hit); // @kanellok @tlb_address_access 

    }
    
  }

  TLB::where_t TLB::lookup(IntPtr address, SubsecondTime now, bool allocate_on_miss, int level, bool model_count, Core::lock_signal_t lock_signal)
  {

    // @kanellok UTOPIA related code segment
    if(ptw->isPageFault(address) && utopia_enabled && m_utopia->getHeurPrimary() ==  Utopia::utopia_heuristic::pf ){


      int page_size = ptw->init_walk_functional(address);

      if(page_size == 12)
        m_utr_4KB->allocate(address, now, m_core_id);
      else if(page_size == 21)
        m_utr_2MB->allocate(address, now, m_core_id);

    }
    //end of UTOPIA related code segment

    int page_size = ptw->init_walk_functional(address);
    IntPtr vpn = address >> page_size;


    if(model_count) m_access++;



    #ifdef DEBUG_TLB
      std::cout << " Lookup: " << vpn << "at level: " << level << " Page size: " << page_size <<  std::endl;
    #endif


// @kanellok UTOPIA related code segment
    if(utopia_enabled && is_dtlb)
    {

        bool skip = false;

        if (m_utr_4KB->inUTR(address,model_count,now,m_core_id)) skip = true; //@kanellok check if data is in UTR and just skip the TLB access in such case
        if (m_utr_2MB->inUTR(address,model_count,now,m_core_id)) skip = true; //@kanellok check if data is in UTR and just skip the TLB access in such case

        if(skip) return where_t::UTR_HIT;
        
    }

    bool hit = m_cache.accessSingleLineTLB(address, Cache::LOAD, NULL, 0, now, true);



    #ifdef DEBUG_TLB

      if(hit)   std::cout << " Hit at level: " << level  <<  std::endl;
      if(!hit)      std::cout << " Miss at level: " << level  <<  std::endl;

    #endif

    if (hit){
       if      (is_dtlb || is_nested || is_itlb) return where_t::L1;
       else if (is_stlb) return where_t::L2;
       else if (is_potm) return where_t::POTM;
    }


    if(model_count) m_miss++; // We reach this point if L1 TLB Miss

    TLB::where_t where_next = TLB::MISS;
   
    bool l2tlb_miss = true;
    
    if (m_next_level) // is there a second level TLB?
    {
        
        where_next = m_next_level->lookup(address, now, false , 2 /* no allocation */,model_count, lock_signal); 
        if( where_next != TLB::MISS)
          l2tlb_miss = false;
    }
    else if(victima_enabled){ // We are at L2 TLB 
      
        //L2 TLB Miss - > check the cache hierarchy to see if TLB entry is cached
          UInt32 set; 
          IntPtr tag;

          IntPtr cache_address = address >> (page_size - 3);
          Cache* l1dcache = m_manager->getCache(MemComponent::component_t::L1_DCACHE);
          Cache* l2cache = m_manager->getCache(MemComponent::component_t::L2_CACHE);
          Cache* nuca = m_manager->getNucaCache()->getCache();


          CacheBlockInfo* cb_l1d = l1dcache->peekSingleLine(cache_address);
          CacheBlockInfo* cb_l2 = l2cache->peekSingleLine(cache_address);
          CacheBlockInfo* cb_nuca = nuca->peekSingleLine(cache_address);

          //std::cout << "Searching in L2 Cache with vpn: " << cache_address << " tag: " << tag <<  " in set: " << set << std::endl;

            if(cb_l1d) // Cached in the L1 Data Cache
            {
              if(cb_l1d->getBlockType() == CacheBlockInfo::block_type_t::TLB_ENTRY){
                l1_tlb_cache_hit++;
                return TLB::L1_CACHE;
              }
            }
            else if(cb_l2){ // Cached in the L2 Data Cache
              
              if(cb_l2->getBlockType() == CacheBlockInfo::block_type_t::TLB_ENTRY){
                        // l2cache->updateSetReplacement(cache_address);
                          CacheCntlr* l1dcache = m_manager->getCacheCntlrAt(m_core_id,MemComponent::component_t::L1_DCACHE);
                          SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);                
                          cache_address =(cache_address) & (~((64 - 1))); 
                          CacheBlockInfo::block_type_t block_type = CacheBlockInfo::block_type_t::TLB_ENTRY;
                          //std::cout << "Inserting in the L1 address: " << address << "with cache address " <<  cache_address << " with page_size " << page_size_evicted << std::endl;
                          l1dcache->processMemOpFromCore(
                                          0,
                                          lock_signal,
                                          Core::mem_op_t::READ,
                                          cache_address, 0,
                                          NULL, 64,
                                          true,
                                          true, block_type, SubsecondTime::Zero());

                          getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);
                          m_manager->tagCachesBlockType(cache_address,CacheBlockInfo::block_type_t::TLB_ENTRY);
                          l2_tlb_cache_hit++;
                          return TLB::L2_CACHE;
              }
            }
            else if(cb_nuca){ // Cached in the Nuca Cache
              
              if(cb_nuca->getBlockType() == CacheBlockInfo::block_type_t::TLB_ENTRY){
                          CacheCntlr* l1dcache = m_manager->getCacheCntlrAt(m_core_id,MemComponent::component_t::L1_DCACHE);
                          SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);                
                          cache_address =(cache_address) & (~((64 - 1))); 
                          CacheBlockInfo::block_type_t block_type = CacheBlockInfo::block_type_t::TLB_ENTRY;
                          //std::cout << "Inserting in the L1 address: " << address << "with cache address " <<  cache_address << " with page_size " << page_size_evicted << std::endl;
                          l1dcache->processMemOpFromCore(
                                          0,
                                          lock_signal,
                                          Core::mem_op_t::READ,
                                          cache_address, 0,
                                          NULL, 64,
                                          true,
                                          true, block_type, SubsecondTime::Zero());

                          getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);
                          m_manager->tagCachesBlockType(cache_address,CacheBlockInfo::block_type_t::TLB_ENTRY);          
                          nuca_tlb_cache_hit++;
                return TLB::NUCA_CACHE;
              }
            }
            else if (victimize_on_ptw){ // TLB entry is NOT cached in the cache hierarchy, but we can insert a TLB entry inside the L2 Cache

                          CacheCntlr* l1dcache = m_manager->getCacheCntlrAt(m_core_id,MemComponent::component_t::L1_DCACHE);
                          SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);                
                          cache_address =(cache_address) & (~((64 - 1))); 
                          CacheBlockInfo::block_type_t block_type = CacheBlockInfo::block_type_t::TLB_ENTRY;
                         // std::cout << "Inserting in the L1 address: " << address << "with cache address " <<  cache_address  << std::endl;
                          l1dcache->processMemOpFromCore(
                                          0,
                                          lock_signal,
                                          Core::mem_op_t::READ,
                                          cache_address, 0,
                                          NULL, 64,
                                          true,
                                          true, block_type, SubsecondTime::Zero());

                          getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);
                          m_manager->tagCachesBlockType(cache_address,CacheBlockInfo::block_type_t::TLB_ENTRY);          
                          victima_alloc_on_ptw++;
            }

         
            #ifdef HIERARCHICAL_TAGGING
              IntPtr cache_address = address >> (page_size);
              Cache* l2cache = m_manager->getCache(MemComponent::component_t::L2_CACHE);
              l2cache->splitAddress(cache_address, tag, set);
              tag = (cache_address >> 11) >> (26-18); // Two-level tagging mechanism (First tag is just 4 MSBs)

              std::cout << "Searching in L2 Cache with vpn: " << cache_address << " tag: " << tag <<  " in set: " << set << std::endl;
              //iterate over all ways of set 
              for(int i=0; i < l2cache->getCacheSet(set)->getAssociativity(); i++){

                CacheBlockInfo* cb_l2 = l2cache->peekBlock(set,i);

                if(cb_l2){
                    if(cb_l2->getTag() == tag){

                      for (int j=0; j<cb_l2->getNumSubBlocks(); j++){

                        if(cb_l2->getTagSublockTLB(j) == cache_address){
                          l2_tlb_cache_hit++;
                          return TLB::L2_CACHE;
                        }

                      }
                    }
                }
              }
            #endif

    }
    else if (potm_enabled && !is_nested && level==2) // We have an L2 TLB Miss and POTM ISCA 2017 is enabled
    {
      TLB* potm = m_manager->getPOTM();
      where_t hit;
      hit = potm->lookup(address, now, false , 3, model_count, lock_signal); // @kanellok @tlb_address_access
      
      IntPtr vpn_4kb =(address >> 12);
      IntPtr vpn_2mb =(address >> 21);

      IntPtr tlb_address_4KB = (IntPtr)software_tlb+((vpn_4kb % m_size) * m_associativity)*16;
      IntPtr tlb_address_2MB = (IntPtr)software_tlb+((vpn_2mb % m_size) * m_associativity)*16;

      
      CacheCntlr* l1dcache = m_manager->getCacheCntlrAt(m_core_id,MemComponent::component_t::L1_DCACHE);
      SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);    
      CacheBlockInfo::block_type_t block_type =  CacheBlockInfo::block_type_t::TLB_ENTRY;
            
      IntPtr cache_address_small_page =(tlb_address_4KB) & (~((64 - 1))); 

      //When we search inside the POTM, we bring the software structure inside the cache hierarchy
      l1dcache->processMemOpFromCore(
                        0,
                        lock_signal,
                        Core::mem_op_t::READ,
                        cache_address_small_page, 0,
                        NULL, 64,
                        true,
                        true, block_type, SubsecondTime::Zero());
          
      SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
      SubsecondTime latency_4KB = t_end - t_start;
      getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);

      t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);                
      IntPtr cache_address_large_page =(tlb_address_2MB) & (~((64 - 1))); 
      l1dcache->processMemOpFromCore(
                        0,
                        lock_signal,
                        Core::mem_op_t::READ,
                        cache_address_large_page, 0,
                        NULL, 64,
                        true,
                        true, block_type, SubsecondTime::Zero());
      t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
      SubsecondTime latency_2MB = t_end - t_start;
      getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);

      final_potm_latency = SubsecondTime::Zero();
      
      if(hit != TLB::MISS){

        if (page_size == 12) // 4KB page
        {
          final_potm_latency = latency_4KB;
          total_potm_latency += latency_4KB;
        }
        else if (page_size == 21) // 2MB page
        {
          final_potm_latency = latency_2MB;
          total_potm_latency += latency_2MB;

        }
        return TLB::POTM;
      }
      else{
        final_potm_latency = std::max(latency_4KB,latency_2MB); 
        total_potm_latency += std::max(latency_4KB,latency_2MB);

      }

    }


    bool allocated_in_utopia = false;

    if(l2tlb_miss && utopia_enabled && level == 1){

        if((m_utopia->getHeurSecondary() == Utopia::utopia_heuristic::pte) ){

            if(pte_pressure_cnt.find(vpn) != pte_pressure_cnt.end() ){ // @kanellok Track the pressure at each PTE entry
                    pte_pressure_cnt[vpn] += 1;
            }
            else {
                    pte_pressure_cnt[vpn] = 1;
            }


            if(pte_pressure_cnt[vpn] > m_utopia->getPTEthr()){ //@kanellok if PTE entry is "hot", allocate the page in UTR and dont insert in the TLB
                  
                  allocated_in_utopia = true;
                  pte_pressure_cnt[vpn] = 0 ;

                  if(page_size == 12)
                    m_utr_4KB->allocate(address,now, m_core_id);
                  if(page_size == 21)
                    m_utr_2MB->allocate(address,now, m_core_id);

                  where_next = TLB::L1;
            }
        
        
        }

    }

    if (allocate_on_miss && !allocated_in_utopia ) //@kanellok allocate in L1TLB
    {
        allocate(address, now, level, lock_signal);
    }

    #ifdef TLB_STATS
    
      if(where_next == TLB::MISS && level == 1 && model_count && track_L2TLBmiss){ // Track L2 TLB misses per page

        if(L2TLB_miss_per_page.find(vpn) != L2TLB_miss_per_page.end()) 
            L2TLB_miss_per_page[vpn] += 1;
        else 
            L2TLB_miss_per_page[vpn] = 1; 

      }
    #endif

     return where_next;
  }


  void TLB::allocate(IntPtr address, SubsecondTime now, int level, Core::lock_signal_t lock_signal)
  {

    

    int page_size = ptw->init_walk_functional(address);
    IntPtr vpn = address >> page_size;

    bool eviction = false;
    IntPtr evict_addr;

    CacheBlockInfo evict_block_info;

    IntPtr tag;
    UInt32 set_index;
    //std::cout << "Miss at level" << level << " UTR heuristic: " << m_utr->getHeur() << std::endl;
    

    m_cache.splitAddressTLB(address, tag, set_index, page_size);

    #ifdef DEBUG_TLB
      std::cout << " Allocate " << address << " at level: " << level << " with page_size " << page_size << "and tag " << tag  <<  std::endl;
    #endif

    m_cache.insertSingleLineTLB(address, NULL, &eviction, &evict_addr, &evict_block_info, NULL, now, NULL,CacheBlockInfo::block_type_t::NON_PAGE_TABLE,page_size);
     
    if(eviction){ // Runs only if L1 translation entry is evicted to the L2 TLB
        
        if(utopia_enabled){
          
          if((m_utopia->getHeurSecondary() == Utopia::utopia_heuristic::tlb) && level == 1){
                
                  m_next_level->m_cache.splitAddressTLB(address, tag, set_index, page_size); //@kanellok check pressure at L2 TLB if a an evicted translation from L1 will evict a translation from L2 
                  

                  if(tlb_pressure_cnt.find(set_index) != tlb_pressure_cnt.end() ){ // @kanellok Track the pressure at each TLB set

                      tlb_pressure_cnt[set_index] += 1;

                  }
                  else {

                      tlb_pressure_cnt[set_index] = 1;
                  }

                  if(tlb_pressure_cnt[set_index] > m_utopia->getTLBthr()){

                      tlb_pressure_cnt[set_index] = 0;
                      if(page_size == 12)
                         m_utr_4KB->allocate(address,now, m_core_id);
                      if(page_size == 21)
                         m_utr_2MB->allocate(address,now,m_core_id);

                  }
              
          }
        }
    }

    if(eviction  && !(m_next_level)) // Eviction from L2 TLB or Nested TLB
     {

      int page_size_evicted = evict_block_info.getPageSize();
      IntPtr evict_addr_vpn = evict_addr >> (page_size_evicted-3); // 8 blocks in the cache line (64B) and 3 bits for the offset

      if(victima_enabled) // @kanellok if TLB caching is enabled, insert the evicted translation in the L1/L2 cache
      {

        
          bool victima_miss = false;

          Cache* l1dcache = m_manager->getCache(MemComponent::component_t::L1_DCACHE);
          Cache* l2cache = m_manager->getCache(MemComponent::component_t::L2_CACHE);
          Cache* nuca = m_manager->getNucaCache()->getCache();


          CacheBlockInfo* cb_l1d = l1dcache->peekSingleLine(evict_addr_vpn);
          CacheBlockInfo* cb_l2 = l2cache->peekSingleLine(evict_addr_vpn);
          CacheBlockInfo* cb_nuca = nuca->peekSingleLine(evict_addr_vpn);

          //If TLB entry is not present in the cache hierarchy -> fetch it
          if(!(cb_l1d) && !(cb_l2) && !(cb_nuca)){

            victima_miss=true;
          
          }

          if(victima_miss){
            CacheCntlr* l1dcachecntlr = m_manager->getCacheCntlrAt(m_core_id,MemComponent::component_t::L1_DCACHE);
            SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);                
            IntPtr cache_address =(evict_addr_vpn) & (~((64 - 1))); 
            CacheBlockInfo::block_type_t block_type = CacheBlockInfo::block_type_t::TLB_ENTRY;
            l1dcachecntlr->processMemOpFromCore(
                            0,
                            lock_signal,
                            Core::mem_op_t::READ,
                            cache_address, 0,
                            NULL, 64,
                            true,
                            true, block_type, SubsecondTime::Zero());

            getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);
            m_manager->tagCachesBlockType(cache_address,CacheBlockInfo::block_type_t::TLB_ENTRY);
            victima_alloc_on_eviction++;
          }
      }
    }
    
    #ifdef DEBUG_TLB
      if(eviction)
       std::cout << " Evicted " << evict_addr << " from level: " << level << " with page_size" << page_size  <<  std::endl;
    #endif

      if(eviction) m_eviction++;

      if (eviction && m_next_level){

            #ifdef DEBUG_TLB
              std::cout << "We need to evict from L1 TLB the translation of " << evict_addr << " to L2 TLB" << std::endl;
            #endif
              
              m_next_level->allocate(evict_addr, now, level+1, lock_signal);
      }

      //If POTM is enabled and we have an L2 TLB eviction, allocate the evicted translation in the POTM but dont do this for nested TLB
      if(eviction && !(m_next_level) && potm_enabled && !(is_nested) && (level == 2)){

          int page_size_evicted = evict_block_info.getPageSize();
          IntPtr evict_addr_vpn = evict_addr >> page_size_evicted;

          TLB* potm = m_manager->getPOTM();
          potm->allocate(evict_addr, now, level+1, lock_signal);          

          CacheCntlr* l1dcache = m_manager->getCacheCntlrAt(m_core_id,MemComponent::component_t::L1_DCACHE);
         
          IntPtr tlb_address = (IntPtr)software_tlb+((evict_addr_vpn % m_size) * m_associativity)*16;
          IntPtr cache_address =(tlb_address) & (~((64 - 1))); 

          SubsecondTime t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);                
          CacheBlockInfo::block_type_t block_type =  CacheBlockInfo::block_type_t::TLB_ENTRY;

          //When we allocate inside the POTM, we bring the software structure inside the cache hierarchy
          l1dcache->processMemOpFromCore(
                        0,
                        lock_signal,
                        Core::mem_op_t::READ,
                        cache_address, 0,
                        NULL, 64,
                        true,
                        true, block_type, SubsecondTime::Zero());
        
          getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,t_start);



          m_manager->tagCachesBlockType(tlb_address,CacheBlockInfo::block_type_t::TLB_ENTRY);
      }

    return ;

  }



}

