import bge

# get current scene
scene = bge.logic.getCurrentScene()

# get the current controller
controller = bge.logic.getCurrentController()

# get object script is attached to
obj = controller.owner

# check to see video has been added
if "Video" in obj:
    
    # get video 
    video = obj["Video"]
    
    # update the video 
    video.refresh(True)

                        
# if video hasn't been added 
else:

    # import VideoTexture module
    import VideoTexture
    
    # get matID for the movie screen    
    matID = VideoTexture.materialID(obj, 'MAScreenZ')
    print (matID)
    
    # get the texture
    video = VideoTexture.Texture(obj, matID)
    
    movieName = obj['movie']
    
    # get movie path
    movie = bge.logic.expandPath('//' + movieName)
    print(movie)
    
    # get movie
    video.source = VideoTexture.VideoFFmpeg(movie)
    
    # set scaling
    video.source.scale = True   
    
    # save mirror as an object variable
    obj["Video"] = video
    
    # check for optional loop property
    if "loop" in obj:
        
        # loop it forever
        if obj['loop'] == True:
            video.source.repeat = -1
    
        # no looping
        else:
            video.source.repeat = 0
    
    # start the video
    video.source.play()

