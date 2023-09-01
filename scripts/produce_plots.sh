#!/bin/bash
print_colorful_text() {
  local text="$1"
  local color_code="$2"
  echo "\e[${color_code}m${text}\e[0m"
}


if [ -z "$1" ];  then
  echo "Provide container: docker or podman"
  exit
elif [ "$1" = "docker" ]; then
  container="docker"
  echo "Using docker"
elif [ "$1" = "podman" ]; then
  container="podman"
  echo "Using podman"
else 
  echo "Wrong container: provide docker or podman"
fi 

mkdir -p ./plots


${container} pull docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1


${container} run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/scripts/parse_nn_results.py ./results/nn_results

${container} run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/scripts/create_csv.py

exit_code=$?
if [ $exit_code -ne 0 ]; then
    print_colorful_text "Create_csv.py failed because experiments are still running." "33;1"
    exit 
fi

${container} run --rm -v $PWD:/app/ docker.io/kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/scripts/create_plots.py  > plots_in_tabular.txt


print_colorful_text " Check plots_in_tabular.txt for the plots in tabular format (summer art is waiting for you) " "33;1"
print_colorful_text " Check ./plots for the actual plots " "33;1"

echo "====================================================================================="