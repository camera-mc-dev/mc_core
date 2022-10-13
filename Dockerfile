ROM --platform=linux/amd64 opencv_opt:4.6.0

## scons stuff
RUN apt update && apt install -y --no-install-recommends scons libswscale-dev libavformat-dev libconfig++-dev

# our dependencies (not included in base image)
RUN apt install -y \
	libsfml-dev \
	libglew-dev \
	libfreetype-dev \
	libegl-dev \
	libeigen3-dev \
	libboost-filesystem-dev \
	libmagick++-dev \
	libconfig++-dev \
	libsnappy-dev \
	libceres-dev \
	ffmpeg \
    libassimp-dev 

# and we need HighFive for hdf5 files
WORKDIR /deps/
RUN apt update
RUN apt install libhdf5-dev -y --no-install-recommends
RUN apt install libboost-serialization-dev -y --no-install-recommends
RUN git clone https://github.com/BlueBrain/HighFive.git
WORKDIR /deps/HighFive
RUN mkdir build
WORKDIR /deps/HighFive/build
RUN cmake HIGHFIVE_UNIT_TESTS=OFF ../
RUN make -j6
RUN make install

# and we use nanoflann in various places
WORKDIR /deps/
RUN git clone https://github.com/jlblancoc/nanoflann.git
WORKDIR /deps/nanoflann
RUN mkdir build
WORKDIR /deps/nanoflann/build
RUN cmake ../
RUN make -j6
RUN make install

WORKDIR /home/mc_dev/mc_core
COPY . .
RUN scons -j8

