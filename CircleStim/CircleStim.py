# ###############################################################################
# CircleStim
#

from bge import logic as G
from bge import render as R
from bge import events as GK
from mathutils import Vector, geometry, Color
import math as m

# import site
# print (site.getusersitepackages())
# site.addsitedir("/Python/Lib/site-packages/numpy/")

# import sys
# sys.path.append("/Python/Lib/site-packages/numpy/")

# from numpy.random import randn 
# import sys 
# print (sys.version_info)

# pythonDir = 'C:\WinPython-64bit-3.3.2.3\python-3.3.2.amd64\Lib\site-packages'
# sys.path.append(pythonDir + '\dateutil') 
# print (sys.path)
# import dateutil

# import sci_viz 
# from sci_viz.node_tools import make_link

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

    print ('[A]spect [S]cale [R]otate [C]olor [F]requency [A]spect [M]askRotate')
    G.mode = "Rotate"
    G.hole = objects["Hole"] 
    G.stim = objects["Stim"]
    G.sun = objects["Sun"]
    # G.hole = objects["Suzanne"]

    G.stimRot = 0.0
    G.stimScaleFactor = 1.0 
    G.stimLocation = [0.0, 0.0, G.stim.worldPosition[2]]

    G.holeScaleFactor = 1.0 
    G.holeLocation = [0.0, 0.0, G.hole.worldPosition[2]]
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
            elif value == GK.SKEY:
                G.mode = 'Scale'
            elif value == GK.RKEY:
                G.mode = 'Rotate'
            elif value == GK.CKEY:
                G.mode = 'Color'
            elif value == GK.TKEY:
                G.mode = 'Temporal'
            elif value == GK.MKEY:
                G.mode = 'Mask'
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
    # print (G.hole.worldPosition)
    # print ('vsync ')

# ###############################################################################
#     Mouse Movement
# ###############################################################################
def mouse_move(cont):
    owner = cont.owner
    sensor = cont.sensors["s_movement"]

    #print(dir(sensor))

    follow_mouse_intersection(sensor)
    #follow_mouse(sensor)


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

def mouse_down(cont):
    owner = cont.owner
    sensor = cont.sensors["s_mouse_down"]
    mouse_op ('down')


def mouse_up(cont):
    owner = cont.owner
    sensor = cont.sensors["s_mouse_up"]
    mouse_op('up')

def mouse_op (direction):
    if G.mode == 'Aspect':
        factor = 1.1 
        if direction == 'down': 
            factor = 1.0 / factor
        G.hole.worldScale = Vector((G.hole.worldScale.x * factor,  G.hole.worldScale.y * 1.0 / factor, G.hole.worldScale.z))
    elif G.mode == 'Scale':
        factor = 1.1 
        if direction == 'down': 
            factor = 1.0 / factor
        G.hole.worldScale = G.hole.worldScale * factor
    elif G.mode == 'Rotate':
        factor = 3.1415926 * 2 / 360 * 10    # last is degrees per click
        if direction == 'down': 
            factor *= -1.0
        G.stim.applyRotation ([0.0, 0.0, factor])
    elif G.mode == 'Temporal':
        factor = 1.3 
        if direction == 'down': 
            factor = 1.0 / factor
        G.stim.worldScale = G.stim.worldScale * factor
    elif G.mode == 'Color':
        factor = 1.1 
        if direction == 'down': 
            factor = 1.0 / factor
        print ('sun', G.sun.color)
        # G.sun.color[0] *= factor
        # G.sun.color[1] = 0.5
        hsv = Color(G.sun.color)
        hsv.h *= factor
        hsv.s = 0.5
        hsv.v = 1.0
        G.sun.color = (255, 0, 0)
        print (hsv)
    elif G.mode == 'Mask':
        factor = 3.1415926 * 2 / 360 * 10    # last is degrees per click
        if direction == 'down': 
            factor *= -1.0
        G.hole.applyRotation ([0.0, 0.0, factor])
    elif G.mode == GK.ZKEY:
        G.stim.worldPosition = [0, 0, G.stim.worldPosition[2]]
        G.hole.worldPosition = [0, 0, G.hole.worldPosition[2]]


