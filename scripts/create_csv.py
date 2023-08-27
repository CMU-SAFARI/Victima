
import os
import csv

stats = {
    "Trace": None,
    "Exp": None,
    "performance_model.cycle_count": None,
    "performance_model.instruction_count": None,
    "dtlb.miss": None,
    "dtlb.access": None,
    "stlb.miss": None,
    "stlb.access": None,
    "PTW.page_walks": None,
    "PTW.total-PD-latency": None,
    "PTW.total-PDPE-latency": None,
    "PTW.total-PMLE4-latency": None,
    "PTW.total-PT-latency": None,
    "PTW_0.page_walks": None,
    "PTW_0.total-PD-latency": None,
    "PTW_0.total-PDPE-latency": None,
    "PTW_0.total-PMLE4-latency": None,
    "PTW_0.total-PT-latency": None,
    "PTW_1.page_walks": None,
    "PTW_1.total-PD-latency": None,
    "PTW_1.total-PDPE-latency": None,
    "PTW_1.total-PMLE4-latency": None,
    "PTW_1.total-PT-latency": None,
    "PTW_2.page_walks": None,
    "PTW_2.total-PD-latency": None,
    "PTW_2.total-PDPE-latency": None,
    "PTW_2.total-PMLE4-latency": None,
    "PTW_2.total-PT-latency": None,
    "PTW_cuckoo.latency": None,
    "pwc_L2.access": None,
    "pwc_L2.miss": None,
    "pwc_L3.access": None,
    "pwc_L3.miss": None,
    "pwc_L4.access": None,
    "pwc_L4.miss": None,
    "ptw_radix_0.page_level_latency_0": None,
    "ptw_radix_0.page_level_latency_1": None,
    "ptw_radix_0.page_level_latency_2": None,
    "ptw_radix_0.page_level_latency_3": None,
    "ptw_radix_0.page_level_latency_4": None,
    "ptw_radix_0.page_level_latency_0": None,
    "ptw_radix_1.page_level_latency_0": None,
    "ptw_radix_1.page_level_latency_1": None,
    "ptw_radix_1.page_level_latency_2": None,
    "ptw_radix_1.page_level_latency_3": None,
    "ptw_radix_1.page_level_latency_4": None,
    "ptw_radix_2.page_level_latency_0": None,
    "ptw_radix_2.page_level_latency_1": None,
    "ptw_radix_2.page_level_latency_2": None,
    "ptw_radix_2.page_level_latency_3": None,
    "ptw_radix_2.page_level_latency_4": None,
    "mmu.tlb_and_utr_miss": None,
    "mmu.tlb_and_utr_miss_latency": None,
    "mmu.tlb_hit": None,
    "mmu.tlb_hit_latency": None,
    "mmu.utr_hit": None,
    "mmu.utr_hit_latency": None,
    "mmu.total_latency": None,
    "ulb.access": None,
    "ulb.allocations": None,
    "ulb.miss": None,
    "ulb.miss_inUTR": None,
    "utr_1.accesses": None,
    "utr_1.allocation_conflicts": None,
    "utr_1.allocations": None,
    "utr_1.hits": None,
    "utr_2.accesses": None,
    "utr_2.allocation_conflicts": None,
    "utr_2.allocations": None,
    "utr_2.hits": None,
    "uperm.access": None,
    "uperm.miss": None,
    "utag.access": None,
    "utag.miss": None,
    "nuca-cache.read-misses": None,
    "nuca-cache.reads": None,
    "nuca-cache.write-misses": None,
    "nuca-cache.writes": None,
    "dram.reads": None,
    "dram.writes": None,
    "barrier.global_time": None,
    "dram-queue.total-time-used": None,
    "ddr.page-closing": None,
    "ddr.page-conflict-data-to-data": None,
    "ddr.page-conflict-data-to-metadata": None,
    "ddr.page-conflict-metadata-to-data": None,
    "ddr.page-conflict-metadata-to-metadata": None,
    "ddr.page-closing": None,
    "ddr.page-empty": None,
    "ddr.page-hits": None,
    "ddr.page-miss": None,
    "L1-D.metadata-loads": None,
    "L1-D.metadata-load-misses": None,
    "L1-D.metadata-mshr-latency": None,
    "L1-D.mshr-latency": None,
    "L2.metadata-loads": None,
    "L2.metadata-load-misses": None,
    "nuca-cache.read-misses-metadata": None,
    "nuca-cache.reads-metadata": None,
    "mmu.llc_hit_and_l1tlb_hit": None,
    "mmu.llc_hit_and_l2tlb_hit": None,
    "mmu.llc_hit_and_l2tlb_miss": None,
    "mmu.llc_miss_and_l1tlb_hit": None,
    "mmu.llc_miss_and_l2tlb_hit": None,
    "mmu.llc_miss_and_l2tlb_miss": None,
    "L1-D.data-reuse-0": None,
    "L1-D.data-reuse-1": None,
    "L1-D.data-reuse-2": None,
    "L1-D.data-reuse-3": None,
    "L1-D.data-reuse-4": None,
    "L2.data-reuse-0": None,
    "L2.data-reuse-1": None,
    "L2.data-reuse-2": None,
    "L2.data-reuse-3": None,
    "L2.data-reuse-4": None,
    "nuca-cache.data-reuse-0": None,
    "nuca-cache.data-reuse-1": None,
    "nuca-cache.data-reuse-2": None,
    "nuca-cache.data-reuse-3": None,
    "nuca-cache.data-reuse-4": None,
    "L1-D.metadata-reuse-0": None,
    "L1-D.metadata-reuse-1": None,
    "L1-D.metadata-reuse-2": None,
    "L1-D.metadata-reuse-3": None,
    "L1-D.metadata-reuse-4": None,
    "L1-D.loads-page_table": None,
    "L1-D.load-misses-page_table": None,
    "L2.loads-page_table": None,
    "L2.load-misses-page_table": None,
    "nuca-cache.reads-metadata": None,
    "nuca-cache.read-misses-metadata": None,
    "L1-D.tload-misses": None,
    "L1-D.tloads": None,
    "L1-D.tstore-misses": None,
    "L1-D.tstores": None,
    "L2.loads-non_page_table": None,
    "L2.load-misses-non_page_table": None,
    "L1-D.load-misses-non_page_table": None,
    "L1-D.loads-non_page_table": None,
    "L2.loads-tlb_entry": None,
    "L2.loads-misses-tlb_entry": None,
    "L1-D.loads-tlb_entry": None,
    "L1-D.loads-misses-tlb_entry": None,
    "stlb.l1_tlb_cache_hit": None,
    "stlb.l2_tlb_cache_hit": None,
    "stlb.nuca_tlb_cache_hit": None,
    "stlb.potm_latency": None,
    "potm_tlb.access": None,
    "potm_tlb.miss": None,
    "nested_tlb.access": None,
    "nested_tlb.miss": None,
    "L2.tlb-reuse-0": None,
    "L2.tlb-reuse-1": None,
    "L2.tlb-reuse-2": None,
    "L2.tlb-reuse-3": None,
    "L2.tlb-reuse-4": None,
    "L2.tlb-util-0": None,
    "L2.tlb-util-1": None,
    "L2.tlb-util-2": None,
    "L2.tlb-util-3": None,
    "L2.tlb-util-4": None,
    "L2.tlb-util-5": None,
    "L2.tlb-util-7": None,
    "mmu.l1_tlb_hit": None,
    "mmu.l1c_hit_tlb": None,
    "mmu.l1c_hit_tlb_latency": None,
    "mmu.l2_tlb_hit": None,
    "mmu.l2c_hit_tlb": None,
    "mmu.l2c_hit_tlb_latency": None,
    "utr_cache_2.data-reuse-0": None,
    "utr_cache_2.data-reuse-1": None,
    "utr_cache_2.data-reuse-2": None,
    "utr_cache_2.data-reuse-3": None,
    "utr_cache_2.data-reuse-4": None,
    "utr_cache_1.data-reuse-0": None,
    "utr_cache_1.data-reuse-1": None,
    "utr_cache_1.data-reuse-2": None,
    "utr_cache_1.data-reuse-3": None,
    "utr_cache_1.data-reuse-4": None,
    "L1-D.loads-where-page-table-cache-remote": None,
    "L1-D.loads-where-page-table-dram": None,
    "L1-D.loads-where-page-table-dram-cache": None,
    "L1-D.loads-where-page-table-dram-local": None,
    "L1-D.loads-where-page-table-dram-remote": None,
    "L1-D.loads-where-page-table-L1": None,
    "L1-D.loads-where-page-table-L1_S": None,
    "L1-D.loads-where-page-table-L1I": None,
    "L1-D.loads-where-page-table-L2": None,
    "L1-D.loads-where-page-table-L2_S": None,
    "L1-D.loads-where-page-table-L3": None,
    "L1-D.loads-where-page-table-L3_S": None,
    "L1-D.loads-where-page-table-L4": None,
    "L1-D.loads-where-page-table-L4_S": None,
    "L1-D.loads-where-page-table-miss": None,
    "L1-D.loads-where-page-table-nuca-cache": None,
    "L1-D.loads-where-utopiacache-remote": None,
    "L1-D.loads-where-utopiadram": None,
    "L1-D.loads-where-utopiadram-cache": None,
    "L1-D.loads-where-utopiadram-local": None,
    "L1-D.loads-where-utopiadram-remote": None,
    "L1-D.loads-where-utopiaL1": None,
    "L1-D.loads-where-utopiaL1_S": None,
    "L1-D.loads-where-utopiaL1I": None,
    "L1-D.loads-where-utopiaL2": None,
    "L1-D.loads-where-utopiaL2_S": None,
    "L1-D.loads-where-utopiaL3": None,
    "L1-D.loads-where-utopiaL3_S": None,
    "L1-D.loads-where-utopiaL4": None,
    "L1-D.loads-where-utopiaL4_S": None,
    "L1-D.loads-where-utopiamiss": None,
    "mmu.migrations_requests": None,
    "mmu.migrations_delay": None,
}

# Create a CSV with all these headers
with open('results.csv', 'w') as csvfile:

    # Write the headers
    writer = csv.DictWriter(csvfile, fieldnames=stats.keys())
    writer.writeheader()

    path = "./results/"

    for experiment in os.listdir(path):
        if (os.path.isdir(path + experiment) == True):
            row = {}
            config, trace = experiment.rsplit('_', 1)

            row["Trace"] = trace
            row["Exp"] = config

            # Read the stats file
            with open(path + experiment + "/sim.stats") as f:

                lines = f.readlines()
                for line in lines:
                    key = line.split("=")[0]
                    value = line.split("=")[1]
                    key = key.replace(" ", "")

                    if key in stats:
                        row[key] = float(value)

            writer.writerow(row)
