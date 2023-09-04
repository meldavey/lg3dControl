//--------------------------------------------------------------------------------------
// LightGrid2 3D effects
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
float4 g_MaterialAmbientColor;      // Material's ambient color
float4 g_MaterialDiffuseColor;      // Material's diffuse color

float3 g_LightDir[120];               // Light's direction in world space
float4 g_LightDiffuse[120];           // Light's diffuse color
float4 g_LightAmbient;              // Light's ambient color

texture g_MeshTexture;              // Color texture for mesh

float    g_fTime;                   // App's time in seconds
float4x4 g_mWorld;                  // World matrix for object
float4x4 g_mWorldViewProjection;    // World * View * Projection matrix

int		 g_nNumActiveLights;			// number of active lights

// projected texture vars
float4 vecEye;
float4 vecProjDir;
float4x4 ProjTextureMatrix;
texture ProjTexMap;
float3 curLightPos;

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler MeshTextureSampler = 
sampler_state
{
    Texture = <g_MeshTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

sampler ProjTexMapSampler = sampler_state
{
   Texture = <ProjTexMap>;
   MinFilter = Linear;
   MagFilter = Linear;
   MipFilter = Linear;   
   AddressU = clamp;
   AddressV = clamp;
   AddressW = clamp;   
};


//--------------------------------------------------------------------------------------
// Vertex shader output structure
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Position   : POSITION;   // vertex position 
    float4 Diffuse    : COLOR0;     // vertex diffuse color (note that COLOR0 is clamped from 0..1)
    float2 TextureUV  : TEXCOORD0;  // vertex texture coords 
};

struct VS_OUTPUTP1 // pixel light pass
{
    float4 Pos  : POSITION;
    float3 Light : TEXCOORD0;
    float3 Norm : TEXCOORD1;
    float3 View : TEXCOORD2;
};

struct VS_OUTPUTP2 // projected texture pass
{
    float4 Pos : POSITION;
    float4 Norm : TEXCOORD0;
    float4 Tex : TEXCOORD1;
    float4 Proj : TEXCOORD2;
};

struct VS_OUTPUT_LITPROJTEX // lit projected texture
{
    float4 Pos : POSITION;
    float Attenuation : COLOR0; // vert color r used for attenuation
    float4 Norm : TEXCOORD0;
    float4 Tex : TEXCOORD1;
    float4 Proj : TEXCOORD2;
    float3 Light : TEXCOORD3;
    float3 View : TEXCOORD4;
    float2 TextureUV  : TEXCOORD5;  // vertex texture coords 
};

//--------------------------------------------------------------------------------------
// This shader computes standard transform and lighting
//--------------------------------------------------------------------------------------
VS_OUTPUT RenderSceneVS( float4 vPos : POSITION, 
                         float3 vNormal : NORMAL,
                         float2 vTexCoord0 : TEXCOORD0,
                         uniform bool bTexture,
                         uniform bool bAnimate )
{
    VS_OUTPUT Output;
    float3 vNormalWorldSpace;
  
    float4 vAnimatedPos = vPos;
    
    // Animation the vertex based on time and the vertex's object space position
    if( bAnimate )
		vAnimatedPos += float4(vNormal, 0) * (sin(g_fTime+5.5)+0.5)*5;
    
    // Transform the position from object space to homogeneous projection space
    Output.Position = mul(vAnimatedPos, g_mWorldViewProjection);
    
    // Transform the normal from object space to world space    
    vNormalWorldSpace = normalize(mul(vNormal, (float3x3)g_mWorld)); // normal (world space)
    
    // Compute simple directional lighting equation
    float3 vTotalLightDiffuse = float3(0,0,0);
    for(int i=0; i<g_nNumActiveLights; i++ )
        vTotalLightDiffuse += g_LightDiffuse[i] * max(0,dot(vNormalWorldSpace, g_LightDir[i]));
        
    Output.Diffuse.rgb = g_MaterialDiffuseColor * vTotalLightDiffuse + 
                         g_MaterialAmbientColor * g_LightAmbient;   
    Output.Diffuse.a = 1.0f; 
    
    // Just copy the texture coordinate through
    if( bTexture ) 
        Output.TextureUV = vTexCoord0; 
    else
        Output.TextureUV = 0; 
    
    return Output;    
}

