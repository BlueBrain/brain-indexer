#!/bin/bash
# A script that runs all examples in one go
# Please launch in an allocation, e.g. `salloc -Aproj16 -n 6 .ci/_run_examples.sh`

set -euxo pipefail
cd ${SI_DIR:-"."}
pwd

rm -rf usecase1
rm -rf circuit2k
rm -rf tmp-*
rm -rf example_segment_index
rm -rf multi_index_2k

python3 examples/segment_index_sonata.py
python3 examples/segment_index.py
python3 examples/synapses_index.py
srun -n5 python3 examples/segment_multi_index_sonata.py
srun -n3 python3 examples/synapse_multi_index_sonata.py
bash examples/run_ipynb.sh examples/basic_tutorial.ipynb
bash examples/run_ipynb.sh examples/advanced_tutorial.ipynb 3

set +x
echo "[`date`] Example Tests Finished"
