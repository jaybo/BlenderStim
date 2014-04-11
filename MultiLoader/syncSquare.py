# ###############################################################################
# SyncSquare
# 1. Add a plane called "SyncSquare" to the scene and make it a child of the camera
#    (so it moves with the camera and is always visible).
# 2. Make the Material for the plane "Emit 1.0", and "Shadeless" to ensuer it will be
#    shown full intensity.
# 3. Hook up this file as a Module to an "Always" sensor on the camera.

from bge import logic as G

# change these values
G.duration = 60.0 * 120 # seconds 
G.prerollPostroll = 0.5

G.vsyncCount = 0
G.syncPlane = None

if "SyncSquare" in G.getCurrentScene().objects:
    G.syncPlane = G.getCurrentScene().objects["SyncSquare"]
    G.syncPlane.color = [-1,-1,-1, 1]

    G.prerollFrames = 180 * G.prerollPostroll
    G.frames = G.duration * 180

    G.state = "preroll"
    print ('SyncSquare found. Duration: ',  G.duration, ' seconds')
   
else: print ('SyncSquare NOT found')


def vsync (cont):
    if G.syncPlane is None: return

    G.vsyncCount += 1
          
    if G.state == "preroll":
        if G.vsyncCount >= G.prerollFrames:
            G.vsyncCount = 0
            G.state = "main"
    elif G.state == "postroll":
        if G.vsyncCount >= G.prerollFrames:
            G.vsyncCount = 0
            G.syncPlane.color = [-1,-1,-1,1]
            G.state = "done"
    elif G.state == "done":
        G.syncPlane.color = [1,1,1,1]
        pass
        #G.state = "main"
    else:
        if G.vsyncCount >= G.frames:
            G.state = "postroll"
            # goto black
            G.syncPlane.color = [-1,-1,-1,1]
            G.vsyncCount = 0
        else:
            black = (G.vsyncCount & 1) == 1
            if black:
                G.syncPlane.color = [-1,-1,-1,1]
            else:
                G.syncPlane.color = [1,1,1,1]
            #print ('vsync ', black, G.vsyncCount)
