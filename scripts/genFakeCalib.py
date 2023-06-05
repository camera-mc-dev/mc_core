import numpy as np
import sys
import os
import re


def LookAt( eye, up, target ):
	print( eye )
	print( up )
	print( target )
	# copied from my C++ code
	fwd3 = (target - eye)[:3]
	fwd3 /= np.linalg.norm(fwd3)
	
	up3 = up[:3]
	
	rgt3 = np.cross( fwd3, up3 )
	rgt3 = rgt3 / np.linalg.norm(rgt3)
	
	up3 = np.cross( fwd3, rgt3 )
	up3 = up3 / np.linalg.norm( up3 )
	
	tmp = np.array( [(-np.dot(rgt3,eye[:3])),-np.dot(up3,eye[:3]),-np.dot(fwd3,eye[:3]), 1.0 ] )
	
	
	M = np.eye(4,4)
	M[0, 0:3] = rgt3
	M[1, 0:3] = up3
	M[2, 0:3] = fwd3
	M[:,   3] = tmp
	
	return M


def PointsOnCircle(r,z,n=100):
    return np.array([ [np.cos(2*np.pi/n*x)*r,np.sin(2*np.pi/n*x)*r, z, 1.0] for x in range(0,n)]).T

#
# Define camera intrinsics
#

imW = 1920
imH = 1080
fl  = 1000
s   = 0.0
a   = 1.0
cx  = imW / 2
cy  = imH / 2

distParams = np.array( [0, 0, 0, 0, 0] )

K = np.array( [[ fl, s, cx], [0, a*fl, cy], [0, 0, 1] ] )


# generate some camera centres in a circle.
camCents = PointsOnCircle( 2000, 1500, 16 )
print( camCents.shape )


# set what we're looking at.
o = np.array( [[0,0,0,1]] ).T

# set the up axis
u = np.array( [[0,0,1,1]] ).T

for c in range( camCents.shape[1] ):
	fn = "fake-cam-%02d.calib"%c
	outfi = open( fn, 'w' )
	
	outfi.write("%d\n%d\n"%(imW,imH))
	outfi.write("%s\n\n"%re.sub('[\[\]]', '', np.array_str(K)))
	
	L = LookAt( camCents[:,c], u[:,0], o[:,0] )
	outfi.write("%s\n\n"%re.sub('[\[\]]', '', np.array_str(L)))
	
	outfi.write("%s\n\n"%re.sub('[\[\]]', '', np.array_str(distParams)))



