# CM script to run and reproduce experiments

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
