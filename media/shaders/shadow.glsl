#shader vertex
precision mediump float;
#ifndef GL_ES
#version 410 core
in vec3 vPosition;
#else
attribute vec3 vPosition;
#endif

uniform mat4 MVP;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
}

#shader pixel
precision mediump float;
#ifndef GL_ES
#version 410 core
#endif

void main()
{
}
