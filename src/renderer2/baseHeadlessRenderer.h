#ifndef BHEADLESSRENDERER2_H_ME
#define BHEADLESSRENDERER2_H_ME


#include "renderer2/abstractRenderer.h"
#include "math/mathTypes.h"
#include "renderer2/sceneGraph.h"

#include <string>
#include <vector>
#include <stack>

#include "renderer2/mesh.h"
#include "renderer2/texture.h"
#include "renderer2/renderList.h"

#include "commonConfig/commonConfig.h"

#ifdef USE_EGL
	#include <EGL/egl.h>
	#include <EGL/eglext.h>
	#ifdef Success
		#undef Success
	#ifdef Status
		#undef Status
	#endif
	#ifdef Complex
		#undef Complex
	#endif
	#ifdef None
		#undef None
	#endif
	#endif
#endif

namespace Rendering
{
	
	class BaseHeadlessRenderer : public AbstractRenderer
	{
		friend class RendererFactory;
		template<class T0, class T1 > friend class RenWrapper;
		
		// The constructor should be private or protected so that we are forced 
		// to use the factory...
	protected:
		// the constructor creates the renderer with a window of the specified
		// size, and with the specified title.
		BaseHeadlessRenderer(unsigned width, unsigned height, std::string title);
		
		
		
	public:
		virtual ~BaseHeadlessRenderer();


		// make sure this renderer has the active window.
		virtual void SetActive()
		{
			#ifdef USE_EGL
			eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
			glBindFramebuffer(GL_FRAMEBUFFER, framebufferName);
			#endif
		}
		
		virtual void SetInactive()
		{
			// we don't seem to have en egl call for this.
		}
		
		virtual void SwapBuffers()
		{
			#ifdef USE_EGL
			SetActive();
			glFlush();
			//eglSwapBuffers( eglDisplay, eglSurface );
			#endif
		}
		
		virtual cv::Mat Capture();
		
	protected:
		
	#ifdef USE_EGL
		EGLDisplay eglDisplay;
		EGLSurface eglSurface;
		EGLContext eglContext;
	#endif
	
		GLuint framebufferName;
		GLuint renderTexture;
		GLuint depthrenderbuffer;
		
		
		
	};
} // namespace

#endif
