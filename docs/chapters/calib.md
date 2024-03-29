## What is calibration?

Camera calibration is the process by which the location and imaging properties of a camera are determined. These properties are usually considered as two distinct things:

  - *intrinsic* parameters
  - *extrinsic* parameters

Typically, a camera is represented by some mathematical model such that given some point $p$ in space, we can define some function that projects it into the image $\hat{p} = f(p)$

The *intrinsic* parameters affect the projection model - they represent things like the focal length and distortion of the camera's lens, whether there is any skew or sheer or non-squareness to the camera sensor's pixels, where on the image centre the optical axis of the lens system intersects, etc...

The *extrinsic* parameters define the camera's position in space relative to some defined world coordinate system. This is represented by a homogeneous 3D transformation matrix which I've come to refer to as $L$, such that for a point $p_w$ defined in world coordinates, we can use $L$ to transform the point to be relative to the camera using $p_c = Lp_w$

## Projection model

`mc_dev` uses a fairly standard pinhole projection model the consists of a 3x3 camera matrix $K$ and a set of lens distortion parameters.

$$ K = \left[ \begin{array}{ccc} f & s & c_x \\ 0 & af & c_y \\ 0 & 0 & 1 \end{array} \right] $$

In $K$ we have the following parameters:

  - $f$: focal length
  - $s$: sheer, normally assumed to be 0
  - $(c_x, c_y)$: principle point - where the optical axis intersects the image plane
  - $a$: aspect ratio, normally assumed to be 1

Note that the focal length will be defined in *pixels*, not in mm or cm or otherwise. You can convert it if you really really wanted to, but if you need to do that you're going beyond the scope of this document.

The distortion model consists of 5 parameters provided as the array $k = [\begin{array}{ccccc} k_0 & k_1 & k_2 & k_3 & k_4 \end{array} ]$ for a radial and tangential distortion as per [OpenCV](https://docs.opencv.org/4.5.3/dc/dbb/tutorial_py_calibration.html) - our $k_0$ to $k_4$ are in the same order and OpenCV's `(k1,k2,p1,p2,k3)`.

## Calibration target

The calibration tool is designed to work with a circle-grid planar calibration target. The following image was used for creating the default board used for CAMERA.

<div style="text-align: center">
![default grid](imgs/diCalibrationTargetV4_10x10_75mm.png){style="width: 90%; margin: auto;"}
</div>

Note however that we normally blank-out the top row of that grid so that we have a 9x10 grid with the extra 2 alignment dots on the bottom, rather than a 10 by 10 grid. When printed at full size, this grid has a calibrated spacing of 75 mm between circle centres - but in reality you'll want to measure that yourself.

When you get ideal lighting and relatively close observations of the calibration board, then the alignment dots are very visible and can easily be detected and used by the calibration tool. Often, lighting or scale will make the alignment dots difficult to identify, and as such, it is advantageous to have a non-square grid to help with identifying board alignment.

## Calibrating a network of cameras

The calibration process is typically as follows:

  1) Capture images
  2) create calibration configuration file
  2) detect grids
  3) manually annotate any extra matches
  4) run calibration
  5) align calibration with desired origin

### Capturing images

The exact approach will of course depend on your camera system. You will want to ensure that the cameras are synchronised (or that you have the ability to synchronise them later).

Once your cameras are set up, you will want to identify the point in the scene where you want to place your scene origin, and identify what your x and y axes will be. You will want to make sure that there are lines or point in your scene that are visible from multiple cameras to identify:

  1) The desired scene origin.
  2) Any point on the x-axis of the scene
  3) Any point on the y-axis of the scene.

To maximise the chance of your ground plane being flat, you will want to make your x and y axis points as far away from the origin point as is reasonable given your scene geometry.

Set all the cameras recording, and then walk the calibration board around the scene. Your have three basic aims:

  1) Ensure that observations of the board fill as much of each camera view as possible - helps resolve intrinsics parameters
  2) Ensure that for every camera, you maximise the number of other cameras it has shared board observations with - helps ensure proper extrinsic solve.
  3) Fill the observation area with board observations - helps ensure maximum accuracy throughout the whole of the area you want to observe.

<div style="text-align: center">
![Filling the view](imgs/calib00.gif){style="width: 45%; margin: auto;"}
![Traversing the scene](imgs/calib01.gif){style="width: 45%; margin: auto;"}
</div>
  
