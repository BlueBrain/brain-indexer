"""
    High level command line commands
"""

import os

from spatial_index import logger

from .util import docopt_get_args, is_likely_same_index
from .util import is_strictly_sensible_filename, is_non_string_iterable
from .io import write_multi_population_meta_data
from .resolver import open_index, MorphIndexResolver, SynapseIndexResolver


def spatial_index_nodes(args=None):
    """spatial-index-nodes

    Usage:
        spatial-index-nodes [options] <nodes-file> <morphology-dir>
        spatial-index-nodes --help

    Options:
        -v, --verbose              Increase verbosity level
        -o, --out=<folder>         The index output folder [default: out]
        --multi-index              Whether to create a multi-index
        --population <population>  The population to index.
    """
    options = docopt_get_args(spatial_index_nodes, args)
    _run_spatial_index_nodes(
        options["morphology_dir"],
        options["nodes_file"],
        options.get("population"),
        options
    )


def spatial_index_synapses(args=None):
    """spatial-index-synapses

    Usage:
        spatial-index-synapses [options] <edges_file>
        spatial-index-synapses --help

    Options:
        -v, --verbose              Increase verbosity level
        -o, --out=<folder>         The index output folder [default: out]
        --multi-index              Whether to create a multi-index
        --population <population>  The population to index.
    """
    options = docopt_get_args(spatial_index_synapses, args)
    _run_spatial_index_synapses(options["edges_file"], options.get("population"), options)


def spatial_index_circuit(args=None):
    """spatial-index-circuit

    Create an index for the circuit defined by a SONATA circuit config. The
    index can either be a segment index or a synapse index.

    The segment index expects the SONATA config to provide:
        components/morphologies_dir
        networks/nodes

    For a synapse index we expect the SONATA config to provide
        networks/edges

    Multiple populations are supported through the flag `--populations`. When
    indexing multiple populations, one must list all populations to be indexed.
    When indexing a single population, one may omit `--populations` if the
    population is unique.

    Note: requires libsonata

    Usage:
        spatial-index-circuit segments <circuit-file> [options]
                              [(--populations <populations>) [<populations>...]]
        spatial-index-circuit synapses <circuit-file> [options]
                              [(--populations <populations>) [<populations>...]]
        spatial-index-circuit --help

    Options:
        -o, --out=<out_file>     The index output folder [default: out]
        --multi-index            Whether to create a multi-index
    """
    options = docopt_get_args(spatial_index_circuit, args)
    circuit_config, json_config = _sonata_circuit_config(options["circuit_file"])
    populations = _validated_populations(options, circuit_config)

    if populations is not None and is_non_string_iterable(populations):
        _spatial_index_circuit_multi_population(
            options, circuit_config, json_config, populations
        )
    else:
        output_dir = options["out"]
        _spatial_index_circuit_single_population(
            options, circuit_config, json_config, populations, output_dir
        )


def _spatial_index_circuit_single_population(options, circuit_config, json_config,
                                             population, output_dir):
    if options['segments']:
        props = circuit_config.node_population_properties(population)
        nodes_file = props.elements_path
        morphology_dir = _sonata_morphology_dir(circuit_config, population)
        _run_spatial_index_nodes(
            morphology_dir, nodes_file, population, options, output_dir=output_dir
        )

    elif options['synapses']:
        props = circuit_config.edge_population_properties(population)
        edges_file = props.elements_path
        _run_spatial_index_synapses(
            edges_file, population, options, output_dir=output_dir
        )

    else:
        raise NotImplementedError("Missing subcommand.")


def _spatial_index_circuit_multi_population(options, circuit_config, json_config,
                                            populations):
    basedir = options["out"]

    for pop in populations:
        output_dir = os.path.join(basedir, pop)
        _spatial_index_circuit_single_population(
            options, circuit_config, json_config, pop, output_dir=output_dir
        )

    element_type = "synapse" if options["synapses"] else "morphology"
    write_multi_population_meta_data(basedir, element_type, populations)