VS_OUTPUTP1 VSP1(float4 Pos : POSITION, float3 Normal : NORMAL)
{
    VS_OUTPUTP1 Out = (VS_OUTPUTP1)0;      
    Out.Pos = mul(Pos, g_mWorldViewProjection);			// transform Position
    Out.Light = g_LightDir[0];						// L
    float3 PosWorld = normalize(mul(Pos, g_mWorld)); 
    Out.View = vecEye - PosWorld;					// V
    Out.Norm = mul(Normal, g_mWorld);				// N
    
   return Out;
}

VS_OUTPUTP2 VSP2(float4 Pos : POSITION, float3 Normal : NORMAL)
{
    VS_OUTPUTP2 Out = (VS_OUTPUTP2)0;      
    Out.Pos = mul(Pos, g_mWorldViewProjection);				// transform Position
	Out.Tex = mul(ProjTextureMatrix, Pos);				// project texture coordinates
	
    float4 PosWorld = normalize(mul(Pos, g_mWorld));	// vertex in world space
    Out.Norm = normalize(mul(Normal, g_mWorld));		// normal in world space
    Out.Proj = PosWorld - normalize(vecProjDir);		// projection vector in world space

   return Out;
}

VS_OUTPUT_LITPROJTEX VS_LITPROJTEX(float4 Pos : POSITION, float3 Normal : NORMAL, float2 vTexCoord0 : TEXCOORD0)
{
	float spotRange = 0.05; // == 1 / maxRange

	VS_OUTPUT_LITPROJTEX Out = (VS_OUTPUT_LITPROJTEX)0;

    Out.Pos = mul(Pos, g_mWorldViewProjection);			// transform Position
	Out.Tex = mul(ProjTextureMatrix, Pos);				// project texture coordinates

	float3 vert2light = curLightPos.xyz - Pos;			// vector from vertex to current light
	float d = length(vert2light);						// distance between the two, need this for attenuation
	Out.Attenuation = 1 - (d * spotRange);				// linear attenuation
	Out.Attenuation = clamp(Out.Attenuation, 0, 1);		// clamp to COLOR0 range

    float4 PosWorld = normalize(mul(Pos, g_mWorld));	// vertex in world space
    Out.Norm = normalize(mul(Normal, g_mWorld));		// normal in world space
    Out.Proj = PosWorld - normalize(vecProjDir);		// projection vector in world space
    Out.Light = g_LightDir[0];							// Light (projected texture) direction
    Out.View = vecEye - PosWorld;						// View direction
	Out.TextureUV = vTexCoord0;							// pass along model texture coords

	return Out;
}

//--------------------------------------------------------------------------------------
// Pixel shader output structure
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 RGBColor : COLOR0;  // Pixel color    
};


//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
//       color with diffuse material color
//--------------------------------------------------------------------------------------
PS_OUTPUT RenderScenePS( VS_OUTPUT In,
                         uniform bool bTexture ) 
{ 
    PS_OUTPUT Output;

    // Lookup mesh texture and modulate it with diffuse
    if( bTexture )
        Output.RGBColor = tex2D(MeshTextureSampler, In.TextureUV) * In.Diffuse;
    else
        Output.RGBColor = In.Diffuse;

    return Output;
}

PS_OUTPUT PSP1(float3 Light: TEXCOORD0, float3 Norm : TEXCOORD1, float3 View : TEXCOORD2) : COLOR
{
    PS_OUTPUT Output;
    // float4 diffuse = { 1.0f, 0.0f, 0.0f, 1.0f};
    // float4 ambient = {0.1,  0.0,  0.0, 1.0}; 
    float4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f};
    float4 ambient = {0.1,  0.1,  0.1, 1.0}; 
	
    float3 Normal = normalize(Norm);
    float3 LightDir = normalize(Light);
    float3 ViewDir = normalize(View);    
    float4 diff = saturate(dot(Normal, LightDir)); // diffuse component
    
    // compute self-shadowing term
    // float shadow = saturate(4* diff);
    float4 shadow = saturate(4 * diff);

    float3 Reflect = normalize(2 * diff * Normal - LightDir);  // R
    float4 specular = pow(saturate(dot(Reflect, ViewDir)), 8); // R.V^n

    // I = ambient + shadow * (Dcolor * N.L + (R.V)n)
    Output.RGBColor = ambient + shadow * (diffuse * diff + specular); 
    return Output;
}

