#!/bin/bash
print_colorful_text() {
  local text="$1"
  local color_code="$2"
  echo "\e[${color_code}m${text}\e[0m"
}
mkdir ./plots

docker pull kanell21/artifact_evaluation:victima_ptwcp_v1.1

docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/scripts/create_csv.py
docker run --rm -v $PWD:/app/ kanell21/artifact_evaluation:victima_ptwcp_v1.1 python3 /app/scripts/create_plots.py > plots_in_tabular.txt


print_colorful_text " Check plots_in_tabular.txt for the plots in tabular format (summer art is waiting for you) " "33;1"
print_colorful_text " Check ./plots for the actual plots " "33;1"

echo "====================================================================================="