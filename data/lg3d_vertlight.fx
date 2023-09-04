
#define MAX_BASIC_LIGHTS 48

float4x4 g_mWorldView;		// World X View matrix
float4x4 g_mProj;			// Viewer's (camera) projection matrix
float4x4 g_mWorld;			// world transformation matrix - used to transform object normals into world space for vertex lighting
float4   g_vLightAmbient;	// global ambient light
float4   g_vMaterial;		// object (vertex) material color

float3	 g_LightDirWorld[MAX_BASIC_LIGHTS];		// Light's direction in world space
float3	 g_LightPosWorld[MAX_BASIC_LIGHTS];		// Light's position in world space
float3	 g_LightDiffuse[MAX_BASIC_LIGHTS];		// Light's diffuse color
float	 g_fCosThetaWorld[MAX_BASIC_LIGHTS];	// Cosine of theta of the spot light
int		 g_nNumActiveLights;					// number of active basic lights

texture	tColorMap;			// object's texture map
sampler ColorSampler = sampler_state
{
	texture = (tColorMap);

	MipFilter = Linear;
	MinFilter = Linear;
	MagFilter = Linear;
	
	AddressU = Wrap;
	AddressV = Wrap;
};

// ---------------------------------------------------------
// Scene generation:  multiple lights w/ per vertex lighting
// ---------------------------------------------------------

struct VS_OUTPUT
{
    float4 Position   : POSITION;   // vertex position 
    float4 Diffuse    : COLOR0;     // vertex diffuse color (note that COLOR0 is clamped from 0..1)
    float2 TextureUV  : TEXCOORD0;  // vertex texture coords 
};

VS_OUTPUT RenderSceneMultiLightVS(
		float4 vPos : POSITION, 
		float3 vNormal : NORMAL,
		float2 vTexCoord0 : TEXCOORD0)
{
	VS_OUTPUT Output;

	// Transform the position from object space to homogeneous projection space
    Output.Position = mul( vPos, g_mWorldView );
    Output.Position = mul( Output.Position, g_mProj );

    float3 vNormalWorldSpace;

	// Transform the normal from object space to world space    
	vNormalWorldSpace = normalize(mul(vNormal, (float3x3)g_mWorld)); // normal (world space)

	float4 vPosWorld = mul(vPos, g_mWorld);
	
	// Compute simple directional lighting equation
	float3 vTotalLightDiffuse = g_vLightAmbient;
	float3 vLightVert;
	for(int i=0; i<g_nNumActiveLights; i++ ) {
		vLightVert = normalize((float3)vPosWorld - g_LightPosWorld[i]);
		float vDotLightDir = dot( vLightVert, g_LightDirWorld[i]);
		if (vDotLightDir > g_fCosThetaWorld[i]) {
			float vIntensity = (vDotLightDir - g_fCosThetaWorld[i]) / (1.0f - g_fCosThetaWorld[i]);
			vTotalLightDiffuse += vIntensity * g_LightDiffuse[i] * max(0,dot(vNormalWorldSpace, -g_LightDirWorld[i]));
		}
	}

	// here, we just assign the diffuse light contribution as output.  Could multiply by diffuse & ambient materials of object if needed.
	Output.Diffuse.rgb = vTotalLightDiffuse;
	Output.Diffuse.a = 1.0f; 

	// Just copy the texture coordinate through
	Output.TextureUV = vTexCoord0; 
    
	return Output;    
}

struct PS_AMB_OUTPUT
{
	float4 RGBColor : COLOR0;  // Pixel color    
};

PS_AMB_OUTPUT RenderSceneMultiLightPS( VS_OUTPUT In )
{ 
	PS_AMB_OUTPUT Output;

	// Lookup mesh texture and modulate it with diffuse
	Output.RGBColor = tex2D(ColorSampler, In.TextureUV) * In.Diffuse * g_vMaterial;

	return Output;
}

// RenderSceneMultiLight - accumulates up to 128 vertex lights per object
// adds light contributions plus ambient, multiplies in texture & material
// colors.
technique RenderSceneMultiLight
{
	pass P0
	{          
// 		Lighting	= False;

		VertexShader = compile vs_2_0 RenderSceneMultiLightVS();
		PixelShader  = compile ps_1_1 RenderSceneMultiLightPS();
	}
}

// RenderSceneMultiLightBatch - accumulates up to 128 vertex lights per object
// adds light contributions plus ambient, multiplies in texture & material
// colors.  Performs additive blending with destination buffer.  Used for
// second and subsequent sets of up to 128 vertex lights.
technique RenderSceneMultiLightBatch
{
	pass P0
	{          
        // enable alpha blending
        AlphaBlendEnable = TRUE;

        // enable color blending such that multi-pass lights don't bleed color to white
        // SrcBlend         = ONE;
        // DestBlend        = ONE;
        SrcBlend         = SRCCOLOR;
        DestBlend        = INVSRCCOLOR;

		Lighting	= False;

		VertexShader = compile vs_2_0 RenderSceneMultiLightVS();
		PixelShader  = compile ps_1_1 RenderSceneMultiLightPS();
	}
}

