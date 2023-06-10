#shader vertex
#ifndef GL_ES
#version 410 core
in vec3 vPosition;
#else
uniform vec3 vPosition;
#endif

uniform mat4 MVP;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
}

#shader pixel
#ifndef GL_ES
#version 410 core
#endif

void main()
{
}
