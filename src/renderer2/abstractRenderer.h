#ifndef ABSRENDERER2_H_ME
#define ABSRENDERER2_H_ME

#include "math/mathTypes.h"
#include "renderer2/sceneGraph.h"

#include <string>
#include <vector>
#include <stack>

#include "renderer2/mesh.h"
#include "renderer2/texture.h"
#include "renderer2/renderList.h"

#include "commonConfig/commonConfig.h"


namespace Rendering
{
	//
	// First off, let's make little classes to contain vertex and fragment shaders, and shader programs.
	//
	class VertexShader
	{
	public:
		VertexShader() {}
		VertexShader( std::string source );
		~VertexShader();

		GLuint GetID()
		{
			return shaderID;
		}

	private:
		GLuint shaderID;
	};
	
	class GeomShader
	{
	public:
		GeomShader() {}
		GeomShader( std::string source );
		~GeomShader();

		GLuint GetID()
		{
			return shaderID;
		}

	private:
		GLuint shaderID;
	};
	
	class FragmentShader
	{
	public:
		FragmentShader() {}
		FragmentShader( std::string source );
		~FragmentShader();

		GLuint GetID()
		{
			return shaderID;
		}

	private:
		GLuint shaderID;
	};

	class ShaderProgram
	{
	public:
		ShaderProgram() {}	// shouldn't really allow this!
		ShaderProgram( std::shared_ptr<VertexShader> vs, std::shared_ptr<FragmentShader> fs );
		ShaderProgram( std::shared_ptr<VertexShader> vs, std::shared_ptr<GeomShader> gs, std::shared_ptr<FragmentShader> fs );
		~ShaderProgram();

		GLuint GetID()
		{
			return progID;
		}

	private:
		GLuint progID;
		GLuint vsID;
		GLuint gsID;
		GLuint fsID;
	};
	
	
	
	
	
	
	
	
	
	
	//
	// to be able to use smart pointers, we have a factory method create
	// the actual renderers. 
	//
	class RendererFactory
	{
		public:
		template <class T>
		static void Create(std::shared_ptr<T> &ren, unsigned width, unsigned height, std::string title)
		{
			ren.reset( new T(width, height, title) );
			ren->SetThis( ren );
			ren->FinishConstructor();
		}
	};
	
	
	
	
	
	//
	// Now the abstract renderer, which does no rendering, but forms the base class for our windowed (SFML) or headless (EGL)
	// renderers.
	//
	class AbstractRenderer
	{
		friend class RendererFactory;	
		// The constructor should be private or protected so that we are forced 
		// to use the factory...
	protected:
		// the constructor creates the renderer with a "canvas" of the specified
		// size, and with the specified title (if a title is needed)
		AbstractRenderer(unsigned width, unsigned height, std::string title);
		
		// Because we want to be able to throw around the "this" pointer,
		// but also want to use smart pointers rather than nice easy to use
		// normal pointers (there's some advantages...), object's need to
		// know who they are...
		void SetThis( std::weak_ptr<AbstractRenderer> in_ptr)
		{
			smartThis = in_ptr;
		}
		std::weak_ptr<AbstractRenderer> smartThis;
		
		
	public:
		virtual ~AbstractRenderer();


		// Even loop typically does: render, buffer swap, check user input (if interactive)
		virtual bool StepEventLoop();
		virtual void RunEventLoop();

		// read in shader files
		void LoadVertexShader(std::string filename, std::string shaderName);
		void LoadGeometryShader(std::string filename, std::string shaderName);
		void LoadFragmentShader(std::string filename, std::string shaderName);
		void CreateShaderProgram( std::string vertexShaderName, std::string fragShaderName, std::string progName );
		void CreateShaderProgram( std::string vertexShaderName, std::string geomShaderName, std::string fragShaderName, std::string progName );
		std::shared_ptr<ShaderProgram> GetShaderProg(std::string progName)
		{
			return shaderProgs[progName];
		}

		// make sure this renderer has the active window.
		virtual void SetActive()=0;
		
		virtual void SetInactive()=0;
		
		virtual void SwapBuffers()=0;
		
		unsigned GetWidth()
		{
			return width;
		}
		unsigned GetHeight()
		{
			return height;
		}
		
		std::shared_ptr<Rendering::Texture> GetBlankTexture()
		{
			return blankTexture;
		}

		// A scene group is a scene graph, as well as
		// important details about that scene, such as what the
		// active camera is for the graph, and the renderList that it will use.
		class SceneGroup
		{
		public:
			SceneGroup(AbstractRenderer* renderer) : renderList(renderer)
			{
				rootNode = NULL;
				activeCamera = NULL;
			}
			
			~SceneGroup()
			{
				if( rootNode )
					rootNode->Clean();
				rootNode.reset();
				activeCamera.reset();
			}
			std::shared_ptr<SceneNode> SetRoot( std::shared_ptr<SceneNode> in_root )
			{
				std::shared_ptr<SceneNode> oldRoot = rootNode;
				rootNode = in_root;
				return oldRoot;
			}
			void SetCamera( std::shared_ptr<CameraNode> in_cam )
			{
				// TODO: Check that camera is in this scene graph!
				// TODO: Make sure any other cameras in this scene graph are not active!
				activeCamera = in_cam;
				in_cam->SetActive();
			}

			void Parse()
			{
				renderList.renderActions.clear();
				rootNode->Render( renderList );
			}

			void Render()
			{
				if( enableDepth )
					glEnable(GL_DEPTH_TEST);
				else
					glDisable(GL_DEPTH_TEST);
				renderList.Render(activeCamera);
			}
			
			std::shared_ptr<SceneNode>  rootNode;
			std::shared_ptr<CameraNode> activeCamera;
			bool enableDepth;
			RenderList renderList;
		};
		
		virtual cv::Mat Capture()=0;
		
		
		CommonConfig ccfg;
		
		void SetSceneDepthTestEnabled( int scene, bool isEnabled )
		{
			if( scene >= 0 && scene < scenes.size() )
			{
				scenes[ scene ].enableDepth = isEnabled;
			}
		}
		
	protected:
		
		int width, height;
		
		// some things can't go in the constructor, so each renderer needs to have an initialise. Sucks.
		virtual void FinishConstructor();

		// multiple scene graphs are possible, and the renderer will render them in
		// the order that they appear here. Each scene graph should contain one
		// active camera scene node for rendering with, which need not be the
		// root scene node (indeed, it probably wont be, as that is likely to be
		// a shader node)
		std::vector< SceneGroup > scenes;
		
		// render...
		void Render();
		
		// This parses the scene graph to populate the render list.
		void ParseSceneGraphs();
		RenderList renderList;
		
		
		// read a whole text file into a std::string
		std::string ReadTextFile(std::string filename);
		
		// convenient tool.
		std::shared_ptr<Rendering::Texture>	blankTexture;	// a blank texture, because it is convenient to have one!
		
		// shader management
		std::map < std::string, std::shared_ptr<VertexShader>   > vertexShaders;
		std::map < std::string, std::shared_ptr<GeomShader>     > geomShaders;
		std::map < std::string, std::shared_ptr<FragmentShader> > fragmentShaders;
		std::map < std::string, std::shared_ptr<ShaderProgram>  > shaderProgs;


	};
	
	
	
}

#endif
