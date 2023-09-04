//-----------------------------------------------------------------------------
// LightGrid2 3D effects
// effects file for generating shadow texture map
//-----------------------------------------------------------------------------

#define SHADOW_EPSILON 0.00005f
#define SMAP_SIZE 256

float4x4 g_mWorldViewProjection;    // World * View * Projection matrix
float4x4 g_mWorldView;				// World * View
float4x4 g_mProj;					// Projection
float4x4 g_mViewToLightProj;		// Transform from view space to light projection space

texture g_MeshTexture;              // Color texture for mesh
texture g_txShadow;					// ShadowMap

float	g_fCosTheta;  // Cosine of theta of the spot light
float3	g_vLightPos;  // Light position in view space
float3	g_vLightDir;  // Light direction in view space

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

sampler2D PixShadowSampler =
sampler_state
{
    Texture = <g_txShadow>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = Point;
    AddressU = Clamp;
    AddressV = Clamp;
};

//-----------------------------------------------------------------------------
// Vertex Shader: VertShadow
// Desc: Process vertex for the shadow map
//-----------------------------------------------------------------------------
void VertShadowMap( float4 Pos : POSITION,
                 out float4 oPos : POSITION,
                 out float2 Depth : TEXCOORD0 )
{
    //
    // Compute the projected coordinates
    //
	oPos = mul(Pos, g_mWorldViewProjection);

    //
    // Store z and w in our spare texcoord
    //
    Depth.xy = oPos.zw;
}

//-----------------------------------------------------------------------------
// Pixel Shader: PixShadow
// Desc: Process pixel for the shadow map
//-----------------------------------------------------------------------------
void PixShadowMap( float2 Depth : TEXCOORD0,
                out float4 Color : COLOR )
{
    //
    // Depth is z / w
    //
	Color = Depth.x / Depth.y;
}

void VertScene( float4 iPos : POSITION,
                float3 iNormal : NORMAL,
                float2 iTex : TEXCOORD0,
                out float4 oPos : POSITION,
                out float2 Tex : TEXCOORD0,
                out float4 vPos : TEXCOORD1,
                out float3 vNormal : TEXCOORD2,
                out float4 vPosLight : TEXCOORD3 )
{
    //
    // Transform position to view space
    //
    vPos = mul( iPos, g_mWorldView );

    //
    // Transform to screen coord
    //
    oPos = mul( vPos, g_mProj );

    //
    // Compute view space normal
    //
    vNormal = mul( iNormal, (float3x3)g_mWorldView );

    //
    // Propagate texture coord
    //
    Tex = iTex;

    //
    // Transform the position to light projection space, or the
    // projection space as if the camera is looking out from
    // the spotlight.
    //
    vPosLight = mul( vPos, g_mViewToLightProj );
}


float4 PixScene( float2 Tex : TEXCOORD0,
                 float4 vPos : TEXCOORD1,
                 float3 vNormal : TEXCOORD2,
                 float4 vPosLight : TEXCOORD3 ) : COLOR
{
    float4 Diffuse = { 0.0f, 0.0f, 0.0f, 0.0f};
    float4 Diffuse2 = { 0.4f, 0.4f, 0.4f, 0.0f};

    // vLight is the unit vector from the light to this pixel
    float3 vLight = normalize( float3( vPos - g_vLightPos ) );

    // Compute diffuse from the light
	if( dot( vLight, g_vLightDir ) > g_fCosTheta ) { // Light must face the pixel (within Theta)
		// Pixel is in lit area. Find out if it's in shadow

		// transform from RT space to texture space.
		float2 ShadowTexC = 0.5 * vPosLight.xy / vPosLight.w + float2( 0.5, 0.5 );
		ShadowTexC.y = 1.0f - ShadowTexC.y;

		// transform to texel space
		float2 texelpos = SMAP_SIZE * ShadowTexC;

		float LightAmount = (tex2D( PixShadowSampler, ShadowTexC ) + SHADOW_EPSILON < vPosLight.z / vPosLight.w) ? 0.0f : 1.0f;

		Diffuse = Diffuse2 * LightAmount;
		// Diffuse = Diffuse2;
	}
    return Diffuse;
}


//-----------------------------------------------------------------------------
// Technique: RenderShadow
// Desc: Renders the shadow map
//-----------------------------------------------------------------------------
technique RenderShadowMap
{
    pass p0
    {
        VertexShader = compile vs_1_1 VertShadowMap();
        PixelShader = compile ps_2_0 PixShadowMap();
    }
}

technique RenderSceneAdd
{
	pass p0
	{
        // enable alpha blending
        AlphaBlendEnable = TRUE;
        SrcBlend         = ONE;
        // DestBlend        = SRCCOLOR;
        DestBlend        = ONE;
        VertexShader = compile vs_1_1 VertScene();
        PixelShader = compile ps_2_0 PixScene();
	}
}

