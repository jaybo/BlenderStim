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

    G.color  = 'red'
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

    if G.color == 'red':
        G.red.visible = False
        G.green.visible = True
        G.color = 'green'
    elif G.color == 'green':
        G.green.visible = False
        G.blue.visible = True
        G.color = 'blue'
    elif G.color == 'blue':
        G.blue.visible = False
        G.red.visible = True
        G.color = 'red'

    # print ('vsync ')

# ###############################################################################
#     Mouse Movement
# ###############################################################################
def mouse_move(cont):
    owner = cont.owner
    sensor = cont.sensors["s_movement"]

    #print(dir(sensor))

    follow_mouse_intersection(sensor)
    # follow_mouse(sensor)




