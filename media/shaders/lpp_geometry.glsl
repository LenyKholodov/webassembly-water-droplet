#shader vertex
precision mediump float;

uniform mat4 MVP;
uniform mat4 modelMatrix;
uniform vec3 worldViewPosition;
attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexCoord;
varying vec4 eyeDirection;
varying vec4 normal;
varying vec2 texCoord;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  vec4 position = modelMatrix * vec4(vPosition, 1.0);
  eyeDirection = vec4(worldViewPosition - position.xyz, 0.0);
  normal = modelMatrix * vec4 (vNormal, 0.0);
  texCoord = vTexCoord;
}

#shader pixel

#extension GL_OES_standard_derivatives : enable

precision mediump float;

varying vec4 eyeDirection;
varying vec4 normal;
varying vec2 texCoord;

uniform sampler2D normalTexture;

mat3 CotangentFrame(const in vec3 N, const in vec3 p, const in vec2 uv)
{
  // get edge vectors of the pixel triangle
  vec3 dp1 = dFdx(p);
  vec3 dp2 = dFdy(p);
  vec2 duv1 = dFdx(uv);
  vec2 duv2 = dFdy(uv);

  // solve the linear system
  vec3 dp2perp = cross(dp2, N);
  vec3 dp1perp = cross(N, dp1);
  vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
  vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

  // construct a scale-invariant frame
  float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    
  return mat3(T * invmax, B * invmax, N);
}

void main()
{
  vec3 normalizedEyeDirection = normalize(eyeDirection.xyz);

  mat3 tbn = CotangentFrame(normalize(normal.xyz), normalizedEyeDirection, texCoord);

  vec3 mappedNormal = texture2D(normalTexture, texCoord).xyz * 2.0 - 1.0;

  mappedNormal = normalize(tbn * mappedNormal);

  gl_FragColor = vec4(mappedNormal, 1.0);
}
