# ###############################################################################
# CircleStim
#

from bge import logic as G
from bge import render as R
from bge import events as GK
from mathutils import Vector, geometry
import math as m

# ###############################################################################
# Script to initialize the world.  This script is run once on startup
# ###############################################################################

class Plane:
    def __init__(self, vec_p, vec_n):
        self.p = vec_p
        self.n = vec_n


def init_world():
    G.scenes = {"main":G.getCurrentScene()}
    objects = G.scenes["main"].objects

    R.showMouse(True)

    # Modes - Mouse (default) [S]cale [R]otate [C]olor [F]requency [A]spect
    G.mode = "Mouse"
    G.hole = objects["Hole"] 
    G.stim = objects["Stim"]
    # G.hole = objects["Suzanne"]

    G.stimRot = 0.0
    G.stimScaleFactor = 1.0 
    G.stimLocation = [0.0, 0.0, 0.0]

    G.holeScaleFactor = 1.0 
    G.holeLocation = [0.0, 0.0, 0.0]
    print (G.hole)
    print (G.stim)

    G.plane = Plane(Vector((0,0,0)), Vector ((0,0,1)))
    print ('init_world')



# ###############################################################################
#     Keyboard Management
# ###############################################################################

def keyboard(cont):
    owner = cont.owner
    sensor = cont.sensors["s_keyboard"]

    if sensor.positive:
        keylist = sensor.events

        for key in keylist:
            value = key[0]

            if value == GK.AKEY:
                G.mode = 'Aspect'
                G.stimScaleFactor *= 2.0 
                G.stim.worldScale = [G.stimScaleFactor, G.stimScaleFactor, G.stimScaleFactor]
                G.hole.worldScale = [G.stimScaleFactor, G.stimScaleFactor, G.stimScaleFactor]
            elif value == GK.SKEY:
                G.mode = 'Scale'
                G.stimScaleFactor /= 2.0 
                G.stim.worldScale = [G.stimScaleFactor, G.stimScaleFactor, G.stimScaleFactor]
                G.hole.worldScale = [G.stimScaleFactor, G.stimScaleFactor, G.stimScaleFactor]
            elif value == GK.RKEY:
                G.mode = 'Rotate'
                G.stim.applyRotation ([0.0, 0.0, .1])
            elif value == GK.CKEY:
                G.mode = 'Color'
            elif value == GK.FKEY:
                G.mode = 'Frequency'

            elif value == GK.MKEY:
                G.mode = 'Mouse'
            elif value == GK.ZKEY:
                G.stim.worldPosition = [0, 0, G.stim.worldPosition[2]]
                G.hole.worldPosition = [0, 0, G.hole.worldPosition[2]]

        print (G.mode)
 
def vsync (cont):
    owner = cont.owner
    sensor = cont.sensors["s_vsync"]

    # if sensor.positive: 
    G.stim.worldPosition = G.stimLocation.copy()
    G.hole.worldPosition = G.holeLocation.copy()

    # print (G.stim.worldPosition)
    print (G.hole.worldPosition)
    print ('vsync ')

# ###############################################################################
#     Mouse Movement
# ###############################################################################
def mouse_move(cont):
    owner = cont.owner
    sensor = cont.sensors["s_movement"]

    #print(dir(sensor))

    follow_mouse_intersection(sensor)
    # follow_mouse(sensor)


def follow_mouse_intersection(sensor):
    ray_p0 = sensor.raySource
    ray_p1 = sensor.rayTarget
    # print ("rays ", ray_p0, ray_p1)
    intersection = geometry.intersect_line_plane(ray_p0, ray_p1, G.plane.p, G.plane.n)

    if intersection:
        # print (intersection)
        G.stimLocation = [intersection.x, intersection.y, G.stim.worldPosition[2]]
        G.holeLocation = [intersection.x, intersection.y, G.hole.worldPosition[2]]
    

def follow_mouse(sensor):

    screen_width  =R.getWindowWidth()
    screen_height =R.getWindowHeight()

    win_x, win_y = sensor.position

    x = win_x - screen_width/ 2#/ screen_width - 0.5
    y = screen_height/2 - win_y # - 0.5 #/ 2#/ screen_height - 0.5

    G.stimLocation = [x, y, G.stim.worldPosition[2]]
    G.holeLocation = [x, y, G.hole.worldPosition[2]]

    # print (G.stimLocation)
    # print (G.stimLocation)
    # print (sensor.hitPosition)