If calibrating a small-scale room, such as the _BioCV_ dataset, or a dataset in the CAMERA studio, where you can get relatively close to the cameras, then we typically proceed by:

  1) showing the board to each camera in turn. During this stage, we present the board as close as focus will allow, and shift it gently so that it reaches into the corners of the camera view. We then increase distance, and repeat.
  2) Walking through the scene thinking in terms of pairs or triplets or larger groups of cameras, and trying to present the board so that it is wholly visible in multiple views at one time.

When you look at the network of cameras you have created, it is key to consider how each camera is connected to each other camera. If you think of a board position seen by camera `a` and `b`, then you can think of that as connecting camera `a` and `b`. You want to maximise the number of cameras that `a` can directly connect to, but you want to ensure that there is some path, direct or indirect, connecting camera `a` to all other cameras.

Get this stage right, and the following stages will be trivial.

If you need to, it should be possible to do an intrinsic calibration of each camera on its own and later do the extrinsic.

### Configuration file for calibration tools

The configuration file for all of the calibration tools is generally the same file, though not all options are used by all tools. The configuration file uses the format defined by `libconfig`.

As with almost every config file used by `mc_dev`, the config file starts by specifying the `dataRoot` and the `testRoot`. If you are not already familiar with `mc_dev`'s splitting up of data paths, the idea is that the `dataRoot` might change between different computers, but the `testRoot` will stay the same - so you can use the same configuration file across different computers. Note that this means `dataRoot` is optional as if you don't specify it, it will be taken from your `~/.mc_dev.common.cfg` file.

```bash
dataRoot = "/datasets/"
testRoot = "2021-10-31-ghostCapture/"
```

The next thing you need to supply is the location of each camera's image source (be it a directory of images, or a video), relative to the `testRoot`.

```bash
imgDirs = (
            "00.mp4", "01.mp4", "02.mp4", "03.mp4"
          );
```

You will also need to specify details about the grid. This includes the spacing between dots on the row and column axes, whether the grid is light-dots-on-dark-background and whether to try and detect the alignment dots or not. The `useHypothesis` option allows the system to accept incomplete grids and to hypothesise about where missing circles would be. Mostly you will not want to enable this.

```bash
grid:
{
	rows = 9;
	cols = 10;
	rspacing = 78.5;
	cspacing = 78.5;
	isLightOnDark = false;
	useHypothesis = false;
	hasAlignmentDots = false;
}
```

These next settings are used by the main `circleGridCamNetwork` tool:

```bash
#
# Controls whether we need to perform detection of the grids in the images.
# Grid detections are stored in a "grids" file in the image source directories.
#
useExistingGrids = true;

# the initial calibration is done for the intrinsics
# using a number of the calibration boards.
# set this to 0 to use all boards (default),
# or speed things up significantly by only calibrating
# with some maximum number.
maxGridsForInitial = 90;

#
# Initial calibration is done per-camera to get camera intrinsics.
# Technically, distortion is an intrinsic and it can be calculated quite
# well at this early stage of processing - if you have enough grids
# turned on for initial calibration.
# In practice however, the calibration works better overall if 
# distortion parameters come out of the bundle adjustment stages.
# As such, this should normally be set to true unless you are
# having problems.
#
noDistortionOnInitial = true;

#
# If we have them, then we can use existing intrinsics.
# Probably keep this as false except in exceptional circumstances
#
useExistingIntrinsics = false;

#
# Specify the number of distortion and intrinsic parameters to solve for.
#
# The intrinsics can be:
# 1: just focal length
# 3: focal length and principle point
# 4: as 3 plus aspect ratio
# 5: as 4 plus skew
#
# 3 is normally a good option.
#
#
# the distortion parameters are r^2, r^4, tx, ty, r^6
#
# 2 (r^2, r^4) is generally good enough.
#
# If you have problems, then solving for fewer parameters is generally a
# good place to start.
#
numDistortionToSolve = 2;
numIntrinsicsToSolve = 3;

#
# Calibration works by picking the camera that sees the most grids,
# reconstructing those grids in 3D, and then using those reconstructions
# to estimate the positions of some number of new cameras.
# 
# Once those new cameras are known, they will allow for more grids to
# be estimated, and thus, for more cameras to be added etc.
#
# This parameter forces the solver to only ever add one new camera
# after each stage of estimating new grids.
#
# It leads to more stages of processing, but is generally more reliable.
#
# But not _always_ more reliable.
#
forceOneCam = true;

#
# Similarly, if we are allowing multiple cameras to be added, then
# this specifies the minimum number of grid observations shared between
# cameras for a new camera to be added. Note that all cameras will eventually
# be added - it will then just be one at a time if there's not enough shares.
#
# If you really struggled to get shared grids for a camera, you can set this 
# to 0. Then the tool will allow the camera to be added to the system using
# only auxilliary point matches.
#
# Note too that if you set this high enough, cameras that don't have enough grids 
# but which do have auxMatches, will try to be calibrated from the auxMatches - thus 
# you can force a camera to solve from auxMatches.
#
minSharedGrids = 80;


#
# When calibrating, the algorithm will pick the camera with the most shared views 
# as the "rootCam" - i.e the camera that is initialised as the origin and to which
# all other cameras as built around.
#
# Sometimes, that can be a bad choice and the network wont fully solve. As such, you
# can force a difference root camera. Set to a large number of comment out for 
# default behaviour, or the index of the camera to try.
#
rootCam = 99999

#
# Bundle adjustment is the heavy processing part of the calibration
# process, and is what ultimately results in a high quality calibration.
# It can however be useful to try running without it sometimes for debugging
# or other purposes.
#
useSBA = true;
SBAVerbosity = 3;


# how much visualisation?:
# 0 : none (default)
# 1 : show visualisation before bundle and final
# 2 : show visualisation after  bundle and final
# 3 : show visualisation before and after bundle and final
# >0 : show visualisation of final result
visualise = 0;

#
# Sometimes it can be useful/necessary to augment the grid observations with
# a set of manual point matches between the camera views.
#
# This specifies the file that contains the matches.
#
# Leave it commented out if you are not using manual point matches.
#
# Manual point matches can be created using the pointMatcher tool
#
matchesFile = "matches"
```


