#!/bin/bash




print_colorful_text() {
  local text="$1"
  local color_code="$2"
  echo "\e[${color_code}m${text}\e[0m"
}



if [ "$1" = "--slurm" ]; then
      execution_mode_arg="--slurm"
      echo "Running in job-based mode";
elif ([ "$1" = "--native" ]); then
      execution_mode_arg="--native"
      echo "Running in native mode";
else 
      echo "Provide correct execution mode: --slurm or --native"
      exit
fi

if [ -z "$2" ];  then
  echo "Provide container: docker or podman"
  exit
elif [ "$2" = "docker" ]; then
  container="docker"
  echo "Using docker"
elif [ "$2" = "podman" ]; then
  container="podman"
  echo "Using podman"
else 
  echo "Wrong container: provide docker or podman"
fi 

echo "
╦  ╦┬┌─┐┌┬┐┬┌┬┐┌─┐  ╔═╗┬─┐┌┬┐┬┌─┐┌─┐┌─┐┌┬┐
╚╗╔╝││   │ ││││├─┤  ╠═╣├┬┘ │ │├┤ ├─┤│   │ 
 ╚╝ ┴└─┘ ┴ ┴┴ ┴┴ ┴  ╩ ╩┴└─ ┴ ┴└  ┴ ┴└─┘ ┴ 
 "
 
echo "==================  Run a container test to make sure container works =================="

${container} run docker.io/hello-world

echo "====================================================================================="

echo "==================  Pulling the Docker image to run the experiments =================="

${container} pull docker.io/kanell21/artifact_evaluation:victima

echo "====================================================================================="

echo "==================  Compiling the simulator =================="

${container} run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima /bin/bash -c "cd /app/sniper && make clean && make -j4"

echo "====================================================================================="

echo "==================  Creating the jobfile =================="

echo " Executing python /app/launch_jobs.py in docker container"

${container} run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima python /app/scripts/launch_jobs.py ${execution_mode_arg}  $PWD

echo " Jobfile created - take a look at it to see what experiments will be run"
echo "\n"
echo "====================================================================================="

echo "==================  Decompressing the traces into ./traces =================="

wget https://storage.googleapis.com/traces_virtual_memory/traces_victima
tar -xzf traces_victima

echo "====================================================================================="

echo "================== Launching experiments for Figures 2, 3, 4, 6, 15, 16, 18, 19, 20, 22, 23, 24 =================="

mkdir -p ./results/

sh ./jobfile

echo "====================================================================================="


echo "==================  Reproducing Table 2 =================="

${container} pull docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1
echo "Running docker image kanell21/artifact_evaluation:victima_ptwcp_v1.1"

${container} run docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1 > ./results/nn_results


print_colorful_text " When the experiments finish (all results should be in the results folder) execute the following commands inside the cloned directory: " "33;1" 
print_colorful_text " sh ./scripts/produce_plots.sh " "33;1"





