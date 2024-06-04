#! /usr/bin/env bash

set -xe

cd ${SONATA_EXTENSION_DIR}/source/usecases/usecase1

output_dir="$(mktemp -dt tmp-brain_indexer-XXXXX)"

direct_spi="${output_dir}/direct"
circuit_spi="${output_dir}/circuit"

brain-indexer-nodes --progress-bar nodes.h5 ../components/CircuitA/morphologies/swc -o "${direct_spi}"
brain-indexer-circuit segments --progress-bar circuit_sonata.json -o "${circuit_spi}"

brain-indexer-compare "${direct_spi}" "${circuit_spi}"

direct_spi="${output_dir}/direct2"
circuit_spi="${output_dir}/circuit2"

brain-indexer-synapses --progress-bar edges.h5 -o "${direct_spi}"
brain-indexer-circuit synapses --progress-bar circuit_sonata.json -o "${circuit_spi}"

brain-indexer-compare "${direct_spi}" "${circuit_spi}"

if [ ! -z ${n_mpi_ranks} ]; then
    multi_direct_spi="${output_dir}/multi_direct"
    multi_circuit_spi="${output_dir}/multi_circuit"

    mpirun -n ${n_mpi_ranks} brain-indexer-synapses edges.h5 -o "${multi_direct_spi}" --multi-index
    mpirun -n ${n_mpi_ranks} brain-indexer-circuit synapses circuit_sonata.json -o "${multi_circuit_spi}" --multi-index

    brain-indexer-compare "${direct_spi}" "${multi_direct_spi}"
    brain-indexer-compare "${direct_spi}" "${multi_circuit_spi}"
fi
