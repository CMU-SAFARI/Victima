
#include "cache_base.h"
#include <vector>
#include "stats.h"
#include "rangelb.h"
#include "allocation_manager.h"
#include "simulator.h"
#include "config.hpp"
#include <cmath>
#include <iostream>
#include <utility>
#include "core_manager.h"
#include "cache_set.h"



RLB::RLB(Core *core, String name, ComponentLatency latency, ComponentLatency miss_latency, UInt64 entries, String _repl_policy)
            :   m_latency( latency)
              , m_miss_latency(miss_latency)
              , m_num_sets(entries)
              , m_core(core)
              , repl_policy(_repl_policy)
        {
            
            registerStatsMetric(name, core->getId(), "accesses", &m_accesses);
            registerStatsMetric(name, core->getId(), "misses", &m_misses);
            registerStatsMetric(name, core->getId(), "hits", &m_hits);

        }

void RLB::replace_entry(IntPtr vpn){
            

    if(repl_policy == "random-mru"){
            if(Sim()->getAllocationManager()->exists_in_range_table(vpn,m_core->getId()))
            {

                int candidate;

                candidate = rand() % m_num_sets;

                while (candidate == mru ) candidate = rand() % m_num_sets;

                if(m_ranges.size() == m_num_sets )
                  m_ranges.erase (m_ranges.begin()+candidate-1);

                Range rng;
                rng = Sim()->getAllocationManager()->access_range_table(vpn,m_core->getId());
                m_ranges.push_back(rng);  
            
            }

    }
    
}

bool RLB::access(Core::mem_op_t mem_op_type, IntPtr address, bool count){

            IntPtr vpn = (address >> SIM_PAGE_SHIFT);

            int index = 0 ;
            if(count) m_accesses++;
            for(std::vector<Range>::iterator it = m_ranges.begin(); it != m_ranges.end(); ++it) {    

                if(vpn >= (*it).vpn && vpn <= (*it).bounds){
                    if(count) m_hits++;
                    mru = index;
                    return true;
                }

                index++;
            }

            replace_entry(vpn);

            if(count) m_misses++;
            return false;

}









