# ###############################################################################
# VSync
#

from bge import logic as G
from bge import render as R
from bge import events as GK
from mathutils import Vector, geometry, Color
import math as m


def init_world():
    G.scenes = {"main":G.getCurrentScene()}
    objects = G.scenes["main"].objects

    R.showMouse(True)
    G.red = objects["red"]
    G.green = objects["green"]
    G.blue = objects["blue"]

    G.green.visible = True
    G.red.visible = True
    G.blue.visible = True

    G.vsyncCount = 0
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

        print (G.mode)
 
def vsync (cont):
    owner = cont.owner
    sensor = cont.sensors["s_vsync"]
        
    G.vsyncCount += 1
    i = (G.vsyncCount) % 3
    
    
    if i == 0:
        G.green.visible = False
        G.red.visible = True
        G.blue.visible = False
        #G.green.worldPosition = [G.green.worldPosition[0],G.green.worldPosition[1], 0]
        #G.red.worldPosition = [G.red.worldPosition[0],G.red.worldPosition[1], 1]
    elif i == 1:
        G.green.visible = True
        G.red.visible = False
        G.blue.visible = False
        #G.red.worldPosition = [G.red.worldPosition[0],G.red.worldPosition[1], 0]
        #G.blue.worldPosition = [G.blue.worldPosition[0],G.blue.worldPosition[1], 1]
    elif i == 2:
        G.green.visible = False
        G.red.visible = False
        G.blue.visible = True
        #G.blue.worldPosition = [G.blue.worldPosition[0],G.blue.worldPosition[1], 0]
        #G.green.worldPosition = [G.green.worldPosition[0],G.green.worldPosition[1], 1]

    print ('vsync ', i, G.vsyncCount)

# ###############################################################################
#     Mouse Movement
# ###############################################################################
def mouse_move(cont):
    owner = cont.owner

    G.vsyncCount += 1
    #print(G.color)
    #G.red.visible = False
    #G.green.visible = True
    #G.color = 'green'
    #follow_mouse_intersection(sensor)
    # follow_mouse(sensor)




