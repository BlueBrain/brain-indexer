#! /usr/bin/env bash

set -e

if [[ ! -z ${DATADIR} ]]
then
    export SI_DATADIR=${DATADIR}/spatial_index
fi

if [[ -z ${SI_DATADIR} ]]
then
    echo "SI_DATADIR not set."
    exit -1
fi

circuit_config_seg="${SI_DATADIR}/circuit_config-2k.json"

pushd ${SI_DATADIR}

output_dir=$(mktemp -d ~/tmp-spatial_index-XXXXX)
spatial-index-nodes nodes.h5 ascii_sonata -o "${output_dir}/direct.spi"
spatial-index-circuit segments "${circuit_config_seg}" -o "${output_dir}/circuit.spi"

if ! cmp "${output_dir}/direct.spi" "${output_dir}/circuit.spi"
then
    echo "The output from '*-nodes' and '*-circuit' differ."
    exit -1
fi