Next we have settings specific for the `pointMatcher` tool. Note that the frames specified here are the frames that will be shown when the tool opens. The tool has the ability to change the frame if you need.

```bash
#
# These, meanwhile, are options that configure the pointMatcher tool for
# finding those manual matches. It controls the window size of the tool,
# and which frame number is used for each camera view.
#
frameInds   = (1210,1210,1210,1210,1210,1210,1210,1210,1210)
winX = 1600
winY = 1000
```

And then we have some settings for the alignment tool:

```bash
# the origin-target might not be flush with
# the desired ground plane, so this
# specifies the depth of it. We can then
# apply a -depth shift on z once
# the initial alignment is complete
targetDepth = 0.0

# instead of the interactive and sub-optimal stage of the manual alignment tool,
# we can use some of the matches from the matches file.
# points in order: point on x-axis, origin point, point on y-axis
axisPoints      = (0,1,2)

#
# Sometimes it is easier to click on a negative x or y point than a positive one
# when setting the scene orientation.
#
alignXisNegative = false;
alignYisNegative = false;
```





### Detect grids 

Grid detection is performed by the main calibration tool.

We recommend that you comment out the matches file and disable bundle adjustment for the initial grid detection run. You must also *enable* grid detection:

```bash
# matchesFile = "matches"
useSBA = false;

# enable grid detection
useExistingGrids = false
```

You can now start the tool (this assumes that you are in the `mc_dev` root directory, but you can be anywhere):
```bash
 $ ./mc_core/build/optimised/bin/circleGridCamNetwork network.cfg
```

The tool will run, find grids, and will do an initial calibration without bundle adjustment. Detecting grids can take a long time.

The default grid detector is based on finding `MSER` features in the image, then grouping them together to identify sets of features that are consistent with a grid of the specified number of rows and columns. `MSER` features are really good for finding light or dark circles against dark or light backgrounds over multiple scales and in the presence of varying lighting. But as a feature detector, it is not very fast.

You may also find that some default values for the `MSER` detector are not perfect for your data - for example, maybe your board has really large circles, or is a long way away and has really small circles in the image. In which case, you can adjust the grid detector's parameters by adding the following section to your calibration config file:

```bash
gridFinder:
{
	visualise = false;
	MSER_delta = 3;
	MSER_minArea = 50;
	MSER_maxArea = 40000;
	MSER_maxVariation = 1.0;
	blobDetector = "MSER";
	potentialLinesNumNearest = 8;
	parallelLineAngleThresh = 5.0;
	parallelLineLengthRatioThresh = 0.7;
	gridLinesParallelThresh = 25.0;
	gridLinesPerpendicularThresh = 60.0;
	gapThresh = 0.8;
	 = 25.0;
	alignDotDistanceThresh = 10.0;
	maxGridlineError = 32.0;
};
```

