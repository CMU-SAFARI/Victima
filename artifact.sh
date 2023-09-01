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
###
if [ -z "$1" ]; then

  #find out if an environment variable is set
  
  execution_mode_arg="--native"
  # Check if the environment variable is set
  if [ -z "${EXEC_MODE_461}" ]; then
      echo "Environment variable is not set."
  else
      echo "Environment variable is set to: ${EXEC_MODE_461}"
  fi

  if([ "$EXEC_MODE_461" = "--slurm" ]); then
      execution_mode_arg="--slurm"
      echo "Running in job-based mode";
  else
      execution_mode_arg="--native"
      echo "Running in native mode";
  fi

else
  execution_mode_arg=$1
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





