#include "pwc.h"
#include "stats.h"
#include "config.hpp"
#include <cmath>
#include <iostream>
#include <utility>
#include "core_manager.h"
#include "cache_set.h"

namespace ParametricDramDirectoryMSI
{
  

  UInt32 PWC::SIM_PAGE_SHIFT;
  IntPtr PWC::SIM_PAGE_SIZE;
  IntPtr PWC::SIM_PAGE_MASK;

         
  PWC::PWC(String name, String cfgname, core_id_t core_id, UInt32 L4_associativity, UInt32 L4_num_entries, UInt32 L3_associativity, UInt32 L3_num_entries, UInt32 L2_associativity, UInt32 L2_num_entries, ComponentLatency _access_latency, ComponentLatency _miss_latency, bool _perfect)
    : m_core_id(core_id)
    , access_latency(_access_latency)
    , miss_latency(_miss_latency)
    , m_L4_cache(name + "_L4", cfgname, core_id, L4_num_entries / L4_associativity, L4_associativity, 8, "lru", CacheBase::PR_L1_CACHE) // Assuming 8B granularity
    , m_L3_cache(name + "_L3", cfgname, core_id, L3_num_entries / L3_associativity, L3_associativity, 8, "lru", CacheBase::PR_L1_CACHE)
    , m_L2_cache(name + "_L2", cfgname, core_id, L2_num_entries / L2_associativity, L2_associativity, 8, "lru", CacheBase::PR_L1_CACHE)  
    , perfect(_perfect)
  {

    registerStatsMetric(name+"_L4", core_id, "access", &m_l4_access);
    registerStatsMetric(name+"_L4", core_id, "miss", &m_l4_miss);

    registerStatsMetric(name+"_L3", core_id, "access", &m_l3_access);
    registerStatsMetric(name+"_L3", core_id, "miss", &m_l3_miss);

    registerStatsMetric(name+"_L2", core_id, "access", &m_l2_access);
    registerStatsMetric(name+"_L2", core_id, "miss", &m_l2_miss);

  }

  PWC::where_t PWC::lookup(IntPtr address, SubsecondTime now, bool allocate_on_miss, int level, bool count)
  {
    bool hit;
    if(perfect)
      return PWC::HIT;
    //TODO model bitmap cache access here
    switch(level){

      case 1:{
          hit = m_L4_cache.accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);
          if(count) m_l4_access++;
          if (hit) return PWC::HIT;
          else{
             
              if(count) m_l4_miss++;
              if (allocate_on_miss) allocate(address, now, &m_L4_cache);
              return PWC::MISS;
          }
          break;

      }

      case 2: {
          hit = m_L3_cache.accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);
          if(count) m_l3_access++;
          if (hit) return PWC::HIT;
          else{
             if(count) m_l3_miss++;
             if (allocate_on_miss) allocate(address, now,& m_L3_cache);
            return PWC::MISS;
          }
          break;

      }
      case 3: 
      {
          hit = m_L2_cache.accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);
          if(count) m_l2_access++;
          if (hit) return PWC::HIT;
          else{
                  if(count) m_l2_miss++;
                  if (allocate_on_miss) allocate(address, now, &m_L2_cache);
                  return PWC::MISS;
          }
          break;
      }


    }

  }

  void PWC::allocate(IntPtr address, SubsecondTime now, Cache *pwc_cache)
  {
    bool eviction;
    IntPtr evict_addr;
    CacheBlockInfo evict_block_info;

    IntPtr tag;
    UInt32 set_index;
    pwc_cache->splitAddress(address, tag, set_index);
    pwc_cache->insertSingleLine(address, NULL, &eviction, &evict_addr, &evict_block_info, NULL, now);
  }

}
