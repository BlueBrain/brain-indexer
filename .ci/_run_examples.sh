#!/bin/bash
# A script that runs all examples in one go
# Please launch in an allocation, e.g. `salloc -Aproj16 -n 6 .ci/_run_examples.sh`

set -euxo pipefail
cd ${SI_DIR:-"."}
pwd

rm -r usecase1 || true
rm -r circuit2k || true
rm -rf tmp-* || true
rm -r example_segment_index || true

python examples/segment_index_sonata.py
python examples/segment_index.py
python examples/synapses_index.py
srun -n5 python examples/segment_multi_index_sonata.py
srun -n3 python examples/synapse_multi_index_sonata.py
# This should be done somewhere more appropriate, but here it goes:
srun -n5 python tests/test_validation_FLAT.py --run-multi-index
bash .ci/test_circuit_config-circuit-1or2k.sh
bash .ci/test_circuit_config-usecase1.sh
bash .ci/test_circuit_config-usecase2.sh
bash .ci/test_circuit_config-usecase3.sh
bash .ci/test_circuit_config-usecase4.sh
# Usecase5 deactivated since some of the targets specified in the circuit config
# are not yet compatible with SpatialIndex.
# bash .ci/test_circuit_config-usecase5.sh
bash examples/run_ipynb.sh examples/basic_tutorial.ipynb

set +x
echo "[`date`] Run Finished"
