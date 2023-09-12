
# Artifact Evaluation for our MICRO 2023 paper called "Victima"
## Details about the paper coming soon.. Stay Tuned!

![Alt Text](images/victima.png)

<p align="center">
    <a href="https://github.com/CMU-SAFARI/Victima/releases">
        <img alt="GitHub release" src="https://img.shields.io/github/release/CMU-SAFARI/Victima">
    </a>
    <a href="https://doi.org/10.5281/zenodo.8220613"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.8220613.svg" alt="DOI"></a>
</p>

**Structure of the repo:**
1. **Hardware Requirements**
   - Container images, infrastructure, and hardware details.
   
2. **Software Requirements**
   - Container images and included software.
   
3. **Software Requirements for Containerized Execution**
   - Software and installation instructions.
   
4. **Launching Experiments**
   - Commands for reproducing figures and tables.
   
5. **Parsing Results and Plot Creation**
   - Location of results and plot creation commands.
   
6. **Reusability using MLCommons**
   - MLCommons interface support and evaluation instructions.
 
## Hardware requirements

- We will be using docker images to execute the experiments. All docker
  images have been created for X86-64 architectures so you need an
  X86-64 system.
- The experiments have been executed using a slurm based infrastructure.
  We strongly suggest executing the experiments using such an
  infrastructure.
- Each experiments takes ~5-10 hours to finish and requires about ~5-13GB of free memory.
- The dataset we used requires ~10GB of storage space. 

``` cpp
Hardware infrastructure used: 
1) Nodes used:  Intel(R) Xeon(R) Gold 5118 CPU @ 2.30GHz 

2) Slurm version: slurm-wlm 21.08.5
```

## Software requirements


We have prepared container images which are uploaded publicly in Docker hub
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


## Software Requirements to execute Docker/Podman

``` cpp
We need: 
- Docker/Podman
- curl
- tar
- Debian-based Linux distribution

We used the following versions/distributions in our experiments:
- Docker version 20.10.23, build 7155243
- Podman 3.4.4
- curl 7.81.0   
- tar (GNU tar) 1.34
- Kernel: 5.15.0-56-generic 
- Dist: Ubuntu SMP 22.04.1 LTS (Jammy Jellyfish)
                    
```

To install docker/podman execute the following script:
``` bash
kanellok@safari:~/Victima$ sh install_container.sh docker

or 

kanellok@safari:~/Victima$ sh install_container.sh podman

```
You need to cd back to the cloned repository since we executed:
``` bash
su - $USER 
```
to refresh the permissions. 
## Launch experiments to reproduce Figures 2, 3, 4, 6, 15, 16, 18, 19, 20, 22, 23, 24 and Table 2

### Execute the following command to achieve the following:  

1\. Execute all the experiments of Victima to reproduce the figures of
the paper  
2. Reproduce Table 2 which requires Neural Network inference  

If your infrastructure supports Slurm:
``` bash
kanellok@safari:~/$ cd Victima

kanellok@safari:~/Victima$ sh artifact.sh --slurm docker #(or podman)

```

If you need to run without a job manager (which we do not recommend)
``` bash
kanellok@safari:~/$ cd Victima
kanellok@safari:~/Victima$ sh artifact.sh --native docker #(or podman)
```

### What the scripts do:

1\. Installs dependencies and Docker/Podman  

``` bash
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

2\. Downloads the container image to run the experiments  

``` bash
docker/podman pull docker.io/kanell21/artifact_evaluation:victima
```

3\. Compiles the simulator  

``` bash
docker/podman run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima /bin/bash -c "cd /app/sniper && make clean && make -j4"
```

4\. Creates a ./jobfile with all the slurm commands and decompresses the
traces  

``` bash
docker/podman run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima python /app/scripts/launch_jobs.py
tar -xzf traces.tar.gz
```

### The jobfile should look like:

``` bash
#!/bin/bash
sbatch  -J tlb_base_ideal_bc --output=./results/tlb_base_ideal_bc.out --error=./results/tlb_base_ideal_bc.err docker_wrapper.sh "docker run --rm -v /mnt/panzer/kanellok/victima_ae:/app/ kanell21/artifact_evaluation:victima /app/sniper/run-sniper -s stop-by-icount:500000000 --genstats --power -d /app/results/tlb_base_ideal_bc  -c /app/sniper/config/virtual_memory_configs/radix.cfg  -g --perf_model/stlb/size=1536 -g --perf_model/stlb/associativity=12 -g --perf_model/tlb/l2_access_penalty=12 --traces=/app/traces/bc.sift"
sbatch -J tlb_base_ideal_bc --output=./results/tlb_base_ideal_bc.out --error=./results/tlb_base_ideal_bc.err docker_wrapper.sh "docker run --rm -v /mnt/panzer/kanellok/victima_ae:/app/ kanell21/artifact_evaluation:victima /app/sniper/run-sniper -s stop-by-icount:500000000 --genstats --power -d /app/results/tlb_base_ideal_bc  -c /app/sniper/config/virtual_memory_configs/radix.cfg  -g --perf_model/stlb/size=1536 -g --perf_model/stlb/associativity=12 -g --perf_model/tlb/l2_access_penalty=12 --traces=/app/traces/bc.sift"
.. 
```

5\. Submits the experiments to slurm  

``` bash
source jobfile
```

6\. Runs the neural network inference experiments and outputs Table 2 in
```./results/nn_results.

