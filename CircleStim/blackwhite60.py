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
G.myFps = 60
G.duration = 60.0
G.prerollPostroll = 0.5

print (G.getLogicTicRate())
G.setLogicTicRate(60)
print ("LogicTicRate:    ",  G.getLogicTicRate())
print ("MaxLogicFrame:   ",  G.getMaxLogicFrame())
print ("MaxPhysicsFrame: ",  G.getMaxPhysicsFrame())
G.setMaxLogicFrame(1)
print ("MaxLogicFrame: ",  G.getMaxLogicFrame())
print ("fps: ",  G.getMaxLogicFrame())

def init_world():
    G.scene = G.getCurrentScene()
    objects = G.scene.objects

    R.showMouse(True)
    
    G.red = objects["red"]
    G.green = objects["green"]
    G.blue = objects["blue"]
    G.white = objects["white"]
    G.black = objects["black"]
    G.white.visible = False
    
    G.vsyncCount = 0
    
    G.things = []
    for j in range (30):
        o = G.scene.addObject( "Cube", "Cube", 0)
        o.color=[random(), random(), random(),1]
        o.worldPosition = [random()*10,random()*10,random()*10]
        G.things.append(o)

    G.prerollFrames = G.myFps * G.prerollPostroll
    G.frames = G.duration * G.myFps
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
    #return
      
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
            G.vsyncCount = 0
        else:
            
            #for o in G.things:
            #    o.worldPosition = [random()*10,random()*10,random()*10]
                
            viz = (G.vsyncCount & 1) == 0
                
            G.white.visible = not viz
            G.black.visible = viz
            #G.red.worldPosition[1] = G.red.worldPosition[1] + .002
            #G.NextFrame() 
            print ('vsync ', viz, G.vsyncCount)

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