def spatial_index_compare(args=None):
    """spatial-index-compare

    Compares two circuits and returns with a non-zero exit code
    if a difference was detect. Otherwise the exit code is zero.

    Usage:
        spatial-index-compare <lhs-circuit> <rhs-circuit>
    """
    options = docopt_get_args(spatial_index_compare, args)

    lhs = open_index(options["lhs_circuit"])
    rhs = open_index(options["rhs_circuit"])

    if not is_likely_same_index(lhs, rhs):
        logger.info("The two indexes differ.")
        exit(-1)


def _sonata_available_populations(options, circuit_config):
    if options["segments"]:
        detected_populations = circuit_config.node_populations
        unsupported_types = ["virtual"]
        get_properties = circuit_config.node_population_properties

    elif options["synapses"]:
        detected_populations = circuit_config.edge_populations
        unsupported_types = []
        get_properties = circuit_config.edge_population_properties

    else:
        raise NotImplementedError("Missing circuit kind.")

    def is_supported(population):
        props = get_properties(population)
        return props.type not in unsupported_types

    available_populations = list(filter(is_supported, detected_populations))

    if not available_populations:
        raise ValueError(
            f"No supported populations found. Detected: {detected_populations}"
        )

    return available_populations


def _validated_single_population(options, circuit_config, population):
    available_populations = _sonata_available_populations(options, circuit_config)

    if options["segments"]:
        properties = circuit_config.node_population_properties(population)
        supported_types = ["biophysical"]

    elif options["synapses"]:
        properties = circuit_config.edge_population_properties(population)
        supported_types = ["electrical", "chemical", "synapse_astrocyte"]
        # These are needed to function with `libsonata`.
        supported_types += ["electrical_synapse", "chemical_synapse"]

    else:
        raise NotImplementedError("Missing case.")

    if properties.type not in supported_types:
        raise ValueError(f"{properties.type=} not in {supported_types} for {population=}")

    if population not in available_populations:
        raise ValueError(f"{population=} not in {available_populations=}")

    message = (
        "SpatialIndex needs to be checked before it can use 'exotic' population names."
    )
    assert is_strictly_sensible_filename(population), message

    return population


def _validated_populations(options, circuit_config):
    populations = options["populations"]

    if populations:
        if is_non_string_iterable(populations):
            for pop in populations:
                _validated_single_population(options, circuit_config, pop)

        else:
            _validated_single_population(options, circuit_config, populations)

        return populations

    else:
        # Any falsey `populations`, e.g., `[]`, `""`, ... means default.
        available_populations = _sonata_available_populations(options, circuit_config)

        if len(available_populations) == 0:
            raise ValueError("No populations found.")

        if len(available_populations) > 1:
            logger.error(
                "Detected multiple populations {available_populations}."
                " Please select a population with '--populations'."
            )
            raise ValueError("Too many populations to select a fallback value.")

        population = next(iter(available_populations))
        _validated_single_population(options, circuit_config, population)

        return population


def _sonata_circuit_config(config_file):
    import libsonata
    import json

    circuit_config = libsonata.CircuitConfig.from_file(config_file)
    json_config = json.loads(circuit_config.expanded_json)

    return circuit_config, json_config


def _sonata_morphology_dir(config, population):
    node_prop = config.node_population_properties(population)
    return node_prop.morphologies_dir


def _parse_options_for_builder_args(options, output_dir):
    if options["multi_index"]:
        index_variant = "multi_index"
        index_kwargs = {}
    else:
        index_variant = "in_memory"
        index_kwargs = {"progress": True}

    if output_dir is None:
        output_dir = options["out"]

    index_kwargs["output_dir"] = output_dir

    return index_variant, index_kwargs


def _run_spatial_index_nodes(morphology_dir, nodes_file, population, options,
                             output_dir=None):
    index_variant, index_kwargs = _parse_options_for_builder_args(options, output_dir)

    Builder = MorphIndexResolver.builder_class(index_variant)
    Builder.from_sonata_file(
        morphology_dir, nodes_file, population, **index_kwargs
    )


def _run_spatial_index_synapses(edges_file, population, options, output_dir=None):
    index_variant, index_kwargs = _parse_options_for_builder_args(options, output_dir)

    Builder = SynapseIndexResolver.builder_class(index_variant)
    Builder.from_sonata_file(
        edges_file, population, **index_kwargs
    )
