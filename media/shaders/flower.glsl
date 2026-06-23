// Procedural-flower shader: the vertex colour IS the (lit) albedo, so the generator can paint
// petals / florets / stem without any texture. Same point-light interface as forward_lighting.glsl,
// plus a small ambient so blooms still read at night. Two-sided (petals are single-strip surfaces).

#shader vertex
precision mediump float;

uniform mat4 MVP;
uniform mat4 modelMatrix;
attribute vec4 vColor;
attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexCoord;
varying vec4 position;
varying vec4 normal;
varying vec4 color;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  position = modelMatrix * vec4(vPosition, 1.0);
  normal   = modelMatrix * vec4(vNormal, 0.0);
  color    = vColor;
}

#shader pixel
precision mediump float;

varying vec4 position;
varying vec4 normal;
varying vec4 color;

#define outColor gl_FragColor

#define MAX_POINT_LIGHTS 32

uniform vec3 worldViewPosition;
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
uniform vec3 pointLightAttenuations[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];

const float AMBIENT = 0.16;        // floor so blooms are visible at night
const float WRAP    = 0.35;        // soft wrap-around diffuse for soft petals

void main()
{
  vec3 P = position.xyz;
  vec3 N = normalize(normal.xyz);
  vec3 eyeDir = normalize(worldViewPosition - P);

  // two-sided: face the normal toward the viewer so back-lit petals still shade
  if (dot(N, eyeDir) < 0.0)
    N = -N;

  vec3 albedo = color.rgb;
  vec3 lit = albedo * AMBIENT;

  for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
  {
    vec3 Lpos = pointLightPositions[i];
    vec3 Lcol = pointLightColors[i];
    vec3 Latt = pointLightAttenuations[i];
    float Lrange = pointLightRanges[i];

    float d = length(Lpos - P);
    float atten = min(1.0, Lrange / (Latt.x + Latt.y * d + Latt.z * d * d));
    vec3 Ldir = normalize(Lpos - P);

    float ndl = max(0.0, (dot(N, Ldir) + WRAP) / (1.0 + WRAP));
    lit += Lcol * atten * albedo * ndl;
  }

  // same cool directional moonlight as the leaves, so branches and leaves are lit consistently
  vec3 moonDir   = normalize(vec3(-0.35, 0.88, 0.32));
  vec3 moonColor = vec3(0.80, 0.90, 1.10);
  lit += albedo * moonColor * max(dot(N, moonDir), 0.0);

  outColor = vec4(lit, 1.0);
}
