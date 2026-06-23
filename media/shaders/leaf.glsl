// Leaf shader: same lighting as flower.glsl (point lights + cool moonlight + ambient, two-sided) but
// the albedo comes from the leaf TEXTURE (leaf_color.png) instead of the vertex colour. Used for the
// generated leaf blades. No alpha discard (the blade geometry is already leaf-shaped), which is why
// this is a dedicated pass rather than reusing forward_lighting (whose discard/normal-map path left
// the leaves black).

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
varying vec2 texCoord;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  position = modelMatrix * vec4(vPosition, 1.0);
  normal   = modelMatrix * vec4(vNormal, 0.0);
  texCoord = vTexCoord;
}

#shader pixel
precision mediump float;

varying vec4 position;
varying vec4 normal;
varying vec2 texCoord;

#define outColor gl_FragColor
#define texture texture2D

#define MAX_POINT_LIGHTS 32

uniform sampler2D diffuseTexture;
uniform vec3 worldViewPosition;
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
uniform vec3 pointLightAttenuations[MAX_POINT_LIGHTS];
uniform float pointLightRanges[MAX_POINT_LIGHTS];

const float AMBIENT = 0.18;
const float WRAP    = 0.35;

void main()
{
  vec3 P = position.xyz;
  vec3 N = normalize(normal.xyz);
  vec3 eyeDir = normalize(worldViewPosition - P);

  // two-sided: the blade is a thin surface, light whichever face we see
  if (dot(N, eyeDir) < 0.0)
    N = -N;

  vec3 albedo = texture(diffuseTexture, texCoord).rgb;
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

  vec3 moonDir   = normalize(vec3(-0.35, 0.88, 0.32));
  vec3 moonColor = vec3(0.85, 0.95, 1.15);
  lit += albedo * moonColor * max(dot(N, moonDir), 0.0);

  outColor = vec4(lit, 1.0);
}
