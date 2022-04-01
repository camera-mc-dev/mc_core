FROM --platform=linux/amd64 mc_base
## scons stuff
RUN apt update && apt install -y --no-install-recommends scons libswscale-dev libavformat-dev libconfig++-dev && rm -rf /var/lib/apt/lists/* 
## our dependencies
RUN apt update && apt install --no-install-recommends -y libsfml-dev libglew-dev libfreetype-dev libegl-dev libeigen3-dev libboost-filesystem-dev libmagick++-dev libconfig-dev libsnappy-dev libceres-dev libnanoflann-dev libhdf5-dev && rm -rf /var/lib/apt/lists/* 
WORKDIR /root/mc_dev/mc_core
COPY . .

## install highfive
WORKDIR HighFive/build
RUN cmake .. -DHIGHFIVE_EXAMPLES=OFF -DHIGHFIVE_USE_BOOST=OFF && make install

WORKDIR /root/mc_dev/mc_core
RUN scons -j5 
