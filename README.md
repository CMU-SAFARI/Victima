
![Alt Text](images/victima.png)

 
## Hardware requirements

- We will be using docker images to execute the experiments. All docker
  images have been created for X86-64 architectures so you need an
  X86-64 system.
- The experiments have been executed using a slurm based infrastructure.
  We strongly suggest executing the experiments using such an
  infrastructure.

``` cpp
Hardware infrastructure used: 
1) Nodes used:  Intel(R) Xeon(R) Gold 5118 CPU @ 2.30GHz 

2) Slurm version: slurm-wlm 21.08.5
```

## Software requirements


We have prepared docker images which are uploaded publicly in Docker hub
under the tags:  

``` cpp
#Contains all the simulator dependencies
1. kanell21/artifact_evaluation:victima 

#Contains all python dependencies to reproduce the results of Table 2 and create all the plots
2. kanell21/artifact_evaluation:victima_ptwcp_v1.1 
                    
```

These images contain all the necessary software to run the experiments
such as:  

- Sniper simulator dependencies
- Python dependencies for Pytorch, Matplotlib, Seaborn etc.

## Software Requirements to execute Docker

``` cpp
We need: 
- Docker
- curl
- tar
- Debian-based Linux distribution

We used the following versions/distributions in our experiments:
- Docker version 20.10.23, build 7155243
- curl 7.81.0   
- tar (GNU tar) 1.34
- Kernel: 5.15.0-56-generic 
- Dist: Ubuntu SMP 22.04.1 LTS (Jammy Jellyfish)
                    
```

## Launch experiments to reproduce Figures 2, 3, 4, 6, 15, 16, 18, 19, 20, 21, 23

### Execute the following command to achieve the following:  

1\. Execute all the experiments of Victima to reproduce the figures of
the paper  
2. Reproduce Table 2 which requires Neural Network inference  

``` cpp
kanellok@safari:~/victima_artifact$ sh artifact.sh
```

### What the script does:

1\. Installs dependencies and Docker  

``` cpp>
sudo apt-get update
sudo apt-get install \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg \sc
    lsb-release \
    tar 

# Add Docker’s official GPG key
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg

echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io
             
```

2\. Downloads the docker image to run the experiments  

``` cpp
docker pull kanell21/artifact_evaluation:victima
```

3\. Compiles the simulator  

``` cpp
docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima /bin/bash -c "cd /app/sniper && make"
```

4\. Creates a ./jobfile with all the slurm commands and decompresses the
traces  

``` cpp
docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima python /app/launch_jobs.py
tar -xzf traces.tar.gz
```

### The jobfile should look like:

``` cpp
#!/bin/bash
sbatch  -J tlb_base_ideal_bc --output=./results/tlb_base_ideal_bc.out --error=./results/tlb_base_ideal_bc.err docker_wrapper.sh "docker run --rm -v /mnt/panzer/kanellok/victima_ae:/app/ kanell21/artifact_evaluation:victima /app/sniper/run-sniper -s stop-by-icount:500000000 --genstats --power -d /app/results/tlb_base_ideal_bc  -c /app/sniper/config/virtual_memory_configs/radix.cfg  -g --perf_model/stlb/size=1536 -g --perf_model/stlb/associativity=12 -g --perf_model/tlb/l2_access_penalty=12 --traces=/app/traces/bc.sift"
sbatch -J tlb_base_ideal_bc --output=./results/tlb_base_ideal_bc.out --error=./results/tlb_base_ideal_bc.err docker_wrapper.sh "docker run --rm -v /mnt/panzer/kanellok/victima_ae:/app/ kanell21/artifact_evaluation:victima /app/sniper/run-sniper -s stop-by-icount:500000000 --genstats --power -d /app/results/tlb_base_ideal_bc  -c /app/sniper/config/virtual_memory_configs/radix.cfg  -g --perf_model/stlb/size=1536 -g --perf_model/stlb/associativity=12 -g --perf_model/tlb/l2_access_penalty=12 --traces=/app/traces/bc.sift"
.. 
```

5\. Submits the experiments to slurm  

``` cpp
source jobfile
```

6\. Runs the neural network inference experiments and outputs Table 2 in
the standard output and ./nn_replica/data/results.csv  

``` cpp
docker pull kanell21/artifact_evaluation:victima_ptwcp_v1.1

docker run kanell21/artifact_evaluation:victima_ptwcp_v1.1
cat ./nn_replica/data/results.csv
```

## Parse results and create all the plots

All the results of the experiments are stored under ./results.  
Execute the following command to:  

1\. Parse the results of the experiments. All the results in tabular
format can be found under:
/path/to/victima_artifact/plots_in_tabular.txt  
2. Create all the plots of the paper. All the plots can be found under:
/path/to/victima_artifact/plots/  

``` cpp
kanellok@safari:~/victima_artifact$ sh produce_plots.sh
```

### What the script does:

1\. Creates a CSV file which contains all the raw results  

``` cpp>
docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/create_csv.py
```

2\. Creates all the plots under ./plots and outputs all plots in tabular
format in ./plots_in_tabular.txt  

``` cpp
docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/create_plots.py > plots_in_tabular.txt
```