float4 PSP2(float4 Norm : TEXCOORD0, float4 Tex: TEXCOORD1, float4 Proj : TEXCOORD2) : COLOR
{
    if (dot(Proj, Norm) <= 0.0)
		return Tex.w < 0.0 ? 0.0 : tex2Dproj(ProjTexMapSampler, Tex);
    else
		return 1.0;
}

PS_OUTPUT PS_LITPROJTEX(VS_OUTPUT_LITPROJTEX In) : COLOR
{
    PS_OUTPUT Output;

    float3 ambient = {0.1,  0.1,  0.1}; 
    // float3 diffuseOverride = {0.9,  0.9,  0.9}; 

    float3 Normal = normalize(In.Norm);
    float3 LightDir = normalize(In.Light);
    float3 ViewDir = normalize(In.View);

	// diffuse component
	float3 diffuse = saturate(dot(Normal, LightDir));
	diffuse = diffuse * tex2D(MeshTextureSampler, In.TextureUV);
	// diffuse = saturate(diffuseOverride + diffuse);  // uncomment to test with mostly projected texture map only visible, ie no base tex map contrib

	// specular component
    float3 Reflect = normalize(2 * diffuse * Normal - LightDir);  // R
    float3 specular = pow(saturate(dot(Reflect, ViewDir)), 8); // R.V^n

	// projected texture color
	float4 texContrib = 0.0;
    if (dot(In.Proj, In.Norm) <= 0.0)
		texContrib = In.Tex.w < 0.0 ? 0.0 : tex2Dproj(ProjTexMapSampler, In.Tex);

	// combine
	Output.RGBColor.rgb = ambient + (diffuse + specular) * (float3)texContrib * In.Attenuation;
	// Output.RGBColor.rgb = diffuse + specular;
	// Output.RGBColor = texContrib;
	Output.RGBColor.a = 1.0;
    return Output;
}

//--------------------------------------------------------------------------------------
// Renders scene to render target
//--------------------------------------------------------------------------------------
technique RenderSceneWithTexture
{
    pass P0
    {          
        VertexShader = compile vs_2_0 RenderSceneVS( true, false );
        PixelShader  = compile ps_1_1 RenderScenePS( true ); // trivial pixel shader (could use FF instead if desired)
    }
}

technique RenderSceneNoTexture
{
    pass P0
    {          
        VertexShader = compile vs_2_0 RenderSceneVS( false, false );
        PixelShader  = compile ps_1_1 RenderScenePS( false ); // trivial pixel shader (could use FF instead if desired)
    }
}

/*
technique RenderSceneLitProjTex
{
    pass P0
    {
        // compile shaders
        VertexShader = compile vs_1_1 VSP1();
        PixelShader  = compile ps_2_0 PSP1();
    }

    pass P1
    {
        Sampler[0] = (ProjTexMapSampler);		

        // enable alpha blending
        AlphaBlendEnable = TRUE;
        SrcBlend         = ZERO;
        DestBlend        = SRCCOLOR;

        // compile shaders
        VertexShader = compile vs_1_1 VSP2();
        PixelShader  = compile ps_2_0 PSP2();
    }
}
*/

technique RenderSceneProjTex
{
    pass P0
    {
        Sampler[0] = (ProjTexMapSampler);		

        // enable alpha blending
        AlphaBlendEnable = TRUE;
        SrcBlend         = ZERO;
        DestBlend        = SRCCOLOR;

        // compile shaders
        VertexShader = compile vs_1_1 VSP2();
        PixelShader  = compile ps_2_0 PSP2();
    }
}

technique RenderSceneLitProjTex
{
    pass P0
    {
        Sampler[0] = (MeshTextureSampler);		
        Sampler[1] = (ProjTexMapSampler);		

        // enable alpha blending
        AlphaBlendEnable = TRUE;
        SrcBlend         = ONE;
        DestBlend        = ZERO;

        // compile shaders
        VertexShader = compile vs_1_1 VS_LITPROJTEX();
        PixelShader  = compile ps_2_0 PS_LITPROJTEX();
    }
}
