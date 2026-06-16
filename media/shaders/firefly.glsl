#shader vertex
precision mediump float;

uniform mat4 MVP;
uniform mat4 modelMatrix;

attribute vec3 vPosition;
attribute vec3 vNormal;

varying vec3 worldPos;
varying vec3 worldNormal;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  worldPos    = (modelMatrix * vec4(vPosition, 1.0)).xyz;
  worldNormal = (modelMatrix * vec4(vNormal, 0.0)).xyz;
}

#shader pixel
precision mediump float;

varying vec3 worldPos;
varying vec3 worldNormal;

uniform vec3 worldViewPosition;
uniform vec3 glowColor;   // green * pulse * fade, set per firefly each frame (0 -> invisible)

void main()
{
  // soft orb: bright facing the camera, fading toward the silhouette
  vec3  N = normalize(worldNormal);
  vec3  V = normalize(worldViewPosition - worldPos);
  float facing = max(dot(N, V), 0.0);
  float glow = facing * facing; // tighter, softer core

  // additive blend (in the firefly pass), so the colour simply adds as light
  gl_FragColor = vec4(glowColor * glow, 1.0);
}
