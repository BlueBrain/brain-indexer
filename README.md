# brain-indexer

brain-indexer is a library for efficient spatial queries on large datasets (TBs
and more). It is currently based on `boost::rtree`.

It provides a high level Python API for indexes of simple geometric shapes
(boxes, spheres and cylinders) and the required functionalities to
create indexes of synapses and morphologies.

## Installation

### From PyPI

A version of brain-indexer without processing on multiple nodes is available on PyPI and
can easily be installed with:
```console
pip install brain-indexer
```

### From source

brain-indexer requires Boost with a minimum version of 1.79.0, but preferably 1.80.0 or
newer.  If your system does not provide such a version, one can install a more recent one
into `/opt/boost/` as follows:
```console
wget -qO- https://boostorg.jfrog.io/artifactory/main/release/1.85.0/source/boost_1_85_0.tar.bz2 | tar xjf -
cd boost_1_85_0
./bootstrap.sh
./b2 --prefix=/opt/boost --with-serialization --with-filesystem --with-test install
```
When using a custom Boost installation like this, **it is imperative to set the following
environment variables to make sure that all libraries are found**:
```console
export SKBUILD_CMAKE_DEFINE="CMAKE_INSTALL_RPATH_USE_LINK_PATH=ON"
export CMAKE_PREFIX_PATH=/opt/boost
```

If multi-node processing via MPI is required, the following system dependencies have to be
installed:
```console
sudo apt-get install -y libopenmpi-dev
```

Then the installation of brain-indexer can proceed as usual, either with MPI support:
```console
gh repo clone BlueBrain/brain-indexer
pip install "./brain-indexer[mpi]"
```
or without support:
```console
gh repo clone BlueBrain/brain-indexer
pip install ./brain-indexer/
```

## Where to start

We provide two Jupyter notebooks with a hands-on introduction to brain-indexer:
- A basic introduction in [`basic_tutorial.ipynb`](./examples/basic_tutorial.ipynb)
- More complex use-cases in [`advanced_tutorial.ipynb`](./examples/advanced_tutorial.ipynb)

### Examples

More examples on how to use brain-indexer are available in the [`examples`](./examples) folder:
- [`segment_index.py`](./examples/segment_index.py): simple indexing and querying of a segment index
- [`synapses_index.py`](./examples/synapses_index.py): simple indexing and querying of a synapse index
- [`segment_index_sonata.py`](./examples/segment_index_sonata.py): indexing and querying of a segment index using SONATA files
- [`segment_multi_index_sonata.py`](./examples/segment_multi_index_sonata.py): indexing and querying of a segment multi-index using SONATA files
- [`synapse_multi_index_sonata.py`](./examples/synapse_multi_index_sonata.py): indexing and querying of a synapse multi-index using SONATA files

Also, the `tests` folder contains some tests that double also as examples on how to use
brain-indexer.

## Acknowledgment

The development of this software was supported by funding to the Blue Brain Project,
a research center of the École polytechnique fédérale de Lausanne (EPFL),
from the Swiss government's ETH Board of the Swiss Federal Institutes of Technology.

Copyright (c) 2019-2024 Blue Brain Project/EPFL