``` bash
docker pull kanell21/artifact_evaluation:victima_ptwcp_v1.1
docker run kanell21/artifact_evaluation:victima_ptwcp_v1.1
```

## Parse results and create all the plots

All the results of the experiments are stored under ./results.  
Execute the following command to:  

1\. Parse the results of the experiments. All the results in tabular
format can be found under:
```/Victima/plots_in_tabular.txt```  
2. Create all the plots of the paper. All the plots can be found under:
```/Victima/plots/``` 
3. Create csv with Table 2 under:
```/Victima/plots/table2.csv``` 



``` bash
kanellok@safari:~/victima_artifact$ sh ./scripts/produce_plots.sh docker #(or podman)
```

### What the script does:

1\. Creates a CSV file which contains all the raw results  

``` bash
docker/podman run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/scripts/create_csv.py
```

If one of the jobs is still running, the ``` scripts/list_of_experiments.py```  gets invoked to print the status of the jobs ( Experiment name, Status {Running,Completed}, Time ) and informs about how many have been completed and how many are still running. The create_csv.py exits if all the jobs are not completed.

2\. Creates all the plots under ./plots and outputs all plots in tabular
format in ./plots_in_tabular.txt  

``` bash
docker/podman run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/scripts/create_plots.py > plots_in_tabular.txt
```

### Reusability using MLCommons

We added support to evaluate Victima using the [MLCommons CM automation language](https://github.com/mlcommons/ck).

Make sure you have install CM. Follow this [guide](https://github.com/mlcommons/ck/blob/master/docs/installation.md) to install it.

Next install reusable MLCommons automations: 

```bash
cm pull repo mlcommons@ck
```

Pull this repository via CM:
```bash
cm pull repo CMU-SAFARI@Victima
```

#### Run Victima via CM interface

The core CM script for Victima will be available under ```/CM/repos/CMU-SAFARI@Victima/script/reproduce-micro-2023-paper-victima```

It is described by `_cm.yaml` and several native scripts.

Perform the following steps to evaluate Victima with MLCommons CM automation language:

1) This command will install system dependencies for Docker and require sudo (skip it if you have Docker installed):
```bash
cmr "reproduce micro 2023 victima _install_deps"
```

2) This command will prepare and run all experiments via Docker:

```bash
cmr "reproduce micro 2023 victima _run" 
```

You can specify --job_manager and --container if needed:
```bash
cmr "reproduce micro 2023 victima _run" --job_manager=native|slurm --contianer=docker|podman
```

3) In case of successful execution of a previous command, this command will generate plots to help you validate results from the article:

```bash
cmr "reproduce micro 2023 victima _plot"
```



#### Alternative way to run Victima via CM sub-scripts



The CM scripts for Victima will be available under ```/CM/repos/CMU-SAFARI@Victima/script/```

```
script
|_install_dep  |_produce-plots  |_run-experiments
```

Perform the following steps to evaluate Victima with MLCommons:


1) This command will install system dependencies for Docker/Podman and require sudo (skip it if you have Docker/Podman installed):
```bash
cm run script micro-2023-461:install_dep --env.CONTAINER_461="docker" #(or "podman")
```

2) This command will prepare and run all experiments via the container:

```bash
#For slurm-based execution:

cm run script micro-2023-461:run-experiments --env.EXEC_MODE_461="--slurm" --env.CONTAINER_461="docker" #(or "podman")

#For native execution:

cm run script micro-2023-461:run-experiments --env.EXEC_MODE_461="--native" --env.CONTAINER_461="docker" #(or "podman")


```

3) In case of successful execution of the previous command, this command will generate the the plots of the paper:

```bash
cm run script micro-2023-461:produce-plots --env.CONTAINER_461="docker" #(or "podman")
```

