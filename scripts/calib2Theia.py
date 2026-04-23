import os,sys
from calib import Calib
import numpy as np

if len( sys.argv ) < 4:
	print( "Tool to export a set of calibration files to Theia format" )
	print( sys.argv[0], "<output file> <sensor width in mm> <cam0.calib> <cam1.calib> ..." )
	exit(0)

# open output file and write header.
# it's basically xml.
outfi = open( sys.argv[1], 'w' )
outfi.write("""<calibration third_party="false">
    <results/>
    <cameras>
    """)

swimm = float( sys.argv[2] )

calpaths = sorted( sys.argv[3:] )
for cfp in calpaths:
	
	cal = Calib( cfp )
	
	# We assume that the calib file is of the form <vidfile>.calib
	# where vidfile includes the .mp4 extension.
	cfn = os.path.basename( cfp )
	
	vfn = cfn[ :cfn.find(".calib") ]
	print( cfp, cfn, vfn )
	
	# there's a certain ridiculousness to this, right?
	indent0 = "        "
	indent1 = "            "
	indent2 = "                "
	
	pimm = swimm / cal.width
	flmm = pimm * cal.K[0,0]
	
	outfi.write( '%s<camera active="1" serial="%s">\n'%( indent0, vfn ) )
	
	outfi.write( '%s<intrinsic\n'%indent1 )
	outfi.write( '%sfocallength="%f"\n'%(indent2, flmm)  )
	outfi.write( '%ssensorDimU="%f"\n'%(indent2, cal.width)  )
	outfi.write( '%ssensorDimV="%f"\n'%(indent2, cal.height)  )
	outfi.write( '%sfocalLengthU="%f"\n'%(indent2, cal.K[0,0])  )
	outfi.write( '%sfocalLengthV="%f"\n'%(indent2, cal.K[1,1])  )
	outfi.write( '%scenterPointU="%f"\n'%(indent2, cal.K[0,2])  )
	outfi.write( '%scenterPointV="%f"\n'%(indent2, cal.K[1,2])  )
	outfi.write( '%sskew="%f"\n'%(indent2, cal.K[0,1])  )
	outfi.write( '%sradialDistortion1="%f"\n'%(indent2, cal.k[0])  )
	outfi.write( '%sradialDistortion2="%f"\n'%(indent2, cal.k[1])  )
	outfi.write( '%stangentalDistortion1="%f"\n'%(indent2, cal.k[2])  )
	outfi.write( '%stangentalDistortion2="%f"\n'%(indent2, cal.k[3])  )
	outfi.write( '%sradialDistortion3="%f"\n'%(indent2, cal.k[4])  )
	outfi.write( '%s/>\n'%indent1 )
	
	outfi.write( '%s<transform\n'%indent1 )
	for r in range(3):
		for c in range(3):
			outfi.write( '%sr%d%d="%f"\n'%(indent2, r+1,c+1, cal.L[r,c])  )  # theia uses 1-indexing
	outfi.write( '%sx="%f"\n'%(indent2,  cal.L[0,3])  )
	outfi.write( '%sy="%f"\n'%(indent2,  cal.L[1,3])  )
	outfi.write( '%sz="%f"\n'%(indent2,  cal.L[2,3])  )
	outfi.write( '%s/>\n'%indent1 )
	
	outfi.write( '%s</camera>\n'%indent0 )
	
	
