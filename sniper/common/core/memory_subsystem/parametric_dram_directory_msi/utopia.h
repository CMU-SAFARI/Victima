#ifndef __UTOPIA_H
#define __UTOPIA_H

#include "stats.h"
#include "config.hpp"
#include "simulator.h"
#include <cmath>
#include <iostream>
#include <utility>
#include "cache.h"
#include <vector>
#include "stats.h"
#include "utils.h"
#include "cache_set.h"
#include "hash_map_set.h"


namespace ParametricDramDirectoryMSI
{

        class UTR{

             

                // @kanellok: Add Replacement Policy in a UTR
                private:
                
                        int id;
                        long long int size;
                        int page_size; // UTR can be 4KB, 2MB, 1GB
                        uint assoc;
                        String hash;
                        String repl;
                        int num_sets;
                        int tag_size;
                        UInt64 m_utr_conflicts,m_utr_accesses,m_utr_hits,m_allocations,m_pagefaults;

                        std::unordered_map<IntPtr,bool> vpn_container; // container to check if vpn is inside UTR

                        IntPtr utr_tags_base; // Used to perform mem access
                        IntPtr utr_permissions_base;

                        std::vector<IntPtr> permissions;
                        std::vector<IntPtr> tags;
                        
                        int *utr_allocation_heatmap;
                        Utopia *utopia;

                public:


                std::vector<int> utilization;
                Cache *m_utr_cache;
                Lock *utr_lock; // We need to lock UTR every time we read/modify

                UTR(int id, int size, int page_size, int assoc, String repl, String hash_function, Utopia *utopia);
                ~UTR();

                int getSize(){ return size; }
                std::unordered_map<IntPtr,bool>  getVpnContainer(){return vpn_container;}
                int getPageSize(){return page_size; }
                int getAssoc(){return assoc;}
                std::vector<IntPtr> getPermissionsVector(){return permissions;}
                IntPtr getPermissions(){return utr_permissions_base;}
                IntPtr getTags(){return utr_tags_base;}
                bool inUTR(IntPtr address, bool count,SubsecondTime now, int core_id);
                bool inUTRnostats(IntPtr address, bool count,SubsecondTime now, int core_id);
                void allocate(IntPtr address, SubsecondTime now, int core_id);
                bool permission_filter(IntPtr address, int core_id);
                IntPtr calculate_permission_address(IntPtr address, int core_id);
                IntPtr calculate_tag_address(IntPtr address, int core_id);
                void track_utilization();
        

        };

        class Utopia
        {
                private:
                        std::vector<UTR*> utr_vector;
                        UTR* shadow_utr;
                        
                public: 
                        int utrs;

                enum utopia_heuristic
                {
                        none = 0,
                        tlb,
                        pte,
                        pf
                };
                
                utopia_heuristic heur_type_primary;
                utopia_heuristic heur_type_secondary;

                int pte_eviction_thr;  
                int tlb_eviction_thr;  
                
                bool serial_l2_tlb;
                bool shadow_mode_enabled;
                UInt64 page_faults;

                std::unordered_map<IntPtr, SubsecondTime> *migration_map; // Track migrated pages

                Utopia();

                std::vector<UTR*> getUtrVector(){return utr_vector;}
                UTR* getUtr(int index){return utr_vector[index];}
                UTR* getShadowUTR(){return shadow_utr;}
                std::unordered_map<IntPtr, SubsecondTime>* getMigrationMap(){return migration_map;}

                utopia_heuristic getHeurPrimary(){return heur_type_primary;}
                utopia_heuristic getHeurSecondary(){return heur_type_secondary;}

                void increasePageFaults() { page_faults++; }  

                int getPTEthr(){ return pte_eviction_thr;}
                int getTLBthr(){ return tlb_eviction_thr;}
                bool getShadowMode(){return shadow_mode_enabled;}
                bool getSerialL2TLB(){return serial_l2_tlb;}
                static const UInt64 HASH_PRIME = 124183;

        };

}




#endif 
