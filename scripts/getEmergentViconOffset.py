import os,sys
from ezc3d import c3d
from timecode import Timecode
import numpy as np

if len( sys.argv ) != 3:
	print( "Tool to calculate frame-offset between .c3d file and emergent timestamp" )
	print( "Gets the mean first frame timestamp from emergent export (Murray's script)" )
	print( "Compares that to the timecode embedded in the .c3d file" )
	print( "Usage: " )
	print( sys.argv[0], "< path to emergent capture >  < path to vicon .c3d track >" )
	exit(0)

evPath  = sys.argv[1]
c3dPath = sys.argv[2]



#
# Start by reading the "timecode" from the .c3d file
#

C = c3d( c3dPath )

tcData = C['parameters']['TIMECODE']

tcIsDropFrames = tcData['DROP_FRAMES']['value'][0] > 0
tcStandard     = tcData['STANDARD']['value'][0]
tcNumSubframes = tcData['SUBFRAMESPERFRAME']['value'][0][0]
tcExtraOffset  = tcData['OFFSETS']['value'][0][0]
tcTimecodeList = tcData['TIMECODES']['value']

print( "c3d timecode info: " )
print( "  is drop frame  : ", tcIsDropFrames )
print( "  standard       : ", tcStandard     )
print( "  sub frames     : ", tcNumSubframes )
print( "  offset         : ", tcExtraOffset )


#
# Now check the point information
#
c3dRate = C['header']['points']['frame_rate']
c3dSF   = C['header']['points']['first_frame']
c3dLF   = C['header']['points']['last_frame']
print( "c3d points information" )
print( "  frame rate : ", c3dRate )
print( "  first frame: ", c3dSF   )
print( "  last frame : ", c3dLF   )



# we can use the timecode library, and if we end up with drop frame stuff, we might really want to
# but - it's actually slightly redundant here
tcTimecodeStr  = "%02d:%02d:%02d:%02d"%( tcTimecodeList[0][0], tcTimecodeList[1][0], tcTimecodeList[2][0], tcTimecodeList[3][0] )
tcFPS = '25'
if tcStandard == 'PAL' and tcIsDropFrames == False:
	tcFPS = '25'
else:
	print( "script needs updating to set fps for .c3d timecode standard of: %s and dropframe status %b"%(tcStandard, tcIsDropFrames)  )
	exit(0)

tc = Timecode( tcFPS, tcTimecodeStr )
tcFrames = tc.frames * tcNumSubframes + tcExtraOffset
print( "base timecode is: ", tc )
print( f"which gives us {tcFrames} (sub)frames since midnight" )
print( "  ------  " )


#
# Get all first-frame timestamps from emergent data, and get the mean of them.
#
fs = sorted( [f for f in os.listdir( evPath ) if f[ -len(".tstamps"): ] == ".tstamps" ] )
count = 0
timestamps = []
for f in fs:
	with  open( f"{evPath}/{f}" ) as infi:
		lns = infi.readlines()
		ss = lns[0].split()
		if ss[0] == 'frame0000000000':
			timestamps.append( int( ss[1] ) )
			
medts = np.median( timestamps )
medts = int(medts * 1000)  # microseconds -> nanoseconds

print( "Got median first frame timestamp of: ", medts, " nanoseconds " )
dtFrame0 = np.datetime64( medts, 'ns'  )

print( "which is %s"%( np.datetime_as_string(dtFrame0, unit='ms') )  )

dtMidnight = np.datetime64( np.datetime_as_string(dtFrame0, unit='D') )
print( "midnight was %s", np.datetime_as_string(dtMidnight, unit='ms') )

tsDelta = dtFrame0 - dtMidnight
tsSeconds =  tsDelta / np.timedelta64(1,'s')

print( "which is ", tsSeconds, " since midnight " )

print( "------------------" )


#
# We can now work out the offset between .c3d data and camera data
#

# remove 37 leap seconds from emergent
tsSeconds -= 37

# now, there's a big questionmark hanging over all of this,
# which is "are the two systems using the same framerate?"
# let's assume "yes" for now.

c3dFramerate = float( tcFPS ) * tcNumSubframes
print( "assuming .c3d framerate %s is valid for both system"%( c3dFramerate ) )


tsFrames = int( round( tsSeconds * c3dFramerate ) )

print( "c3d frames since midnight: ", tcFrames )
print( "emergent frames since midnight: ", tsFrames )

offset = tsFrames - tcFrames
print( "offset: ", offset )
