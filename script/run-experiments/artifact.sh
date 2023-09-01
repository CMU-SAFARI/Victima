#!/bin/bash




print_colorful_text() {
  local text="$1"
  local color_code="$2"
  echo "\e[${color_code}m${text}\e[0m"
}

###
# Need to be inside the root of Victima repository to mount this directory and pass it to Docker
cm_repo=`cm find repo micro-2023-461`
cm_repo_dir=${cm_repo#*= }
echo "Changing to ${cm_repo_dir}"
cd ${cm_repo_dir}


if [ -z "${CONTAINER_461}" ];  then
  echo "Provide container: docker or podman"
  exit
else if [ "${CONTAINER_461}" = "docker" ]; then
  container="docker"
  echo "Using docker"
else if [ "${CONTAINER_461}" = "podman" ]; then
  container="podman"
  echo "Using podman"
else 
  echo "Wrong container: provide docker or podman"
fi 

if([ "$EXEC_MODE_461" = "--slurm" ]); then
      execution_mode_arg="--slurm"
      echo "Running in job-based mode";
else if ([ "$EXEC_MODE_461" = "--native" ]); then
      execution_mode_arg="--native"
      echo "Running in native mode";
else 
      echo "Provide correct execution mode: --slurm or --native"
      exit
fi


echo "
╦  ╦┬┌─┐┌┬┐┬┌┬┐┌─┐  ╔═╗┬─┐┌┬┐┬┌─┐┌─┐┌─┐┌┬┐
╚╗╔╝││   │ ││││├─┤  ╠═╣├┬┘ │ │├┤ ├─┤│   │ 
 ╚╝ ┴└─┘ ┴ ┴┴ ┴┴ ┴  ╩ ╩┴└─ ┴ ┴└  ┴ ┴└─┘ ┴ 
 "
 
echo "==================  Run a Docker test to make sure Docker works =================="

docker run hello-world

echo "====================================================================================="

echo "==================  Pulling the Docker image to run the experiments =================="

docker pull kanell21/artifact_evaluation:victima

echo "====================================================================================="

echo "==================  Compiling the simulator =================="

docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima /bin/bash -c "cd /app/sniper && make clean && make -j4"

echo "====================================================================================="

echo "==================  Creating the jobfile =================="

echo " Executing python /app/launch_jobs.py in docker container"

docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima python /app/scripts/launch_jobs.py $1 ${execution_mode_arg}

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

docker pull kanell21/artifact_evaluation:victima_ptwcp_v1.1
echo "Running docker image kanell21/artifact_evaluation:victima_ptwcp_v1.1"

docker run kanell21/artifact_evaluation:victima_ptwcp_v1.1 > ./results/nn_results


print_colorful_text " When the experiments finish (all results should be in the results folder) execute the following commands inside the cloned directory: " "33;1" 
print_colorful_text " sh ./scripts/produce_plots.sh " "33;1"





