# ###############################################################################
# VSync
#

from bge import logic as G
from bge import render as R
from bge import events as GK
from random import random
from mathutils import Vector, geometry, Color
import math as m

# change these values
G.duration = 60.0 * 10
G.prerollPostroll = 0.5

print (G.getLogicTicRate())
#G.setLogicTicRate(180)
print ("LogicTicRate:    ",  G.getLogicTicRate())
print ("MaxLogicFrame:   ",  G.getMaxLogicFrame())
print ("MaxPhysicsFrame: ",  G.getMaxPhysicsFrame())
#G.setMaxLogicFrame(1)
print ("MaxLogicFrame: ",  G.getMaxLogicFrame())

def init_world():
    G.scene = G.getCurrentScene()
    objects = G.scene.objects

    R.showMouse(True)
    
    G.red = objects["red"]
    G.green = objects["green"]
    G.blue = objects["blue"]
    G.white = objects["white"]
    G.white1 = objects["white.001"]
    G.black = objects["black"]
    G.white.visible = False
    
    G.vsyncCount = 0
    
    G.things = []
    for j in range (150):
        o = G.scene.addObject( "Cube", "Cube", 0)
        o.color=[random(), random(), random(),1]
        o.localScale=[0.25, 0.25, 0.25]
        o.worldPosition = [random()*10,random()*10,random()*10]
        G.things.append(o)

    G.prerollFrames = 180 * G.prerollPostroll
    G.frames = G.duration * 180
    G.state = "preroll"
       
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
          
    if G.state == "preroll":
        if G.vsyncCount >= G.prerollFrames:
            G.vsyncCount = 0
            G.state = "main"
    elif G.state == "postroll":
        if G.vsyncCount >= G.prerollFrames:
            G.vsyncCount = 0
            G.state = "done"
    elif G.state == "done":
        #pass
        G.state = "main"
    else:
        if G.vsyncCount >= G.frames:
            G.state = "postroll"
            G.white.visible = False
            G.black.visible = True
            G.vsyncCount = 0
        else:
            rscale = 0.08
            for o in G.things:
                k = 0
                #o.worldPosition = [random()*10,random()*10,random()*10]
                o.applyMovement ([(random()-0.5) * rscale, (random()-0.5) * rscale,  (random()-0.5) * rscale], True)
                
            viz = (G.vsyncCount & 1) == 1
                
            G.white.visible = not viz
            G.black.visible = viz
            G.white1.visible = viz
            #G.red.worldPosition[1] = G.red.worldPosition[1] + .002 
            #print ('vsync ', viz, G.vsyncCount)

# ###############################################################################
#     Mouse Movement
# ###############################################################################
def mouse_move(cont):
    owner = cont.owner

    #print(G.color)
    #G.red.visible = False
    #G.green.visible = True
    #G.color = 'green'
    #follow_mouse_intersection(sensor)
    # follow_mouse(sensor)




