# `mc_core`

## Introduction

`mc_core` provides core functionality for all the `mc_dev` repositories. Primarily, this consits of:

  - Camera calibration
  - 3D rendering
  - image io
  - maths and miscellaneous core functionality

Documentation in the `mc_base` repository provides basic information about the other parts of the framework.

## Getting the source

The source is mostly developed by Murray Evans as part of the university of Bath's CAMERA research group. The source can be pulled from `github.com` or from CAMERA's own `gitolite` server.

Altough the `mc_core` repo can be compiled without any of the other `mc_` repositories, it is highly recommended (especially if you intend to make use of any other `mc_dev` repos) to first check out the `mc_base` repository.

```bash
  cd ~/where/you/keep/your/code
  git clone git@github.com:camera-mc-dev/mc_base.git mc_dev
  cd mc_dev
  git clone git@github.com:camera-mc-dev/mc_core.git
```

## Dependencies

`mc_core` and thus all of `mc_dev` depends on a number of external libraries. These can mostly be installed through your package manager.

  - [OpenCV](https://opencv.org/): 4.8.0 was most recently used. We typically build it ourselves to control the configuration:
    - Use both the [main](https://github.com/opencv/opencv) and [contrib](https://github.com/opencv/opencv_contrib) modules
    - when configuring:
      - enable `OPENCV_ENABLE_NONFREE` as we use SIFT and SURF
      - enable `OPENCV_GENERATE_PKGCONFIG`
      - Recommend enabling OpenMP for taking advantage of some parallel optimisations
      - Recommand enabling CUDA if you have it, but it wont affect `mc_dev` at all.
  - Rendering:
    - [SFML](https://www.sfml-dev.org/): Create and manage OpenGL contexts and handle interactivity
    - [GLEW](https://glew.sourceforge.net/): So we can use modern OpenGL
    - [freetype 2](http://freetype.org/): font rendering library
    - [EGL](https://docs.mesa3d.org/egl.html): used for headless OpenGL rendering - i.e. render without an OpenGL window, or even without a window manager.
  - [Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page): matrix maths library
  - [Boost filesystem](https://www.boost.org/doc/libs/1_83_0/libs/filesystem/doc/index.htm): Useful filesystem tools from boost - maybe one day we'll have the time to switch to the version in the C++ standard template library.
  - [Magick++](https://imagemagick.org/script/magick++.php): Image loading and saving library. At some point in history it was a notable advantage over what OpenCV had. 
  - [libconfig++](https://github.com/hyperrealm/libconfig): All our config files use this.
  - [snappy](https://github.com/google/snappy): Google's speed oriented compressions library. We needed a fast way to read/write lossless images to disk so threw together our own `.charImg` and `.floatImg` formats using this. 
  - [Ceres Solver](http://ceres-solver.org/): Used for bundle adjustment
  - [nanoflann](https://github.com/jlblancoc/nanoflann): Neat little library for fast nearest neighbour search trees with an Eigen interface
  - [HighFive](https://github.com/BlueBrain/HighFive): And HDF5 file library *Optional*
  - [Pandoc](https://pandoc.org/): "compile" markdown files into html or other formats.
  - [SCons](https://scons.org/): Python based build system. Think Make, only using Python and with a different set of compromises.

Most of those dependencies can be installed using your package manager. For example, in Ubuntu:

```bash
   sudo apt install libsfml-dev libglew-dev libfreetye-dev libegl-dev\
                    libboost-filesystem-dev \
                    libeigen3-dev           \
                    libmagick++-dev         \
                    libonfig++-dev          \
                    libsnappy-dev           \
                    libceres-dev            \
                    pandoc scons            
```

Nanoflann and HighFive are simple installs - clone the repositories and follow a normal CMake build process. You may encounter problems compiling against nanoflann newer than commit `d804d14325a7fcefc111c32eab226d15349c0cca` so checkout that one first.

As stated above, we tend to build OpenCV ourselves to ensure we have the configuration that we want - do adhere to the points above to get a build of OpenCV that will work with the `mc_dev` framework, particularly enabling the `pkgConfig` option as we don't use and will not be using CMake.

## Build configuration.

As with any software build, you will need to tell the build system where to find the libraries that it needs. This is currently done by the `mc_core/mcdev_core_config.py` script.

In most cases all of the above will be installed to standard locations and you wont need to modify this file. If you don't want to use HighFive, or install to non-standard locations, you may need to do some minor editing.

For each dependency you will find a `Find<X>()` function in the script. For most of the dependencies this just wraps calls to the way `scons` uses `pkg-config` type tools. Unless you've done something non-standard with your installs, then these defaults will probably satisfy you. Scons handles this with:

```python
env.ParseConfig("pkg-config libavformat --cflags --libs")
```

Basically, you pass in the same string you would use if you typed it on the command line.

Where a `pkg-config` is not available, Scons has simple ways to add flags, library paths, libraries, etc...

Need to specify an include path?

```python
env.Append(CPPPATH=['/path/to/include/'])
```

Need to add a library path?

```python
env.Append(LIBPATH=['/path/to/lib/'])
```

Need to add a library?

```python
env.Append(LIBS=['grumpy'])  # adds libgrumpy.a or .so or whatever.
```

Need to specify a pre-processor define or other C++ flag?

```python
env.Append(CPPFLAGS=['-Wall', '-DUSE_PIZZA' ])
```

Need to specify linker flags?

```python
env.Append(LINKFLAGS=['-framework', 'OpenGL'])
```



## Building

The project is built using the `scons` build system. If you have not already done so, you will need to install `scons` now, probably using your package manager.

```bash
  $ sudo apt install scons
```

Next, change into your `mc_core` directory, e.g.:

```bash
  $ cd ~/where/you/keep/your/code/mc_dev/mc_core
```

To build everything in optimised mode just type (using 5 build jobs):

```bash
  $ scons -j5
```

## Runtime configuration

All of the `mc_dev` tools will look for a common configuration file in your home directory. This is a "hidden" or "dot" file: `~/.mc_dev.common.cfg`. If you run any tool you should find that it creates a default version of this file, but you will need to modify the paths. In general, it will look like this:

```bash
dataRoot = "/path/to/your/data";
shadersRoot = "/path/to/mc_dev/mc_core/shaders";
coreDataRoot = "/path/to/mc_dev/mc_core/data";
scriptsRoot = "/path/to/mc_dev/mc_core/python";
maxSingleWindowWidth = 1920;
maxSingleWindowHeight = 1280;
fmpegPath = "/usr/bin/ffmpeg"
```


## Tools

`mc_core` supplies the following tools:

  - `renderSyncedSources`: Given a list of image sources (directory of image files, individual videos) which are time sychronised (i.e. frame `f` of source `na` is the same as frame `f` of source `nb` ) this will show the image sources as a grid in one window. Can handle up to 16 sources at once.
  - `renderSyncedSourcesHeadless`: As above, but does the rendering using EGL so that there is no need for a window, or even a window manager. Rendered images are instead written to disk.
  - `convertImg`: given an image `a` will convert the image and save out as `b`. This is particularly useful when using `mc_dev`'s custom `.charImg` and `.floatImg` files.
  - `circleGridCamNetwork`: This performs calibration of a network of cameras using observations of a suitable calibration grid.
  - `calibCheck`: Tool for visualising the result of a calibration. Draws cameras and origin on each view.
  - `pointMatcher`: Calibration can often benefit from an extra set of point matches between camera views, particularly in circular arrangements of cameras where half the cameras see the un-patterned back side of the calibration grid. This tool allows the user to annotate such matches.
  - `manualAlignNetwork`: Given a calibration and a set of points (desired origin, point on desired x-axis, point on desired y-axis) this tool will reorient the calibration so that it has the desired origin and orientation.
  - `makeGroundPlaneImage`: Useful little visualisation tool. Projects camera views onto the z=0 plane which by convention is the ground plane. This can be very useful for identifying if any cameras in the calibration are sub-optimal.

## Documentation

You can find more documentation, such as advice on calibration, and structure of the library, under the `docs/` folder in the repository. You can either directly read the markdown files, or 'compile' that into an html book.

To make the html book of the documentation:

  - install pandoc: `sudo apt install pandoc`
  - enter the docs directory: `cd docs/`
  - run the `.book` file: `./mc_core.book`
    - the `.book` file is actually a python script which wraps up calls to `pandoc` for making a nice html book from the markdown files, so you could equally run `python mc_core.book`


