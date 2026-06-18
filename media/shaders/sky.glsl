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
  gl_Position.z = gl_Position.w; // pin the sky to the far plane (NDC z = 1) so it's always the infinite background, never occluding/occluded by the scene at any camera distance
  position = modelMatrix * vec4(vPosition, 1.0);
  eyeDirection = modelViewMatrix * vec4(normalize(worldViewPosition - position.xyz), 0.0);
}

#shader pixel

precision mediump float;
varying vec4 position;
varying vec4 eyeDirection;

uniform samplerCube diffuseTexture;
uniform vec3 worldViewPosition;   // camera world position

void main()
{
  const float SKY_BRIGHTNESS = 1.0;
  // sample by the eye -> fragment direction (not the world-origin -> fragment direction), so the sky stays
  // fixed in world space and doesn't drift/parallax as the camera moves
  vec3 dir = normalize(position.xyz - worldViewPosition);
  vec3 color = textureCube(diffuseTexture, dir).xyz * SKY_BRIGHTNESS;
  gl_FragColor = vec4(color, 1.0);
}
