## Overview

The Renderer is intended to be a useful but relatively simple tool for debug visualisations and such like - it is not a full blown graphics engine. What it is is a basic scene graph and OpenGL rendering path for interactive or headless 2D and 3D rendering. One of its core benefits vs. importing powerful graphics engines such as Ogre or Panda (or worse, full game engines) is that we can define camera matrices that replicate our camera calibration. That, indeed, is a great thing. It also have major advantages over the OpenCV tools for showing images (imho) - because it's not really all that hard to be better than those.

## Backends

There are two backends and ultimately, two inheritence trees for the renderer. One option is to use SFML, which will create windows with OpenGL contexts and allow nice mouse and keyboard interactivity. The second option is to use EGL as a backend, so that we can do *Headless* OpenGL rendering - meaning that there is no window to render onto, and no need even for a window manager, which is great when you have jobs that need to be done remotely on powerful servers. In this case, EGL creates a rendering context and the resulting images can be saved to disk.

## Code structure.

At the base of things is the `abstractRenderer.h` file, which contains declarations for classes that handle key OpenGL concepts:

  - `VertexShader`
  - `GeomShader`
  - `FragmentShader`
  - `ShaderProgram`

We then have the `RendererFactory` class, which is a singleton with sole purpose of a method that creates renderers. All renderer objects will be created by the `RendererFactory`.

After this, we have an abstract base class for the renderers themselves, which defines the basic interface for any renderer. From this we have two base class renderers, one for the headless EGL family of renderers, and one for the SFML based renderers - `BaseRenderer` and `BaseHeadlessRenderer`.

Various tools throughout the framework may define their own specific renderer derived from one of these base classes, but in many cases, all you will need to use are one of the provided basic renderers:

  - `BasicRenderer`
  - `BasicPauseRenderer`
  - `BasicHeadlessRenderer`

## Example using a BasicRenderer

Say you are processing some data. You have some code already for reading the data from a file `ReadData` and you have some code that renders some debugging information onto an image, `RenderDebug`. Now, as you process each frame of that data, you want to just render that debug image onto the window. The `BasicRenderer` will do exactly what you want in this case.

First, we need to create a renderer:

```cpp
	std::shared_ptr<Rendering::BasicRenderer> ren;
	Rendering::RendererFactory::Create( ren, 
	                                    windowWidth, 
	                                    windowHeight, 
	                                    "Debug Renderer" );
```

It is vital that the renderer has a smart pointer to itself. But we can't create that pointer in the renderer's own constructor. That is why we force you to use a factory to create renderers, because the factory knows to call an initialise method of the renderer after creating the renderer that ensures the renderer's `smartThis` member is initialised.

All you need to know to create a renderer though is the size of window you want, and what you want to title the window with.

The easiest way to show a debug image on the renderer's window is to set it as the background image:

```cpp
	ren->SetBGImage( yourDebugImage );
```

And to actual update the display and progress the event loop of the window, you need to call:

```cpp
	ren->StepEventLoop();
```

So, your full programme might just look like this:

```cpp
int main(void)
{
	data = ReadData()
	
	std::shared_ptr<Rendering::BasicRenderer> ren;
	Rendering::RendererFactory::Create( ren, 
	                                    data.windowWidth, 
	                                    data.windowHeight, 
	                                    "Debug Renderer" );
	
	for( unsigned fc = 0; fc < data.frames.size(); ++f )
	{
		cv::Mat debugImage = RenderDebug( data );
		
		ren->SetBGImage( debugImage );
		ren->StepEventLoop();
	}
}
```

### But, what if you actually need headless rendering?

In this case, because things are so simple, all you would need to do is swap out to the headless renderer instead, and then capture the renderer's output and save it to file:

```cpp
int main(void)
{
	data = ReadData()
	
	std::shared_ptr<Rendering::BasicHeadlessRenderer> ren;
	Rendering::RendererFactory::Create( ren, 
	                                    data.windowWidth, 
	                                    data.windowHeight, 
	                                    "Debug Renderer" );
	
	for( unsigned fc = 0; fc < data.frames.size(); ++f )
	{
		cv::Mat debugImage = RenderDebug( data );
		
		ren->SetBGImage( debugImage );
		ren->StepEventLoop();
		
		cv::Mat grab = ren->Capture();
		std::stringstream ss;
		ss << "results/" << std::setw(6) << std::setfill('0') << fc << ".jpg";
		SaveImage( grab, ss.str() );
	}
}
```

