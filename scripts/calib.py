import numpy as np
import cv2

class Calib:
	def __init__(self, filename=None):
		if filename == None:
		    self.width = 0
		    self.height = 0
		    self.L = np.eye(4)
		    self.K = np.eye(3)
		    self.k = np.zeros(5)
		else:
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
		    
		    # distortions
		    self.k = [float(v) for v in ss[27:]]
	
	def Write( self, filename ):
		outfi = open(filename, 'w')
		outfi.write("%d %d\n\n"%(self.width, self.height))
		
		for rc in range(3):
			for cc in range(3):
				outfi.write( "%f "%self.K[rc,cc] )
			outfi.write("\n")
		outfi.write("\n")
		
		for rc in range(4):
			for cc in range(4):
				outfi.write( "%f "%self.L[rc,cc] )
			outfi.write("\n")
		outfi.write("\n")
		
		for v in self.k:
			outfi.write("%f "%v )
		outfi.write("\n")
		outfi.close()
	

	def Rotate90( self ):
		R33 = cv2.Rodrigues( np.array( [0, 0, np.pi/2.0]  ) )[0]
		R44 = np.eye(4)
		R44[0:3,0:3] = R33
		
		self.L = np.dot( R44, self.L )
		
		h = self.height
		self.height = self.width
		self.width  = h
		
		nK = np.eye(3)
		nK[0,0] = self.K[1,1]
		nK[1,1] = self.K[0,0]
		nK[0,2] = h - self.K[1,2]
		nK[1,2] = self.K[0,2]
		self.K = nK

