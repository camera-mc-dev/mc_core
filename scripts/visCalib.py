import numpy as np
import sys
import os
from mpl_toolkits import mplot3d
import matplotlib.pyplot as plt

class Calib:
	def __init__(self, filename):
		infi = open( filename )
		
		# whole file into a string
		data = infi.read()
		
		# split into tokens
		ss = data.split()
		
		# image shape 
		self.width  = int( ss[0] )
		self.height = int( ss[1] )
		
		# K matrix
		K0 = [float(v) for v in ss[2:11]]
		self.K = np.array( K0 ).reshape((3,3))
		
		# L matrix
		L0 = [float(v) for v in ss[11:27]]
		self.L = np.array( L0 ).reshape((4,4))
		
		print( self.L )
		print( np.linalg.inv( self.L ) )
		print("--")
		
		# distortions
		self.k = [float(v) for v in ss[27:]]


def Scatter( pts, colour, ax ):
	x = pts[:,0,0]
	y = pts[:,1,0]
	z = pts[:,2,0]
	ax.scatter3D(x, y, z, color=colour)

def Lines( pts0, pts1, colour, ax ):
	assert( pts0.shape == pts1.shape )
	for c in range( pts0.shape[0] ):
		x = [pts0[c,0,0], pts1[c,0,0]]
		y = [pts0[c,1,0], pts1[c,1,0]]
		z = [pts0[c,2,0], pts1[c,2,0]]
		ax.plot3D( x, y, z, colour )


cfs = sys.argv[1:]

calibs = [ Calib(f) for f in cfs ]

n = 1.0
#n = 500

# camera origins
o = np.array( [[0,0,0,1]] ).T
camOrigins = np.array( [ np.dot( np.linalg.inv(c.L), o ) for c in calibs ] )
print( camOrigins.shape )

# camera xs
x = np.array( [[n,0,0,1]] ).T
camXs = np.array( [ np.dot( np.linalg.inv(c.L), x ) for c in calibs ] )


# camera ys
y = np.array( [[0,n,0,1]] ).T
camYs = np.array( [ np.dot( np.linalg.inv(c.L), y ) for c in calibs ] )


# camera zs
z = np.array( [[0,0,n,1]] ).T
camZs = np.array( [ np.dot( np.linalg.inv(c.L), z ) for c in calibs ] )

mnZ = np.mean( camZs, axis=0 )
print( mnZ )


sceneOrigin = np.array( [[[0],[0],[0]]] )
sceneXs     = np.array( [[[n],[0],[0]]] )
sceneYs     = np.array( [[[0],[n],[0]]] )
sceneZs     = np.array( [[[0],[0],[n]]] )

# draw stuff
fig = plt.figure(figsize=(9, 6))
ax = plt.axes(projection='3d')

Scatter( camOrigins, 'gray', ax )
Lines( camOrigins, camXs, 'red', ax )
Lines( camOrigins, camYs, 'green', ax )
Lines( camOrigins, camZs, 'blue', ax )

Lines( sceneOrigin, sceneXs, 'red', ax )
Lines( sceneOrigin, sceneYs, 'green', ax )
Lines( sceneOrigin, sceneZs, 'blue', ax )


ax.axes.set_xlim3d(left=-4000, right=4000) 
ax.axes.set_ylim3d(bottom=-4000, top=4000) 
ax.axes.set_zlim3d(bottom=-4000, top=4000)

plt.show()



