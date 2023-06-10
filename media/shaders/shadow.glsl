#shader vertex
#version 410 core

uniform mat4 MVP;
in vec3 vPosition;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
}

#shader pixel
#version 410 core

void main()
{
}
