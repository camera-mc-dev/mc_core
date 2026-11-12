from SCons.Script import *

import urllib.request
import os

def SetCompiler(env):
	print( env['PLATFORM'] )
	conf = Configure(env)
	if env['PLATFORM'] == 'darwin':
		# Prefer an install of the non-apple clang.
		if conf.CheckProg('/opt/homebrew/opt/llvm@18/bin/clang++') != None:
			env.Replace(CXX = '/opt/homebrew/opt/llvm@18/bin/clang++')
			env.Append(CPPPATH=["/opt/homebrew/opt/llvm@18/include","/opt/homebrew/opt/libomp/include"])
			env.Append(LIBPATH=["/opt/homebrew/opt/llvm@18/lib","/opt/homebrew/opt/libomp/lib"])
			env.Append(LIBS=['omp'])
		else:
			print( " 'Real' clang not installed. Last we checked, Apple Clang didn't support OpenMP" )
			print( " Recommend installing 'real' clang from Homebrew or checking Apple clang OpenMP support " )
			exit(0)
		env.Append(CPPPATH=["/opt/homebrew/include", "/opt/homebrew/opt/sfml@2/include"])
		env.Append(LIBPATH=["/opt/homebrew/lib", "/opt/homebrew/opt/sfml@2/lib"])
	elif env['PLATFORM'] == 'posix':
		pass
	
	else:
		print( "-----------------------------------------------------------------------------------" )
		print( " ---- System is not Posix or Darwin( OSX ), so not bothering to try compiling! ---- " )
		exit(0)
	
	# Enable C++ 11
	env.Append(CPPFLAGS=['-std=c++17'])
	
	# all warnings, but also show how to disable each
	# warning if necessary.
	env.Append(CPPFLAGS=['-Wall', '-fdiagnostics-show-option' ])
	
	env = conf.Finish()


def SetProjectPaths(env):
	# path for src/ directory
	env.Append(CPPPATH=['.'])
	
	# get /usr/local in the include and library paths
	env.Append(CPPPATH=['/usr/local/include'])
	env.Append(LIBPATH=['/usr/local/lib'])


def FindFFMPEG(env):
	# We use FFMPEG directly for our video writer.
	env.ParseConfig("pkg-config libavformat --cflags --libs")
	env.ParseConfig("pkg-config libavutil --cflags --libs")
	env.ParseConfig("pkg-config libavcodec --cflags --libs")
	env.ParseConfig("pkg-config libswscale --cflags --libs")


def FindOpenGL(env):
	# We use SFML for creating windows and window interaction.
	env.Append(CPPPATH=['/usr/include/SFML'])
	if env['PLATFORM'] == 'posix':
		env.Append(LIBS=['jpeg'])
		env.ParseConfig("pkg-config sfml-all --cflags --libs")
	elif env['PLATFORM'] == 'darwin':
		env.Append(LIBS=['sfml-graphics', 'sfml-window', 'sfml-system'])
	
	# We also directly use OpenGL for rendering.
	# OpenGL on Mac needs to be done one way, and linux another.
	if env['PLATFORM'] == 'darwin':
		env.Append(LINKFLAGS=['-framework', 'OpenGL'])
	elif env['PLATFORM'] == 'posix':
		env.Append(LIBS=['GL', 'GLEW', 'GLU'])
	
	# freetype2 for font rendering. The v2 renderer can't use
	# SFML's text code, so we wrapped up our own.
	env.ParseConfig("pkg-config freetype2 --cflags --libs")
	
	# we can also use EGL for headless OpenGL rendering contexts,
	# which is great for remote applications where we need to render,
	# but don't care about seeing it.
	if env['PLATFORM'] == 'posix':
		env.Append(CPPDEFINES=["USE_EGL"])
		env.ParseConfig("pkg-config egl --cflags --libs")


def FindOpenCV(env):
	# We use OpenCV for lots of things.
	env.ParseConfig("pkg-config opencv4 --cflags --libs")

	# OpenCV now needs this... well, on Mac anyway...
	# Note: This assumes Opencv was installed with Homebrew.
	if env['PLATFORM'] == 'darwin':
		env.Append(LINKFLAGS=['-rpath', '/usr/local/opt/opencv3/lib'])

	# at present, OpenCV's pkg-config doesn't supply this
	if env['PLATFORM'] == 'posix':
		env.Append(LIBPATH=["/usr/local/share/OpenCV/3rdparty/lib/"])


def FindEigen(env):
	# Eigen is used for the maths.
	env.ParseConfig("pkg-config eigen3 --cflags --libs")

def FindBoost(env):
	# We're using boost filesystem to get multi-platform filesystem handling.
	env.Append(LIBS=['boost_filesystem']) # 'boost_system',
	env.Append(CPPFLAGS=["-DBOOST_NO_CXX11_SCOPED_ENUMS"] )

def FindMagick(env):
	# Image magick has advantages over OpenCV in my experience.
	# Having said that, we can probably make this optional or just plump for OpenCV
	# and avoid this dependency....
	env.ParseConfig("pkg-config Magick++ --cflags --libs")


def FindLibConfig(env):
	# libconfig is used for config files and data loading.
	env.ParseConfig("pkg-config libconfig++ --cflags --libs")

def FindSnappy(env):
	# We use a very basic custom image format for rapid saving 
	# of float images which are not hugely standardised as yet.
	# Our format does nothing special but it does use Google's
	# Snappy to get some speed-prioritised compression.
	env.Append(LIBS=["snappy"])

def FindCeres(env):
	# We use Ceres for our bundle adjust solver, which in turn requires
	# some google libs.
	env.Append(LIBS=["gflags", "glog", "ceres"])

def FindHDF5(env):
	# We use HDF5 files when we create and load a training dataset.
	env.ParseConfig("pkg-config hdf5 --libs --cflags")
	env.Append(LIBS=['hdf5'])
	env.Append(CPPFLAGS=['-DHAVE_HIGH_FIVE'])



# def FindHTTPLib( env ):
# 	if not os.path.exists("3rdParty/httplib"):
# 		os.makedirs("3rdParty/httplib")
# 	if not os.path.exists("3rdParty/httplib/httplib.h"):
# 		url = "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h"
# 		urllib.request.urlretrieve(url, "3rdParty/httplib/httplib.h")
# 	env.Append( CPPPATH=[f"{Dir(".").srcnode().abspath}/3rdParty/httplib"] )



def FindHTTPLib(env):
	# Anchor to the source root directory using SCons' Dir('#')
	root_dir = Dir('#').srcnode().abspath
	third_party_dir = os.path.join(root_dir, "3rdParty", "httplib")
	httplib_path = os.path.join(third_party_dir, "httplib.h")

	if not os.path.exists(third_party_dir):
		os.makedirs(third_party_dir, exist_ok=True)
		
	if not os.path.exists(httplib_path):
		print("Fetching cpp-httplib header...")
		url = "https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h"
		urllib.request.urlretrieve(url, httplib_path)
		print("Successfully downloaded httplib.h")

	# Append to CPPPATH so variant builds can always locate it in the source tree
	env.Append(CPPPATH=[third_party_dir])


def SetCoreConfig(env):
	# --
	# Basic setup
	# --
	
	SetCompiler(env)
	SetProjectPaths(env)
	
	# ---
	#  Now we add the various required libraries.
	# ---
	
	FindOpenCV(env)
	FindOpenGL(env)
	FindFFMPEG(env)
	FindEigen(env)
	FindBoost(env)
	FindMagick(env)
	FindLibConfig(env)
	FindSnappy(env)
	FindCeres(env)
	FindHDF5(env)
	FindHTTPLib(env)
