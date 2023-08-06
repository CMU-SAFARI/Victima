#ifndef __RANGE_H
#define __RANGE_H

#include "cache_base.h"
#include <vector>
#include "stats.h"




struct Range{

        IntPtr vpn;
        IntPtr bounds;
     /* @kanellok Used for storing the ranges in the range cache */
};

class RLB 
{
        public:
        const ComponentLatency m_latency;
        const ComponentLatency m_miss_latency;
        const UInt64 m_num_sets;
        static const UInt32 SIM_PAGE_SHIFT = 12; // 4KB
        static const IntPtr SIM_PAGE_SIZE = (1L << SIM_PAGE_SHIFT);

        String repl_policy;
        Core* m_core;
        std::vector<Range> m_ranges;
        UInt64 m_accesses;
        UInt64 m_misses;
        UInt64 m_hits;
        int mru;

        RLB(Core *core, String name, ComponentLatency latency, ComponentLatency miss_latency, UInt64 entries, String repl_policy);
        ~RLB();
        void replace_entry (IntPtr vpn);
        bool exists_in_range_table(IntPtr vpn);
        bool access(Core::mem_op_t mem_op_type, IntPtr vpn, bool count); 

};






#endif 
