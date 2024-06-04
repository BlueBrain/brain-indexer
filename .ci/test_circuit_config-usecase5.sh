#! /usr/bin/env bash

set -e

cd ${SONATA_EXTENSION_DIR}/source/usecases/usecase5

output_dir="$(mktemp -dt tmp-brain_indexer-XXXXX)"

segments_spi="${output_dir}/circuit-segments"
synapses_spi="${output_dir}/circuit-synapses"

brain-indexer-circuit segments circuit_sonata.json \
    --populations nodeA \
    -o "${segments_spi}"

python3 << EOF
import brain_indexer
index = brain_indexer.open_index("${segments_spi}")
sphere = [0.0, 0.0, 0.0], 1000.0

results = index.sphere_query(*sphere, fields="gid")
assert len(results) > 0
EOF

# We want this to fail until 'astrocytes' are supported.
if [ $(brain-indexer-circuit segments circuit_sonata.json \
                                      --populations astrocyteA \
                                      -o "${segments_spi}" &> /dev/null) ]
then
  echo "'--populations astrocyteA' was accepted, but should not have been."
  exit -1
fi


brain-indexer-circuit synapses circuit_sonata.json \
    --populations nodeA__nodeA__chemical astrocyteA__astrocyteA__glialglial \
    -o "${synapses_spi}"

python3 << EOF
import brain_indexer
index = brain_indexer.open_index("${synapses_spi}")
sphere = [0.0, 0.0, 0.0], 1000.0

results = index.sphere_query(*sphere, fields="id")
assert "nodeA__nodeA__chemical" in results
assert "astrocyteA__astrocyteA__glialglial" in results

for result in results.values():
    assert result.size != 0

EOF
