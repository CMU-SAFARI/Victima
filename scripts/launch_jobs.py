
import os


trace_path = "/app/traces/"

traces = [("bc", "bc.sift"),
            ("bfs", "bfs.sift"),
            ("cc", "cc.sift"),
            ("tc", "tc.sift"),
            ("gc", "gc.sift"),
            ("pr", "pr.sift"),
            ("sssp", "sssp.sift"),
            ("rnd", "rnd.sift"),
            ("xs", "xs.sift"),
            ("dlrm", "dlrm.sift"),
            ("gen", "gen.sift")
            ]


# Docker command to run the binary inside the container
docker_command = "docker run --rm -v /mnt/panzer/kanellok/victima_ae:/app/ kanell21/artifact_evaluation:victima"

baseline = " -c /app/sniper/config/virtual_memory_configs/radix.cfg "
victima = " -c /app/sniper/config/virtual_memory_configs/victima.cfg "
potm = " -c /app/sniper/config/virtual_memory_configs/potm.cfg "
virtu = " -c /app/sniper/config/virtual_memory_configs/virtualized.cfg "

configs = [

    ("tlb_base_ideal", baseline+" -g --perf_model/stlb/size=1536 -g --perf_model/stlb/associativity=12 -g --perf_model/tlb/l2_access_penalty=12"),
    ("tlb_2x_ideal", baseline+" -g --perf_model/stlb/size=2048 -g --perf_model/stlb/associativity=16 -g --perf_model/tlb/l2_access_penalty=12"),
    ("tlb_4x_ideal", baseline+" -g --perf_model/stlb/size=4096 -g --perf_model/stlb/associativity=16 -g --perf_model/tlb/l2_access_penalty=12"),
    ("tlb_8x_ideal", baseline+" -g --perf_model/stlb/size=8192 -g --perf_model/stlb/associativity=16 -g --perf_model/tlb/l2_access_penalty=12"),
    ("tlb_16x_ideal", baseline+" -g --perf_model/stlb/size=16384 -g --perf_model/stlb/associativity=16 -g --perf_model/tlb/l2_access_penalty=12"),
    ("tlb_32x_ideal", baseline+" -g --perf_model/stlb/size=32768 -g --perf_model/stlb/associativity=16 -g --perf_model/tlb/l2_access_penalty=12"),
    ("tlb_64x_ideal", baseline+" -g --perf_model/stlb/size=65536 -g --perf_model/stlb/associativity=16 -g --perf_model/tlb/l2_access_penalty=12"),
    ("tlb_128x_ideal", baseline+" -g --perf_model/stlb/size=131072 -g --perf_model/stlb/associativity=16 -g --perf_model/tlb/l2_access_penalty=12"),

    ("L3TLB_ideal_15", potm+ " -g --perf_model/potm_tlb/size=65536  -g --perf_model/potm_tlb/latency=15"),
    ("L3TLB_ideal_20", potm+ " -g --perf_model/potm_tlb/size=65536  -g --perf_model/potm_tlb/latency=20"),
    ("L3TLB_ideal_25", potm+ " -g --perf_model/potm_tlb/size=65536  -g --perf_model/potm_tlb/latency=25"),
    ("L3TLB_ideal_30", potm+ " -g --perf_model/potm_tlb/size=65536  -g --perf_model/potm_tlb/latency=30"),
    ("L3TLB_ideal_35", potm+ " -g --perf_model/potm_tlb/size=65536  -g --perf_model/potm_tlb/latency=35"),
    ("L3TLB_ideal_40", potm+ " -g --perf_model/potm_tlb/size=65536  -g --perf_model/potm_tlb/latency=40"),

    ("pomtlb_64K", potm+" -g --perf_model/potm_tlb/size=65536" ),

    ("victima_ptw_1MBL2", victima+" -g --perf_model/l2_cache/cache_size=1024 -g --perf_model/l2_cache/data_access_time=12 -g --perf_model/victima/victimize_on_ptw=true -g --perf_model/l2_cache/srrip/tlb_enabled=true"),
    ("victima_ptw_2MBL2", victima+" -g --perf_model/l2_cache/cache_size=2048 -g --perf_model/l2_cache/data_access_time=16 -g --perf_model/victima/victimize_on_ptw=true -g --perf_model/l2_cache/srrip/tlb_enabled=true"),
    ("victima_ptw_4MBL2", victima+" -g --perf_model/l2_cache/cache_size=4096 -g --perf_model/l2_cache/data_access_time=22 -g --perf_model/victima/victimize_on_ptw=true -g --perf_model/l2_cache/srrip/tlb_enabled=true"),
    ("victima_ptw_8MBL2", victima+" -g --perf_model/l2_cache/cache_size=8192 -g --perf_model/l2_cache/data_access_time=30 -g --perf_model/victima/victimize_on_ptw=true -g --perf_model/l2_cache/srrip/tlb_enabled=true"),

    ("baseline_radix_virtualized", virtu),
    ("victima_virtualized" , victima + " -g --perf_model/ptw/enabled=false  -g --perf_model/ptw/virtualized=true  -g --perf_model/victima/victimize_on_ptw=true"),
    ("potm_virtualized", potm+" -g --perf_model/potm_tlb/size=65536 -g --perf_model/ptw/enabled=false  -g --perf_model/ptw/virtualized=true")
]
     

sniper_parameters = "/app/sniper/run-sniper -s stop-by-icount:500000000 --genstats --power"

# # # Create the jobfile: a bash script that runs all the binaries with all the configurations
with open("/app/jobfile", "w") as jobfile:
    jobfile.write("#!/bin/bash\n")

    for (trace_name,trace) in traces:

        for (config_name, configuration_string) in configs:

        
            trace_command = "--traces={}".format(trace_path+trace)

            output_command  = "-d /app/results/{}_{}".format(config_name, trace_name)
            
            slurm_command = "sbatch --exclude kratos0,kratos3,kratos8,kratos6,kratos5 -J {}_{} --output=./results/{}_{}.out --error=./results/{}_{}.err docker_wrapper.sh ".format(config_name, trace_name,config_name, trace_name,config_name, trace_name)


            command = slurm_command + "\""+ docker_command + " " + sniper_parameters + " " + output_command+" "+configuration_string+" "+trace_command+"\""
            #command = docker_command + " " + sniper_parameters + " " + output_command+" "+configuration_string+" "+trace_command

            jobfile.write(command)
            jobfile.write("\n")



