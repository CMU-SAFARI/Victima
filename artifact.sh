#!/bin/bash

print_colorful_text() {
  local text="$1"
  local color_code="$2"
  echo "\e[${color_code}m${text}\e[0m"
}
echo "
╦  ╦┬┌─┐┌┬┐┬┌┬┐┌─┐  ╔═╗┬─┐┌┬┐┬┌─┐┌─┐┌─┐┌┬┐
╚╗╔╝││   │ ││││├─┤  ╠═╣├┬┘ │ │├┤ ├─┤│   │ 
 ╚╝ ┴└─┘ ┴ ┴┴ ┴┴ ┴  ╩ ╩┴└─ ┴ ┴└  ┴ ┴└─┘ ┴ 
 "
 
echo "==================  Install Docker (you can skip if already installed)=================="

sudo apt-get update
sudo apt-get -y install \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg \
    lsb-release \
    tar 

# Add Docker’s official GPG key
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg

echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt-get update
sudo apt-get -y install docker-ce docker-ce-cli containerd.io

sudo usermod -aG docker $USER

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

docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima python /app/scripts/launch_jobs.py $1 $PWD

echo " Jobfile created - take a look at it to see what experiments will be run"
echo "\n"
echo "====================================================================================="

echo "==================  Decompressing the traces into ./traces =================="

wget https://storage.googleapis.com/traces_virtual_memory/traces_victima
tar -xzf traces_victima

echo "====================================================================================="

echo "==================  Launching experiments for Figures 2, 3, 4, 6, 15, 16, 19, 22, 23, 24 =================="

mkdir -p ./results/

sh ./jobfile

echo "====================================================================================="


echo "==================  Reproducing Table 2 =================="

docker pull kanell21/artifact_evaluation:victima_ptwcp_v1.1
echo "Running docker image kanell21/artifact_evaluation:victima_ptwcp_v1.1"

docker run kanell21/artifact_evaluation:victima_ptwcp_v1.1

cat ./ptw_cp/data/results.csv


print_colorful_text " When the experiments finish (all results should be in the results folder) execute the following commands inside the cloned directory: " "33;1" 
print_colorful_text " sh ./scripts/produce_plots.sh " "33;1"





