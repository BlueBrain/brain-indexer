#!/bin/env python
"""
    Blue Brain Project - Spatial-Index

    A small example script on how to create a circuit segment multi index
    with SONATA (and perform spatial queries).
"""

from mpi4py import MPI

from spatial_index import MorphMultiIndex, MorphMultiIndexBuilder

# Loading some small circuits and morphology files on BB5
CIRCUIT_1K = "/gpfs/bbp.cscs.ch/project/proj12/jenkins/cellular/circuit-1k"
NODES_FILE = CIRCUIT_1K + "/nodes.h5"
MORPH_FILE = CIRCUIT_1K + "/morphologies/ascii"

OUTPUT_DIR = "tmp-doei"


def example_create_multi_index_from_sonata():
    # Create a new indexer and load the nodes and morphologies
    # directly from the SONATA file
    MorphMultiIndexBuilder.from_sonata_file(
        MORPH_FILE,
        NODES_FILE,
        "All",
        output_dir=OUTPUT_DIR,
    )


def example_query_multi_index():
    if MPI.COMM_WORLD.Get_rank() == 0:
        # The index may use at most roughly 1e6 bytes.
        core_index = MorphMultiIndex.open_core_index(OUTPUT_DIR, mem=int(1e6))

        # Define a query window by its two extreme corners, and run the
        # query.
        min_corner, max_corner = [-50, 0, 0], [0, 50, 50]
        found = core_index.find_intersecting_window_np(min_corner, max_corner)

        # Now you're ready for the real science:
        print(found)


if __name__ == "__main__":
    example_create_multi_index_from_sonata()
    example_query_multi_index()