### What if you want to pause to look at a render?

In this case you might find use in the `BasicPauseRenderer`. You would only need quite mild changes to your code to be able to pause and unpause processing using the space-bar, or to advance a single frame using the up arrow.


```cpp
int main(void)
{
	data = ReadData()
	
	std::shared_ptr<Rendering::BasicPauseRenderer> ren;
	Rendering::RendererFactory::Create( ren, 
	                                    data.windowWidth, 
	                                    data.windowHeight, 
	                                    "Debug Renderer" );
	
	bool paused = false;
	bool advance = false;
	for( unsigned fc = 0; fc < data.frames.size(); ++f )
	{
		cv::Mat debugImage = RenderDebug( data );
		
		ren->SetBGImage( debugImage );
		
		do
		{
			ren->Step(paused, advance);
		}
		while(paused && !advance)
		
		advance = false;
	}
}
```

## Scenes

Every renderer has a set of `Scenes` that it will step through in order and render onto the output. The various `BasicRenderers` have three scenes, which are *intended* to be used as:

  - 2D background
  - 3D
  - 2D foreground

But that is actually merely the names given and you can do what you want. You can also create your own custom renderer inheriting from one of the base renderers and create as many scenes as you need. The renderer basically has a `std::vector` of scenes and will render them in order starting from 0. This means that you can render one scene on top of another scene, which is often useful.

Scenes are defined as `SceneGroup` structures inside the `AbstractRenderer`, and consist of a scene graph and ancillary pieces of information such as the camera used to render that scene.

The scene graph is a tree and each node of that tree is either a thing to render or a transformation of somesort. We can consider a basic rendering task to demonstrate simple use of a scene graph.

Consider a situation like above, only we want to compare two different processes on our data. So now, instead of `RenderDebug` we have `RenderProcess0` and `RenderProcess1`, each producing their own debug image, and those images are square.

Again, create our renderer:

```cpp
	std::shared_ptr<Rendering::BasicRenderer> ren;
	Rendering::RendererFactory::Create( ren, 
	                                    windowWidth, 
	                                    windowHeight, 
	                                    "Debug Renderer" );
```

Now we want to setup the background scene. First, we need our camera to see the two output images. We have two images, which we'll put side by side, and we thus need an orthographic camera that can see 2 units by 1 unit:

```cpp
	ren->Get2dBgCamera()->SetOrthoProjection(0,2,0,1,-100,100);
```

Note that because we're using the `BasicRenderer` we have nice little shortcut methods such as `Get2dBgCamera`. If you create your own renderer, you will need to make your own such shortcuts, or access a scene directly. But more on that later.

Now you need to create somewhere to draw your two images. We do this using rendering nodes. Because we're living in 3D rendering land, life is more complicated than just saying "put the image on the background" - but it is still not hard and we've done our best with this renderer to make easy easy and facilitate hard (unlike modern OpenGL, which facilitates very very hard but makes easy a pain in the arse). Anyway, what we're going to do is create a two-triangle square mesh called a card and texture it would our image. So we'll put a `MeshNode` on our scene graph for each image, and we'll put them right next to each other in the window.

