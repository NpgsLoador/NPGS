#version 450
#pragma shader_stage(fragment)

layout(location = 0) out vec4 FragColor;

layout(location = 0) in _FragInput
{
    vec3 Normal;
	vec2 TexCoord;
	vec3 FragPos;
    vec4 FragPosLightSpace;
} FragInput;

struct FMaterial
{
    float Shininess;
};

struct FLight
{
    vec3 Position;
    vec3 Ambient;
    vec3 Diffuse;
    vec3 Specular;
};

layout(std140, set = 0, binding = 1) uniform LightMaterial
{
    FMaterial Material;
    FLight    Light;
    vec3      ViewPos;
} iLightMaterial;

layout(set = 1, binding = 0) uniform sampler   iSampler;
layout(set = 1, binding = 1) uniform texture2D iDiffuseTex;
layout(set = 1, binding = 2) uniform texture2D iNormalTex;
layout(set = 1, binding = 3) uniform texture2D iSpecularTex;
layout(set = 1, binding = 4) uniform sampler2D iShadowMap;

float CalcShadow(vec4 FragPosLightSpace, float Bias)
{
    vec3 ProjectiveCoord = FragPosLightSpace.xyz / FragPosLightSpace.w;
    ProjectiveCoord.xy = ProjectiveCoord.xy * 0.5 + 0.5;

    if (ProjectiveCoord.z > 1.0)
    {
        return 0.0;
    }

    float Shadow = 0.0;
    vec2 TexelSize = 1.0 / textureSize(iShadowMap, 0);
    
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float ClosestDepth = texture(iShadowMap, ProjectiveCoord.xy + vec2(x, y) * TexelSize).r;
            float CurrentDepth = ProjectiveCoord.z;
            Shadow += CurrentDepth - Bias > ClosestDepth ? 1.0 : 0.0;
        }
    }

    Shadow /= 9.0;

    return Shadow;
}

void main()
{
#if defined(LAMP_BOX)
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
#else
    vec3 AmbientColor = iLightMaterial.Light.Ambient * texture(sampler2D(iDiffuseTex, iSampler), FragInput.TexCoord).rgb;

    vec3 FragPosDx  = dFdx(FragInput.FragPos);
    vec3 FragPosDy  = dFdy(FragInput.FragPos);
    vec2 TexCoordDx = dFdx(FragInput.TexCoord);
    vec2 TexCoordDy = dFdy(FragInput.TexCoord);

    vec3 Tangent   = normalize(FragPosDx * TexCoordDy.y - FragPosDy * TexCoordDx.y);
    vec3 Bitangent = normalize(FragPosDy * TexCoordDx.x - FragPosDx * TexCoordDy.x);
    vec3 Normal    = normalize(FragInput.Normal);

    Tangent   = normalize(Tangent - Normal * dot(Normal, Tangent));
    Bitangent = cross(Normal, Tangent);

    mat3x3 TbnMatrix = mat3x3(Tangent, Bitangent, Normal);

    vec3  NormalRgb    = normalize(TbnMatrix * texture(sampler2D(iNormalTex, iSampler), FragInput.TexCoord).rgb * 2.0 - 1.0);
    vec3  LightDir     = normalize(iLightMaterial.Light.Position - FragInput.FragPos);
    float DiffuseCoef  = max(dot(NormalRgb, LightDir), 0.0);
    vec3  DiffuseColor = iLightMaterial.Light.Diffuse * DiffuseCoef * texture(sampler2D(iDiffuseTex, iSampler), FragInput.TexCoord).rgb;

    vec3  ViewDir       = normalize(iLightMaterial.ViewPos - FragInput.FragPos);
    vec3  HalfWayDir    = normalize(LightDir + ViewDir);
    float SpecularCoef  = pow(max(dot(NormalRgb, HalfWayDir), 0.0), iLightMaterial.Material.Shininess);
    vec3  SpecularColor = iLightMaterial.Light.Specular * SpecularCoef * texture(sampler2D(iSpecularTex, iSampler), FragInput.TexCoord).rgb;

    float Bias   = max(0.05 * (1.0 - dot(NormalRgb, LightDir)), 0.005);
    float Shadow = CalcShadow(FragInput.FragPosLightSpace, Bias);
    vec3  Result = AmbientColor + (1.0 - Shadow) * (DiffuseColor + SpecularColor);
    FragColor    = vec4(Result, 1.0);
#endif
}
