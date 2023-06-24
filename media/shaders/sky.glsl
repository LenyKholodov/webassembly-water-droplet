#shader vertex
precision mediump float;

uniform mat4 MVP;
uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelViewMatrix;
uniform vec3 worldViewPosition;
attribute vec3 vPosition;
varying vec4 position;
varying vec4 eyeDirection;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  position = modelMatrix * vec4(vPosition, 1.0);
  eyeDirection = modelViewMatrix * vec4(normalize(worldViewPosition - position.xyz), 0.0);
}

#shader pixel

precision mediump float;
varying vec4 position;
varying vec4 eyeDirection;

uniform samplerCube diffuseTexture;

void main()
{
  vec3 color = textureCube(diffuseTexture, normalize(position).xyz).xyz * 0.05;
  //gl_FragColor = vec4(eyeDirection.xyz, 1.0);
  gl_FragColor = vec4(color, 1.0);
}
