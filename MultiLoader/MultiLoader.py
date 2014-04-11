from bge import logic as G
from bge import render as R
from bge import events as GK


def keyboard(cont):
    owner = cont.owner
    sensor = cont.sensors["s_keyboard"]
    #sensor = G.keyboard.events

    if sensor.positive:
        keylist = sensor.events

        for key in keylist:
            value = key[0]

            if value == GK.F1KEY:
                G.setLogicTicRate(1)

                G.startGame(r"..\CircleStim\CircleStim180.blend")
            elif value == GK.F2KEY:
                G.setLogicTicRate(1)
                G.startGame(r"..\CircleStim\blackwhite.blend")
            elif value == GK.F3KEY:
                G.setLogicTicRate(1)
                G.startGame(r"..\MultiLoader\Terrain.blend")
            elif value == GK.F4KEY:
                G.setLogicTicRate(1)
                G.startGame(r"..\MultiLoader\TerrainDome.blend")
            elif value == GK.F5KEY:
                G.setLogicTicRate(1)
                G.startGame(r"..\Test\BeachSkydomeFrameCounterVideoTeapot912X1140.180Hz.blend")
            elif value == GK.F6KEY:
                G.setLogicTicRate(1)
                G.startGame(r"..\Test\RotatingSpheres.blend")