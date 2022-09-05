"""
    High level command line commands
"""

import spatial_index
from .util import docopt_get_args, is_likely_same_index
from .resolver import open_index, MorphIndexResolver, SynapseIndexResolver


def spatial_index_nodes(args=None):
    """spatial-index-nodes

    Usage:
        spatial-index-nodes [options] <nodes-file> <morphology-dir>
        spatial-index-nodes --help

    Options:
        -v, --verbose            Increase verbosity level
        -o, --out=<folder>       The index output folder [default: out]
        --multi-index            Whether to create a multi-index
    """
    options = docopt_get_args(spatial_index_nodes, args)
    _run_spatial_index_nodes(options["morphology_dir"], options["nodes_file"], options)


def spatial_index_synapses(args=None):
    """spatial-index-synapses

    Usage:
        spatial-index-synapses [options] <edges_file> [<population>]
        spatial-index-synapses --help

    Options:
        -v, --verbose            Increase verbosity level
        -o, --out=<folder>       The index output folder [default: out]
        --multi-index            Whether to create a multi-index
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

    Currently, only a single population is supported. Therefore, the
    'networks/{nodes,edges}' must identify a unique file. This is possible
    if either the list only contains one dictionary, or, if a population has
    been selected, only one file matches the specified population.

    Note: requires libsonata

    Usage:
        spatial-index-circuit segments <circuit-file> [options]
        spatial-index-circuit synapses <circuit-file> [options]
        spatial-index-circuit --help

    Options:
        -o, --out=<out_file>     The index output folder [default: out]
        --multi-index            Whether to create a multi-index
        --populations=<populations>...  Restrict the spatial index to the listed
                                        Currently, at most one population is supported
    """
    options = docopt_get_args(spatial_index_circuit, args)
    circuit_config, json_config = _sonata_circuit_config(options["circuit_file"])
    population = _validated_population(circuit_config, options)

    if options['segments']:
        nodes_file = _sonata_nodes_file(json_config, population)
        morphology_dir = _sonata_morphology_dir(circuit_config, population)
        _run_spatial_index_nodes(morphology_dir, nodes_file, options)

    elif options['synapses']:
        edges_file = _sonata_edges_file(json_config, population)
        _run_spatial_index_synapses(edges_file, population, options)

    else:
        raise NotImplementedError("Missing subcommand.")


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
        spatial_index.logger.info("The two indexes differ.")
        exit(-1)


def _validated_population(circuit_config, options):
    populations = options["populations"]

    if options["segments"]:
        available_populations = circuit_config.node_populations

    elif options["synapses"]:
        available_populations = circuit_config.edge_populations

    else:
        raise NotImplementedError("Missing circuit kind.")

    if populations is None:
        populations = available_populations

    error_msg = "At most one population is supported."
    assert len(populations) == 1, error_msg

    return next(iter(populations))


def _sonata_select_by_population(iterable, key, population):
    def matches_population(n):
        return population == "All" or population in n.get("populations", dict())

    selection = [n[key] for n in iterable if matches_population(n)]
    assert len(selection) == 1, "Couln't determine a unique '{}'.".format(key)

    return selection[0]


def _sonata_circuit_config(config_file):
    import libsonata
    import json

    circuit_config = libsonata.CircuitConfig.from_file(config_file)
    json_config = json.loads(circuit_config.expanded_json)

    return circuit_config, json_config


def _sonata_nodes_file(config, population):
    nodes = config["networks"]["nodes"]
    return _sonata_select_by_population(nodes, key="nodes_file", population=population)


def _sonata_edges_file(config, population):
    edges = config["networks"]["edges"]
    return _sonata_select_by_population(edges, key="edges_file", population=population)


def _sonata_morphology_dir(config, population):
    node_prop = config.node_population_properties(population)
    return node_prop.morphologies_dir


def _run_spatial_index_nodes(morphology_dir, nodes_file, options):

    if options["multi_index"]:
        index_kind = "multi_index"
        index_kwargs = {}
    else:
        index_kind = "in_memory"
        index_kwargs = {"progress": True}

    Builder = MorphIndexResolver.builder_class(index_kind)
    Builder.create(
        morphology_dir, nodes_file, output_dir=options["out"], **index_kwargs
    )


def _run_spatial_index_synapses(edges_file, population, options):

    if options["multi_index"]:
        index_kind = "multi_index"
        index_kwargs = {}
    else:
        index_kind = "in_memory"
        index_kwargs = {"progress": True}

    Builder = SynapseIndexResolver.builder_class(index_kind)
    Builder.from_sonata_file(
        edges_file, population, output_dir=options["out"], **index_kwargs
    )
