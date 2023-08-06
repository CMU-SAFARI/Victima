#ifndef PWC_H
#define PWC_H

#include "fixed_types.h"
#include "cache.h"
#include <util.h>
#include <unordered_map>
#include "lock.h"
#include <vector>

namespace ParametricDramDirectoryMSI
{
  class PWC
  {
  private:
    static  UInt32 SIM_PAGE_SHIFT; // 4KB
    static  IntPtr SIM_PAGE_SIZE;
    static  IntPtr SIM_PAGE_MASK;

    Cache m_L4_cache; // @kanellok Cache PMLE4
    Cache m_L3_cache; // Cache PDPT
    Cache m_L2_cache; // Cache PD

    UInt64 m_l4_access, m_l4_miss;
    UInt64 m_l3_access, m_l3_miss;
    UInt64 m_l2_access, m_l2_miss;
    




    core_id_t m_core_id;
    bool perfect;

  public:

    enum where_t
      {
        HIT = 0,
        MISS
      };

    ComponentLatency access_latency;
    ComponentLatency miss_latency;

    PWC(String name, String cfgname, core_id_t core_id, UInt32 L1_num_entries, UInt32 L1_associativity, UInt32 L2_num_entries, UInt32 L2_associativity, UInt32 L3_num_entries, UInt32 L3_associativity, ComponentLatency access_latency, ComponentLatency miss_latency, bool perfect);
    PWC::where_t lookup(IntPtr address, SubsecondTime now, bool allocate_on_miss , int level , bool count);
    void allocate(IntPtr address, SubsecondTime now, Cache *pwc_cache);
    static const UInt64 HASH_PRIME = 124183;
 
  };
}

#endif // TLB_H
