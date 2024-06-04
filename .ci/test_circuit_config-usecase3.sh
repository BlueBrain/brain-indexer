#! /usr/bin/env bash

set -e

cd ${SONATA_EXTENSION_DIR}/source/usecases/usecase3

output_dir="$(mktemp -dt tmp-brain_indexer-XXXXX)"

segments_spi="${output_dir}/circuit-segments"
synapses_spi="${output_dir}/circuit-synapses"

brain-indexer-circuit segments circuit_sonata.json \
    --populations NodeA NodeB \
    -o "${segments_spi}"

python3 << EOF
import brain_indexer
index = brain_indexer.open_index("${output_dir}/circuit-segments")
sphere = [0.0, 0.0, 0.0], 1000.0

results = index.sphere_query(*sphere, fields="gid")
assert "NodeA" in results
assert "NodeB" in results

for result in results.values():
    assert result.size != 0

EOF

brain-indexer-circuit synapses circuit_sonata.json \
    --populations NodeA__NodeA__chemical NodeB__NodeB__chemical \
    -o "${synapses_spi}"

python3 << EOF
import brain_indexer
index = brain_indexer.open_index("${output_dir}/circuit-synapses")
sphere = [0.0, 0.0, 0.0], 1000.0

results = index.sphere_query(*sphere, fields="id")
assert "NodeA__NodeA__chemical" in results
assert "NodeB__NodeB__chemical" in results

for result in results.values():
    assert result.size != 0

EOF