```cpp
	// create the mesh for the card. We can use a utility function from
	// geomTools.h header.
	auto cardMesh = GenerateCard(1,1,false);
	
	// and we need to give the geometry data to the renderer
	cardMesh->UploadToRenderer(ren);
	
	// now we need to create two MeshNodes to go on our scene graph
	std::shared_ptr<Rendering::MeshNode> n0,n1;
	Rendering::NodeFactory::Create(n0, "process 0 node");
	Rendering::NodeFactory::Create(n1, "process 1 node");
	
	// now initialise the nodes
	// we can start off by giving the nodes blank textures
	// and the BasicRenderer has a shortcut for that
	n0->SetShader( ren->GetShader("basicShader") );
	n0->SetTexture( ren->GetBlankTexture() );
	n0->SetMesh( cardMesh );
	
	n1->SetShader( ren->GetShader("basicShader") );
	n1->SetTexture( ren->GetBlankTexture() );
	n1->SetMesh( cardMesh );
	
	// We need to say where each node is.
	// The vertices of the card mesh are (0,0), (1,0), (1,1), (0,1)
	// so to put n0 is already in the right place, 
	// but n1 needs to be shifted 1 unit to the right:
	transMatrix t0 = transMatrix::SetIdentity();
	transMatrix t1 = transMatrix::SetIdentity();
	t1(3) = 1.0; 
	
	n0->SetTransformation(t0);
	n1->SetTransformation(t1);
	
	// Now we can put these two nodes on our background scene graph
	ren->Get2dBgRoot()->AddChild(n0);
	ren->Get2dBgRoot()->AddChild(n1);
```

Now when rendering, instead of calling the shortcut to set the background image, we instead upload new images to the textures of each node:

```cpp
	for( unsigned fc = 0; fc < data.frames.size(); ++f )
	{
		cv::Mat img0 = RenderProcess0( data );
		cv::Mat img1 = RenderProcess1( data );
		
		n0->GetTexture()->UploadImage( img0 );
		n1->GetTexture()->UploadImage( img1 );
		
		ren->StepEventLoop();
	}
```

Believe it or not, you've now learned most of what you will need to know to render a wide variety of things. You define the 3D mesh, create a node for it, and give it a texture or some colour, and put that node on the scene graph.

### Scene nodes

There are a few types of nodes that we can put on the scene graph. You can find them all in the `mc_core/src/renderer2/sceneGraph.h` header. Right now, there are 3:

  - SceneNode: Applies a transformation matrix to all children.
  - CameraNode: Allows you to define a camera on the graph, should you wish to anchor a camera to some other object, for example. Note that every scene has a default camera already and if you want to add a camera node and use it instead of the default, you will have to change the scene camera.
  - MeshNode: A node that contains geometry to render. You can use the node to apply a transformation matrix to set the position of the geometry relative to its parent node, as well as setting the texture of the mesh, or the base colour of the mesh.


## Cameras

We provide two projection models for your cameras while rendering.

  - orthographic: great for 2D stuff
  - perspective: for 3D stuff

One of the key things we wanted to have with the framework's renderer was a way to render using the same camera model we use for all the computer-vision work and which we calibrate using all of our calibration tools.

It is not entirely trivial, but it is possible to define a projection matrix for OpenGL that is equivalent to the linear part of the camera's projection - we don't as yet handle the distortion parameters but if you have an image from a camera and want to render 3D ontop of that image, you would undistort your camera image, then project the 3D scene.

Anyway, that's not key right now. What is key is how you set the camera you want to render with.

```c++
	ren->Get2dBgCamera()->SetOrthoProjection(left,right,top,bottom,near,far);
	ren->Get3dCamera()->SetFromCalibration( calib, near, far );
```

Obviously, to use a perspective camera in this way you will first have to define a calibration. But this is actually simple.


```c++
	Calibration calib;
	calib.K << 800, 0, 400,
	           0, 800, 400,
	           0,   0,   1;
	hVec3D eye, target, up;
	eye << 0, 1000, 0, 1;
	target << 0,0,0,1;
	up  << 0, 1, 0, 0;
	calib.L = LookAt( eye, target, up );
```

You will find that if you're in any way used to dealing with parameterising a camera in terms of computer-vision-style calibration then it is fairly intuitive to define a camera view in that way. Basically, setting the focal length to be about the same as the camera resolution is a fairly natural field of view. Longer gets more zoomed in, smaller is wider.

The `LookAt` function is declared in the `mc_core/src/math/matrixGenerators.h` header file and allows you to place the camera at `eye`, look towards `target` and have an "up" direction of `up`. 


When defining a camera for rendering you will need to supply the `near` and `far` clipping planes. You are free to do as you will here, but keep in mind that the depth buffer has finite resolution so if you span too large a range in depth you may suffer the odd issue but don't let it stress you out.

When doing orthographic projection it may seem odd but that depth dimension still exists and can be used you just wont see any perspective effects from using the depth.