For `MSER` we found the implementation from [IDIAP](https://github.com/idiap/mser) to be faster than OpenCV and shamelessly cloned their code into `mc_core`. `MSER` is a natural approach to locating the grid circles because it just looks for dark blobs surrounded by light regions (or light blobs surrounded by dark) and is a bit more robust than circle detectors. The `MSER` parameters are best understood with reference to the paper but in general, reducing `maxVariation` or increasing `delta` will reduce the number of features that are detected. The area options limit the size of detected features and is probably the first thing to consider if you need to tune the detector.

Grid detection works by first detecting MSER features, then drawing a line from each feature to its `potentialLinesNumNearest` nearest neighbours. Each of those lines _could_ represent a section of a grid line, but which lines really do? We filter out some features by checking that each line `a` has at least one other line to which it is parallel and which is about the same length (with leeway provided by the `parallelLineAngleThresh` and `parallelLineLengthRatioThresh`) when considering just the lines `b` involving the two features of line `a`. As such, those thesholds can be adjusted to allow more lines through and detect more grids, but increase the chance of detecting bad grids. Next, we observe that true grid lines will pass through multiple features. Indeed, true grid lines should pass very close to atleast `n = min(numRows,numCols)` features. So, we rate each line by the total distance to `n` closest features and sort the lines from best to worst. We then take the best line and start collecting lines that are parallel to that line ( with `gridLinesParallelThresh` giving some (lots of!) leeway ) until the lines exceed `maxGridlineError`. Then we go back to the start and look for the first line that is perpendicular (angle larger than `gridLinesPerpendicularThresh`) and hunt down all lines parallel to that line. We now have two sets of parallel(ish) lines with each set perpendicular(ish) to the other. We filter those sets to get the subset with most consistent gaps between them (`gapThresh` says how close a gap must be to get into a subset) and then try to figure out which set of lines is the rows of the grid and which the columns based on how many lines are left in each set. If the grid is expected to have alignment dots, we predict where those could be and look for suitable features that are within the right size and distance of the prediction (`alignDotSizeDiffThresh`, `alignDotDistanceThresh`) and use that to determine the orientation of the grid, otherwise we do it heuristically with the expectation that 'down' is towards the bottom of the screen and the grid (0,0) is its top left circle - it really helps to not have a square grid for this heuristic. 

It's a brutally heuristic algorithm but remarkably effective and rarely is it worth tuning the line parameters unless you really need to get some perspective-oblique boards detected). 



### Augment grids with points matches

For some camera arrangements, getting the grids will be enough. But for many others, it can be vital to augment the grid observations with a set of manually annotated point matches between various camera views (this is a compromise - it is easier in practice to faff around with some manual annotation once than it is to keep fighting with automatic feature detection and matching tools and having to keep filterig or fixing those matches).

To use the point matcher, make sure that you uncomment the `matchesFile` setting in your config file, then run the tool:

```bash
  $ ./mc_core/build/optimised/bin/pointMatcher network.cfg
```

This will bring up a single window containing the views of 2 cameras. There are some mouse controls which you can see on screen, but you will find these keyboard shortcuts valuable as well:

  - <kbd>a</kbd>, <kbd>s</kbd>    : change camera in left view
  - <kbd>z</kbd>, <kbd>x</kbd>    : change camera in right view
  - <kbd>up</kbd>,<kbd>down</kbd> : change current point id
  - <kbd>[</kbd>, <kbd>]</kbd>    : change frame backward, forward. Hold <kbd>shift</kbd> for larger jump.

To use the point matcher, simply cycle to the view you are interested in, set a point ID, and then click on the image. This will then bring up a blow-up view around the region of the image where you clicked. Click again on this blow-up view to set a sub-pixel accurate point annotation.

As a minimum, you will need to annotate the desired world origin, as well as your point on the desired x axis and point on the desired y axis. Usually these will be points on the positive x-axis and y-axis, but you can annotate points on the negative axes so long as you remember to set the right configuration flag later (see cofiguration documentation above).

Advice for adding extra points for matching is to fill in spaces where the grid couldn't reach, like place high in the background. But the highest priority is to find matches within the volume of the space you are trying to make measurements. That can be quite a hard thing to do - you are after all trying to annotate easily identifiable points in different views (corners of desks, the end of a nose). You also want these points to be visible in as many cameras as possible.

One option that can work well in scenarios where the cameras are in a circle around the observed area and thus where 50% or more of the cameras will see the un-patterned back side of the calibration board, is to annotated the corners of the calibration board, and these are often visible in *all* (or nearly all) cameras.

<div style="text-align: center">
![Using the point matcher](imgs/calib03.jpg){style="width: 90%; margin: auto;"}
</div>

### Calibrate

You are now ready for a proper calibration. Set:

```bash
useExistingGrids = true;
useSBA = true;
```

in the configuration file, and run the calibration tool:

```bash
 $ ./mc_core/build/optimised/bin/circleGridCamNetwork network.cfg
```

When calibration completes, you are looking for the final calibration errors to contain sub-pixel mean errors, and for max errors to be well controlled at only a few pixels. Larger errors will imply poor calibration, bad annotations, and bad grids - grids that have been detected in the wrong orientation can be a particularly pernicious annoyance.

### Align to desired scene origin and orientation.

Hopefully, you have already annotated your desired scene origin, as well as points on your desired x and y axes. Take a note of the point IDs you gave those annotations, and set the relevant configuration parameters (see above for comments detailing the meaning of these settings):

```bash
targetDepth = 0.0

axisPoints      = (0,1,2)

alignXisNegative = false;
alignYisNegative = false;
```

Now you just need to run the alignment tool:

```bash
 $ ./mc_core/build/optimised/bin/manualAlignNetwork network.cfg
```



### Check the calibration.

If you run:

```bash
 $ ./mc_core/build/optimised/bin/calibCheck network.cfg
```

You will be presented with a window showing the camera views. Cycle through these using the left and right arrow keys. You should see that there are depictions of the cameras drawn over the actual camera positions, as well as a depiction of the scene origin in the correct place with the correct orientation (red=x,green=y,blue=z). If you have annotated a lot of point matches, you should also see those- both as the annotation and as a projection of the reconstruction.


You are looking for tell-tales of bad calibrations:

  - extreme distortions of the images
  - re-projected point matches misaligned with the annotations
  - bad positioning or orientation of the scene origin/axes
  - bad positions of cameras relative to the real camera.

If a calibration is poor you will need to

  1) Add more manual point matches
  2) identify bad point matches (annotation mistakes)
  3) identify bad grid detections (e.g. mis-detected orientation)
  4) identify gaps in the connections of your grid-sharing.

