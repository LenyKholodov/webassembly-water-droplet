#shader vertex
#version 410 core

uniform mat4 MVP;
in vec4 vColor;
in vec3 vPosition;
in vec3 vNormal;
out vec3 position;
out vec3 normal;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  position = gl_Position.xyz;
  normal = vNormal;
}

#shader pixel
#version 410 core

in vec3 position;
in vec3 normal;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outSpecular;

void main()
{
  outPosition = position; 
  outNormal = normal; 
  outAlbedo = vec4(0.f, 0.f, 1.f, 1.f); 
  outSpecular = vec4(1.f, 0.f, 0.f, 1.f);
}
