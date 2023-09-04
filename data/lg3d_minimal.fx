//--------------------------------------------------------------------------------------
// LightGrid2 3D effects
// Basic object rendering (tex + amb lighting only)
//--------------------------------------------------------------------------------------

// float4x4 g_mWorldViewProjection;    // World * View * Projection matrix
texture g_MeshTexture;              // Color texture for mesh
float4x4 g_mWorldView;				// World * View
float4x4 g_mProj;					// Projection

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

//--------------------------------------------------------------------------------------
// Vertex shaders
//--------------------------------------------------------------------------------------

struct VS_OUTPUT
{
    float4 Position   : POSITION;   // vertex position 
    float2 TextureUV  : TEXCOORD0;  // vertex texture coords 
};

//--------------------------------------------------------------------------------------
// This vertex shader transforms the position and passes the tex coord along
//--------------------------------------------------------------------------------------
VS_OUTPUT RenderSceneVS( float4 vPos : POSITION, 
                         float3 vNormal : NORMAL,
                         float2 vTexCoord0 : TEXCOORD0 )
{
	VS_OUTPUT Output;

	// Transform the position from object space to homogeneous projection space
	// Output.Position = mul(vPos, g_mWorldViewProjection);
	float4 tempPos;
    tempPos = mul( vPos, g_mWorldView );
    Output.Position = mul( tempPos, g_mProj );

	// Just copy the texture coordinate through
	Output.TextureUV = vTexCoord0; 

	return Output;    
}

//--------------------------------------------------------------------------------------
// Pixel shader output structure
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
	float4 RGBColor : COLOR0;  // Pixel color    
};

//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by doing a texture lookup
//--------------------------------------------------------------------------------------
PS_OUTPUT RenderScenePS( VS_OUTPUT In ) : COLOR
{ 
	PS_OUTPUT Output;

    float4 ambient = { 0.3f, 0.3f, 0.3f, 1.0f};
	// Lookup mesh texture and output it
	Output.RGBColor = tex2D(MeshTextureSampler, In.TextureUV) * ambient;
	Output.RGBColor.a = 1.0;

    // float4 diffuse = { 1.0f, 1.0f, 1.0f, 1.0f};
	// Output.RGBColor = diffuse;

	return Output;
}

technique RenderScene
{
    pass P0
    {          
        VertexShader = compile vs_1_1 RenderSceneVS();
        PixelShader  = compile ps_1_1 RenderScenePS();
    }
}
