# `mc_core`

## Introduction

`mc_core` is the central repository for the `mc_dev` set of repositories. As the name suggests, it provides all of the core functionality that is shared between the various parts of the project. Primarily, this consits of:

  - Camera calibration
  - 3D rendering
  - image io
  - maths and miscellaneous core functionality

The CAMERA wiki provides an overview of the various other parts of `mc_dev`.

## Getting the source

The source is mostly developed by Murray Evans as part of the university of Bath's CAMERA research group. The source can be pulled from the CAMERA git server *rivendell* - please see the wiki for details on using and accessing the git server.

Altough the `mc_core` repo can be compiled without any of the other `mc_` repositories, it is highly recommended (especially if you intend to make use of any other `mc_dev` repos) to first check out the `mc_base` repository.

```bash
  $ cd ~/where/you/keep/your/code
  $ git clone camera@rivendell.cs.bath.ac.uk:mc_base mc_dev
  $ cd mc_dev
  $ git clone camera@rivendell.cs.bath.ac.uk:mc_core
```

## Building

The project is built using the `scons` build system. If you have not already done so, you will need to install `scons` now, probably using your package manager or Homebrew on Mac. e.g. for Ubuntu based systems:

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

Obviously, that will fail if you have not yet installed all the required dependencies or modified the build to find those dependencies.

## Dependencies

`mc_core` and thus all of `mc_dev` depends on a number of external libraries. These can mostly be installed through your package manager.

  - OpenCV: 3.4.9 will work. Although it can come from a package manager, you should build it yourself to be sure you have:
    - You will need the contrib packages
    - You will need to enable `OPENCV_ENABLE_NONFREE` as we use SIFT and SURF
    - Recommend enabling OpenMP for taking advantage of some parallel optimisations
    - Use CUDA if you have it.
    - Update to 4.x branch is on the ToDo list...
  - Rendering: 
    - SFML: Create and manage OpenGL contexts and handle interactivity
    - GLEW: So we can use modern OpenGL
    - freetype 2: font rendering library
    - EGL: used for headless OpenGL rendering - i.e. render without an OpenGL window, or even without a window manager.
    - `sudo apt install libsfml-dev libglew-dev libfreetype-dev libegl-dev`
  - Eigen: matrix maths library
    - `sudo apt install libeigen3-dev`
  - Boost filesystem
    - `sudo apt install libboost-filesystem-dev`
  - Magick++: image loading library - I had fondness for this over OpenCV, but may remove requirement.
    - `sudo apt install libmagick++-dev`
  - libconfig : parse config files.
    - `sudo apt install libconfig-dev`
  - snappy : Google's fast compression library - used for custom `.charImg` and .`floatImg` 
    - `sudo apt install libsnappy-dev`
  - ceres solver
    - `sudo apt install libceres-dev`
  - High5 *optional*: An HDF5 file library
    - https://github.com/BlueBrain/HighFive
    - edit the `FindHDF5()` in `mcdev_core_config.py` if you use it.

Although OpenCV *can* typically be installed through a package manager, we advise building it yourself because you can a) ensure you enable all the parts you might want, including CUDA and OpenMP support, as well as the `contrib` modules and the non-free modules (for using SIFT and SURF features for example). The 4.x branch of OpenCV appears to have included some notable restructuring and so, at the present time, `mc_dev` can not be compiled against 4.x - we expect to rectify this in the near future but it is low priority.

## Specifing where dependencies are

As with any software build, you will need to tell the build system where to find the libraries that it needs. This is currently done by the `mc_core/mcdev_core_config.py` script.

For each dependency you will find a `Find<X>()` function in the script. For most of the dependencies this just wraps calls to the way `scons` uses `pkg-config` type tools. Unless you've done something non-standard with your installs, then these defaults will probably satisfy you. Scons handle this with:

```python
env.ParseConfig("pkg-config libavformat --cflags --libs")
```

Basically, you pass in the same string you would use if you typed it on the command line.

We may in time modify these `Find<X>()` functions to do the hunting for you like `CMake` promises (and normally fails) to do. Anyway.

Scons is pretty simple.

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

For more details on how the build system is set up, see the main documentation.



## Tools

`mc_core` supplies the following tools:

  - `renderSyncedSources`: Given a list of image sources (directory of image files, individual videos) which are time sychronised (i.e. frame `f` of source `na` is the same as frame `f` of source `nb` ) this will show the image sources as a grid in one window. Can handle up to 16 sources at once.
  - `renderSyncedSourcesHeadless`: As above, but does the rendering using EGL so that there is no need for a window, or even a window manager. The grid of images is instead written to disk.
  - `convertImg`: given an image `a` will convert the image and save out as `b`. This is particularly useful when using `mc_dev`'s custom `.charImg` and `.floatImg` files.
  - `circleGridCamNetwork`: This performs calibration of a network of cameras, where the input is a set of synchronised images sources where each camera observed a calibration target consisting of a grid of circles.
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
    - the `.book` file is actually a python script which wraps up calls to `pandoc` for making a nice html book from the markdown files.