## Text rendering

The renderer has the facility to do text rendering. It is not beautiful, but it is functional. Text rendering is done through the `SDFText` class from `mc_core/renderer2/sdfText.h` - where SDF means Signed Distance Field, more on that in a second.

To render text, first create an `SDFText` object, passing it the path of a true-type font file and the renderer you want to use it with:

```cpp
Rendering::SDFText textMaker( fontFile, ren);
```

`mc_core` is supplied with a defualt font from Google and you can find that in the core data path, making use of the `CommonConfig` class.

```cpp
	CommonConfig ccfg;
	std::stringstream fntss;
	fntss << ccfg.coreDataRoot << "/NotoMono-Regular.ttf";
	std::string fontFile = fntss.str();
```

You will need a basic `SceneNode` to serve as the root of the text you want to render.

```cpp
	std::shared_ptr<Rendering::SceneNode> textRoot;
	Rendering::NodeFactory::Create( textRoot, "textRoot" );
```

You can then simple ask the `SDFText` object to create the renderable object for you using one of the methods:

```cpp
	void RenderString( std::string text, float height, float r, float g, float b, std::shared_ptr<Rendering::SceneNode> sn );
	void RenderString( std::string text, float height, hVec3D pos, Eigen::Vector4f colour, std::shared_ptr<Rendering::SceneNode> sn );
	void RenderString( std::string text, float height, hVec2D pos, Eigen::Vector4f colour, std::shared_ptr<Rendering::SceneNode> sn );
```

Put your renderable nodes somewhere on your scene graph and (assuming you've got your position and size right) you'll have text on your render.

### How text rendering works

To my understanding, this technique to rendering text was developed by Valve for Halflife. You start off by loading a font file and rendering a map image - which is to say, a single image where you know the location of each letter at the size you think will give you the detail that you need.

If you wanted, you could then use this image to render strings - you just make suitable cards to put each letter on and texture them with the right letter. In practice this doesn't scale well.

So what was done was to take a signed distance field of the map image - if you rendered the font as black on white originally, then the sdf image simple says, for each white pixel, how far it is from its nearest black pixel, and for black pixels, the distance to the nearest white - except inside the letter is negative and outside is positive.

When you now use this sdf to texture the cards that you've put together to make words or sentences, you do so using a fragment shader that understands how to use the sdf and get nice letters that scale well. I shall leave the full details to your Google-Fu.

## Creating a custom renderer

In most cases, you will simply inherit from the relevant `BasicRenderer` and then create your own `Step` method which *must* then call the renderer's `Render` method. For an SFML based window you will also need to handle the SFML event loop, in which case you will want to have a look athe the `StepEventLoop()` method of the `BasicRenderer` or the `Step()` method of the `BasicPauseRenderer`.

Sometimes you will want to inherit directly from one of the `Base` renderers, in which case you will need to define your scenes as well as your `Step()` function. You create scenes using the `InitialiseGraphs()` method and for now, the documentation you need for this is to have a look in the source code for the `BasicRenderer`.

Basically though, you resize your `scenes` member, and then give each scene a root node, a camera, and decide if you want depth testing. You may find it useful to then provide shortcuts to each scene's root node or camera - as `scenes` is a protected member of the `AbstractRenderer`.

You will also have to go about loading up all the shaders that you think you will need to use with your renderer - against, consult the source of the `BasicRenderer` to get more of a clue how this all works.

## A little bit more detail

You'll have noticed by now that updating the renderer for each frame of output involves calling a method variously called `Step()` or `StepEventLoop()`. It can really be any method, but the key point is that this update function calls the `Render()` method, and any event handling (for SFML).

The `Render()` method is responsible for traversing each scene graph and building a list of things to render. Each scene node class must have its own `Render()` method, which takes in the current render list and state and adds the render jobs of the node. So, for example, a scene node renders its children and applies its own transformation matrix. A Mesh node render's its children, and adds its own "render a mesh" `RenderAction` to the `RenderList`. Finally, the `RenderList` is parsed where individual `RenderActions` get performed - which is where the OpenGL calls all happen.

If you need this level of understanding on the renderer's internals, I'm sorry to say, you'll have to consult the code itself!
