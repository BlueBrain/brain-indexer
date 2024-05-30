# BrainIndexer Benchmarking

This folder contains scripts to benchmark the performance of BrainIndexer.
The content of this folder is provided as is and it's meant to provide a starting point for benchmarking BrainIndexer. It is not meant to be a complete benchmarking suite and although we will put our best effort in maintaining it, don't expect it to be updated regularly.

## Files included

Each of the bash script included in this folder will run a benchmark for a specific use case. The benchmarks are:
* `SI_seg_benchmark.sh`: benchmark for indexing and querying segments using BrainIndexer.
* `SI_syn_benchmark.sh`: benchmark for indexing and querying synapses using BrainIndexer.

The folder also includes two other script that are used by the benchmarking scripts:
* `create_uniform_index.py`: script to create a uniform index of segments or synapses. Can be used as follows:
  ```
  usage: create_uniform_index.py [-h] [--output OUTPUT] [--n_sections N_SECTIONS] [--segments_per_section SEGMENTS_PER_SECTION]
                                [--boundary BOUNDARY]
                                n_elements

  Create a uniform index with N points

  positional arguments:
    n_elements            Number of elements to generate. Default:1000. If the number of section AND the number of segments per
                          section are provided, the number of elements will be ignored.

  optional arguments:
    -h, --help            show this help message and exit
    --output OUTPUT       Output folder. Default: uniform_index
    --n_sections N_SECTIONS
                          Number of sections to generate. Needs to be provided together with the number of segments per section or
                          will be ignored.
    --segments_per_section SEGMENTS_PER_SECTION
                          Number of segments per section. Needs to be provided together with the number of sections or will be
                          ignored.
    --boundary BOUNDARY   Boundary of the index. The index will be a cube of size 2*boundary. Default: 1000
  ```
  By default the script will save the index in a folder called `uniform_index`. This can be changed with the `--output` option.

* `distribution.ipynb`: a Jupyter notebook with similar functionalities to `create_uniform_index`.
  It doesn't save the resulting index to file but will simply show in a 3D plot the distribution of the generated segments in the index.

## How to run the benchmarks

Run any of the bash script as follows:
```
sbatch name_of_the_script.sh
```
Each script will create two files, one `csv` file containing just the time it took to run the whole script, the indexing and the querying. The other file contains all the remaining output of the script. The name of the resulting files will change depending on the script.
