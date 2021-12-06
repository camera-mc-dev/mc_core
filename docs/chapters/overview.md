## The Library

`mc_core` exists to provide basic and fundamental tools for the rest of the `mc_dev` framework, especially camera calibration and basic rendering functions for debug and display.

The calibration tools have already been documented in the previous chapter, and the remaining portions of this book will focus more on the actual software, implementations, etc.. for future developers.

### Basic structure

The `mc_core` repository is split between three directories:

  - `src/`
  - `apps/`
  - `tests/`

As per the other parts of `mc_dev` we have various directories under `src/` which each contain C++ source files grouped together in common functions. These are all compiled to binaries and gathered together into a single library, `mc-core/build/<build-mode>/src/libMC-core.a`

Under `apps/` you will find the single-source-file tools provided with the library for calibration, video source viewing, etc... These will all be compiled to binaries under `mc-core/build/<build-mode>/bin`.

Under `tests/` you will find any small videos for testing features of the library. These will all be compiled to binaries under `mc-core/build/<build-mode>/tests`

### Library

The main parts of the `mc_core` library are:

   - `src/calib`: The calibration algorithms
   - `src/commonConfig`: Small class to handle common configuration for all framework tools
   - `src/imgio`: All source relating to image loading, saving and sources
   - `src/math`: All source related to maths.
   - `src/misc`: Several miscellaneous functions and classes.
   - `src/renderer2`: All the rendering classes.

### Common Config

Most of the framework depends upon knowing a few common parameters, especially locations where to find the renderer's shaders. There is a single header-only class `CommonConfig` defined in `src/commonConfig/commongConfig.h` which handles making sure a user *has* a common config file and otherwise reading the settings from that file.

Whenever a framework tool is loaded that uses the `CommonConfig` class, the class checks in the user's home directory for a hidden dot file: `.mc_dev.common.cfg`. If the file is not there, it will create a default one. If the file is there, it will read the settings from the file.

Note that before anybody can use one of the `mc_dev` tools, they will need to adjust their personal `.mc_dev.common.cfg` file. A typical file will look like:

```bash
dataRoot = "/Users/me475/programming/mc_dev/data/";
shadersRoot = "/Users/me475/programming/mc_dev/shaders/";
coreDataRoot = "/Users/me475/programming/mc_dev/data/";
scriptsRoot = "/Users/me475/programming/mc_dev/python/";
maxSingleWindowWidth = 2200;
maxSingleWindowHeight = 1200;
```

Here, the user can set their preferred global `dataRoot`, meaning they can leave that setting out of other config files.

The more important things to set are to make sure that the `shadersRoot` points to the shaders for the `mc_core` renderer, and the path to the core data, which at time of writing consists of a directory containing a single font file.

### Image Loading/Saving

Throughout `mc_dev` images are stored and manipulated using the OpenCV `Mat` datastructure. And so, image loading *can* just be done using the OpenCV loading and saving functions. However, you *should* use the `src/imgio/loadsave.h` header and the functions 

 - `SaveImage( cv::Mat &img, std::string filename )`
 - `LoadImage( std::string filename )`

The `SaveImage` and `LoadImage` functions primarily make use of `Magick++` for loading and saving which have some advantages over using OpenCV, but they also handle the framework specific `.floatImg` and `.charImg` format. Speaking of which...

#### Custom image formats

The `.floatImg` and `.charImg` formats were created to deal with two small problems:

  - The lack of a fast and simple float32 image format that suits our needs and has no other baggage.
  - The need to read/write at pace without completely killing disk space.

The two file formats are binary file formats with a very simple structure. There is a short header with a magic number and details of size of the file and whether it is float or byte (character) data. The data is mildly compressed using Google's [Snappy](https://github.com/google/snappy) which favours speed over disk space.

These image formats are a very useful compromise for the needs of the overall project.

### Maths

Maths in the `mc_dev` framework is mostly handled by the `Eigen` library, which can make for a bit of annoyance in swapping between OpenCV and Eigen every now and then, but it is worth it for the nice Matrix classes of Eigen that are not trying to worry about being images as well.

The source files in `mc_core/src/math/` provide a number of useful extra functions as well as some key `typedefs`.

  - `mathTypes.h`: This defines matrix types used heavily throughout the framework.
  - `distances.h`: Various distance computations, such as point-line and point-plane etc.
  - `intersections.h`: Various intersection computations, line-line, multiple lines in 3D, etc..
  - `products.h`: a Cross Product function for homogeneous 3D vectors
  - `matrixGenerators.h`: Functions to generate useful matrices, such as rotations, scales, and the `LookAt` which helps to create virtual cameras looking from a point towards a second point.
  - `miscMath.h`: A few extras. Most notably code to compute the rigid transformation or rigid rotation between two sets of points.

The `mathTypes.h` file is interesting. Most of the time we deal with matrix maths and in particular the maths of camera projections and calibrations and 3D geometry. These are almost always easier to work with when we use homogenous coordinates. The `mathTypes.h` file defines a `typedef` for a 2D and 3D homogeneoud vectors, as well as transformation matrices.

  - `hVec2D`: Homogeneous 2D vector $(x,y,z,w)$
  - `hVec3D`: Homogeneous 2D vector $(x,y,w)$
  - `genMatrix`: General purpose matrix of any size
  - `genRowMajMatrix`: As above, but explicity stating that the in-memory layout of the matrix will be row-major.
  - `transMatrix2D`: 3x3 2D transformation matrix
  - `transMatrix3D`: 4x4 3D transformation matrix
  - `gl44Matrix`: 4x4 matrix with column-major layout as expected by OpenGL
  - `cfMatrix`: General size matrix but for complex floats

Note that our use of homogeneous vectors is mostly quite lazy. We assume that you as a user are keeping track of your vectors and keeping that homogeneous coordinate at 1 or 0 - so if you're expecting things to be elegantly handled when you've got points such as $(5,100,13,7)$ - then think again.

### Misc

`mc_core/src/misc/` contains various tools which find use in the framework or provide useful utility. 

  - `tokeniser.h`: Provides a small tool to take an input string and split it on spaces or other delimiters. Useful when parsing text based data files.
  - `types.h`: Typedefs for miscellaneous types used in the framework.
  - `imgSender.h`: Tool for sending images across the network.

The image sender is a neat pair of classes which can be used to transmit an image over a network socket. It's not designed for hardcore image streaming but can be useful for communication between a few tools. It was created initially so that when live cameras were being calibrated it could, in theory, be possible to send images from the grabber computer to a small satellite computer or phone or ipad to help with focussing (to focus, you need to stand at the camera, but that might be some distance from the computer screen...). Use of the image sender and reciever class is demonstrated in the `mc_core/tests/isender.cpp` and `mc_core/tests/ircv.cpp` test programmes.


### The renderer

The renderer will be documented in more detail in its own chapter.

### Calibration

Details of the calibration algorithms will be given in their own chapter.
