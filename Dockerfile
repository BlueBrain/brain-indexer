FROM python:latest

RUN apt-get update \
 && apt-get install -y libopenmpi-dev
RUN wget -qO- https://boostorg.jfrog.io/artifactory/main/release/1.85.0/source/boost_1_85_0.tar.bz2 | tar xjf - \
 && cd boost_1_85_0 \
 && ./bootstrap.sh \
 && ./b2 --prefix=/opt/boost --with-serialization --with-filesystem --with-test install
COPY . /workspace
ENV SKBUILD_CMAKE_DEFINE="CMAKE_INSTALL_RPATH_USE_LINK_PATH=ON"
ENV CMAKE_PREFIX_PATH=/opt/boost
RUN pip install "/workspace[mpi]"
