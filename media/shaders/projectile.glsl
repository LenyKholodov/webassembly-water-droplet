#shader vertex
#version 410 core

uniform mat4 MVP;
in vec3 vPosition;
in vec2 vTexCoord;
out vec2 texCoord;

void main()
{
  gl_Position = MVP * vec4(vPosition, 1.0);
  texCoord = vTexCoord;
}

#shader pixel
#version 410 core

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec3 outNormal;

uniform vec2 shadowMapPixelSize;
uniform sampler2D positionTexture;
uniform sampler2D projectileTexture;
uniform sampler2D shadowTexture;
uniform mat4 shadowMatrix;
uniform vec3 projectileColor;

in vec2 texCoord;

float OffsetLookup(vec4 shadowTexCoord, vec2 offset)
{
  float shadowDepth = texture(shadowTexture, shadowTexCoord.xy + offset * shadowMapPixelSize * shadowTexCoord.w).x + 0.0001;

  if (shadowDepth < shadowTexCoord.z)
  {
    return shadowTexCoord.z - shadowDepth;
  }
 
  return 1.0;
}

float PCF(in vec4 shadowTexCoord)
{
  float sum = 0.0;
  float y = -1.5;

  const int STEPS_COUNT = 3;

  for (int i=0; i<STEPS_COUNT; i++, y += 1.5)
  { 
    float x = -1.5;

    for (int j=0; j<STEPS_COUNT; j++, x+= 1.5)
      sum += OffsetLookup(shadowTexCoord, vec2(x, y));
  }

  return sum / float (STEPS_COUNT * STEPS_COUNT);
}

void main()
{
  vec3 position = texture(positionTexture, texCoord).xyz;
  vec4 shadowTexCoord = shadowMatrix * vec4(position, 1.0);

  vec3 color = vec3(0);
  vec3 normal = vec3(0);

  if (shadowTexCoord.w > 0)
  {
    shadowTexCoord /= shadowTexCoord.w;

    shadowTexCoord = shadowTexCoord * 0.5 + 0.5;

    if (shadowTexCoord.x >= 0.0 &&
        shadowTexCoord.x <= 1.0 &&
        shadowTexCoord.y >= 0.0 &&
        shadowTexCoord.y <= 1.0)
    {
      float attenuation = PCF(shadowTexCoord);
      vec4 projectileTexColor = texture(projectileTexture, shadowTexCoord.xy);

      color = attenuation * projectileTexColor.xyz * projectileColor;

      float normalAttenuation = dot(color.rgb, vec3(0.299, 0.587, 0.114));
      normal = -outNormal * normalAttenuation;
    }
  }

  outAlbedo = vec4(color, 1.0);
  outNormal = normal;
}