<div style="text-align: center">
![Example calibration result. This takes great advantage of all the many features on the floor.](imgs/calib02.jpg){style="width: 90%; margin: auto;"}
</div>


### Check the ground plane

Another tool you might find value in is the `makeGroundPlaneImage`. This projects the camera views onto the $z=0$ ground plane and can be extremely useful for ensuring that you have a good ground plane calibration. The key thing to look for is that you see features on the ground plane (such as lane marking on a running track, court lines on a tennis court, tracks on a push-track, random leaves on some random outdoor scene...) lining up in the same place on the produced image. Strong linear features are particularly good for this, and can identify where individual cameras are inducing strange distortions, or are misaligned - often visible because a straight line becomes curved, or you see multiples of the line being not-quite-parallel.

Note that misalignment of features on the ground plane image can mean several things: it could be that one or more cameras have a poor calibration, or it could mean that you have a bad ground-plane alignment and need to repeat your annotation of desired origin, desires x-axis and desired y-axis.

The tool is run with:

```bash
  $ ./mc_core/build/optimised/bin/makeGroundPlaneImage <config> <minx> <miny> <world size> <image size>
```
For example, this makes a 15 m by 15 m ground plane image in an 800 x 800 pixel image:

```bash
  $ ./mc_core/build/optimised/bin/makeGroundPlaneImage network-00.cfg -5000 -1000 15000 800
```

<div style="text-align: center">
![Projecting images to the ground plane.](imgs/calib04.jpg){style="width: 90%; margin: auto;"}
</div>


If you're struggling to understand the output image, consider just rendering one camera. For instance, the above image is constructed from 9 cameras, which could be individually projected as:

<div style="text-align: center">
![ground 00](imgs/calib05_00.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_01.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_02.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_03.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_04.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_05.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_06.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_07.jpg){style="width: 30%; margin: auto;"}
![ground 00](imgs/calib05_08.jpg){style="width: 30%; margin: auto;"}
</div>

The fact that all these images merge together and give a clear image of the running track's lane lines gives us good confidence in our calibration and ground plane estimates.
