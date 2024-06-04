#! /usr/bin/env bash

set -e

cd ${SONATA_EXTENSION_DIR}/source/usecases/usecase2

output_dir="$(mktemp -dt tmp-brain_indexer-XXXXX)"

segments_spi="${output_dir}/circuit-segments"
segments_NodeA_spi="${output_dir}/circuit-NodeA-segments"
synapses_spi="${output_dir}/circuit-synapses"

brain-indexer-circuit segments circuit_sonata.json -o "${segments_spi}"
python3 << EOF
import brain_indexer
index = brain_indexer.open_index("${segments_spi}")
sphere = [0.0, 0.0, 0.0], 1000.0

result = index.sphere_query(*sphere, fields="gid")
assert result.size != 0, f"'result.size' isn't zero."
EOF

brain-indexer-circuit segments circuit_sonata.json --populations "NodeA" -o "${segments_NodeA_spi}"
python3 << EOF
import brain_indexer
index = brain_indexer.open_index("${segments_spi}")
sphere = [0.0, 0.0, 0.0], 1000.0

result = index.sphere_query(*sphere, fields="gid")
assert result.size != 0, f"'result.size' isn't zero."
EOF


brain-indexer-circuit synapses circuit_sonata.json \
    --populations NodeA__NodeA__chemical VirtualPopA__NodeA__chemical \
    -o "${synapses_spi}"

python3 << EOF
import brain_indexer
index = brain_indexer.open_index("${synapses_spi}")
sphere = [0.0, 0.0, 0.0], 1000.0

results = index.sphere_query(*sphere, fields="id")
for pop in ["NodeA__NodeA__chemical", "VirtualPopA__NodeA__chemical"]:
    assert pop in results, f"'{pop}' missing."

for result in results.values():
    assert result.size != 0, f"{pop}: size isn't zero."

EOF
