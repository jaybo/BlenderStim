import bge
 
cont = bge.logic.getCurrentController()
 
VertexShader = """

   varying vec4 texCoords; // texture coordinates at this vertex

   void main() // all vertex shaders define a main() function
   {
      gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
         // this line transforms the predefined attribute gl_Vertex 
         // of type vec4 with the predefined uniform 
         // gl_ModelViewProjectionMatrix of type mat4 and stores 
         // the result in the predefined output variable gl_Position 
         // of type vec4. (gl_ModelViewProjectionMatrix combines 
         // the viewing transformation, modeling transformation and 
         // projection transformation in one matrix.)
         
        // coordinate of the 1st texture channel
        texCoords = gl_MultiTexCoord0; // in this case equal to gl_Vertex
   }
"""
 
FragmentShader = """
   
   varying vec4 texCoords; 
   uniform sampler2D color_0;
    
   void main()
   {   
        // gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        vec4 color_0 = texture2D(color_0, texCoords.st);
        //gl_FragColor = vec4(color_0.r, 1.0 - color_0.g, 1.0-color_0.b, 1.0;
        gl_FragColor.g = 0.0; // 1.0 - color_0.g;
        gl_FragColor.b = 0.0; // 1.0 - color_0.b;
        gl_FragColor.r = color_0.r;    
        
   }
"""

 
mesh = cont.owner.meshes[0]
for mat in mesh.materials:
    shader = mat.getShader()
    if shader != None:
        if not shader.isValid():
            shader.setSource(VertexShader, FragmentShader, 1)
            
        # get the first texture channel of the material
        shader.setSampler('color_0', 0)