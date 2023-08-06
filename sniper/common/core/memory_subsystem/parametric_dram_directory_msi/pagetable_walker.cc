#include "pagetable_walker.h"
#include <ctime>
#include "config.hpp"
#include "pwc.h"
#include "subsecond_time.h"
#include "nuca_cache.h"
#include "string.h"

namespace ParametricDramDirectoryMSI
{

	std::default_random_engine generator;
	std::uniform_int_distribution<long long int> distribution(1,0xA00000000000000);

	//https://spc.unige.ch/en/teaching/courses/algorithmes-probabilistes/random-numbers-week-1/
	
    int PageTableWalker::counter = 0;

	PageTableWalker::PageTableWalker(int _core_id, int _page_size, ShmemPerfModel* _m_shmem_perf_model, PWC* _pwc, bool _page_walk_cache_enabled)
	{
		 
		CR3 = 0x1000000;
		CR3 = CR3*(core_id+1);
		data_size = 8;		
		page_size = _page_size;
		String name = "PTW_";
		name = name+std::to_string(counter).c_str();

		m_shmem_perf_model = _m_shmem_perf_model;
		page_walk_cache_enabled = _page_walk_cache_enabled;
		core_id = _core_id;
		stats.number_of_2MB_pages=0;
		if( page_walk_cache_enabled){

			pwc = _pwc; // Instantiate page walk caches

		}

		srand(time(0));

		ptw_latency_heatmap = (int*) calloc(5, sizeof(int)); // Used to trigger Per-page PTW stats 

		bzero(&stats, sizeof(stats));
   		
		registerStatsMetric(name, core_id, "page_walks", &stats.page_walks);
		registerStatsMetric(name, core_id, "number-of-2MB-pages", &stats.number_of_2MB_pages);
		registerStatsMetric(name, core_id, "ptw_latency_heatmap", &ptw_latency_heatmap, true);
		counter++;
	}

	SubsecondTime PageTableWalker::init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache,CacheCntlr *_cache, Core::lock_signal_t _lock_signal, Byte* _data_buf, UInt32 _data_length, bool _modeled, bool _count)
	{
		
		// Setup the step walk functions
		stats.page_walks++;
		bzero(&current_lat, sizeof(current_lat));

		cache = _cache;
		modeled = _modeled;
		count = _count;
		data_buf = _data_buf;
		vpn = address >> page_size;
		data_length = _data_length;
		lock_signal = _lock_signal;
		latency = SubsecondTime::Zero();
		
		//printf("Page Walk latency %ld \n",SubsecondTime::divideRounded(current_lat,period));

		return latency;
	}



	void PageTableWalker::track_per_page_ptw_latency(int vpn, SubsecondTime current_lat){
		//std::cout<<"Works!\n";
		SubsecondTime period = SubsecondTime::SEC() / (2.6*1000000000);
		UInt64 cycles = SubsecondTime::divideRounded(current_lat, period);
		if(per_page_ptw_latency.find(vpn) != per_page_ptw_latency.end())
			per_page_ptw_latency[vpn] += cycles ;
		else
			per_page_ptw_latency[vpn] = cycles ;

	}


};
