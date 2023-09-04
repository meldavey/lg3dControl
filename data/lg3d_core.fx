

#define SMAP_SIZE 512
#define SHADOW_EPSILON 0.00005f

float4x4 g_mWorldView;
float4x4 g_mProj;
float4x4 g_mViewToLightProj;			// Transform from view space to light projection space
float4   g_vMaterial;
float4   g_vLightColor;					// color of source light
float3   g_vLightPos;					// Light position in view space
float3   g_vLightDir;					// Light direction in view space
float3   g_vEyeDir;						// Eye direction in view space
float3   g_vEyePos;						// Eye position in view space
float4   g_vLightAmbient = float4( 0.3f, 0.3f, 0.3f, 1.0f );  // Use an ambient light of 0.3
float    g_fCosTheta;					// Cosine of theta of the spot light
float	 g_fLinearAttenuation;			// linear attenuation coefficient
float	 g_fQuadraticAttenuation;		// quadratic attenuation coefficient

texture	tColorMap;
texture tSpotMap;
texture tShadowMap;
texture tNormalMap;

sampler ColorSampler = sampler_state
{
	texture = (tColorMap);

	MipFilter = Linear;
	MinFilter = Linear;
	MagFilter = Linear;
	
	AddressU = Wrap;
	AddressV = Wrap;
};

sampler NormalSampler = sampler_state
{
	texture = (tNormalMap);

	MipFilter = Linear;
	MinFilter = Linear;
	MagFilter = Linear;
	
	AddressU = Wrap;
	AddressV = Wrap;
};

sampler SpotSampler = sampler_state
{
	texture = (tSpotMap);

	MipFilter = Linear;
	MinFilter = Linear;
	MagFilter = Linear;
	
	AddressU = Clamp;
	AddressV = Clamp;
};

sampler ShadowSampler = sampler_state
{
	texture = (tShadowMap);

    MinFilter = Point;
    MagFilter = Point;
    MipFilter = Point;

	AddressU = Clamp;
	AddressV = Clamp;
};

// ----------------------------------
// Shaders
// ----------------------------------


// ----------------------------------
// Shadow Map Generation
// ----------------------------------

void VS_Shadow( float4 Pos : POSITION,
                 float3 Normal : NORMAL,
                 out float4 oPos : POSITION,
                 out float2 Depth : TEXCOORD0 )
{
    // Compute the projected coordinates
    oPos = mul( Pos, g_mWorldView );
    oPos = mul( oPos, g_mProj );

    // Store z and w in our spare texcoord
    Depth.xy = oPos.zw;
}

void PS_Shadow( float2 Depth : TEXCOORD0,
                out float4 Color : COLOR )
{
    // Depth is z / w
    Color = Depth.x / Depth.y;
}

// ----------------------------------
// Scene generation:  Ambient light * texture color
// ----------------------------------

struct VS_AMB_OUTPUT
{
    float4 Position   : POSITION;   // vertex position 
    float4 Diffuse    : COLOR0;     // vertex diffuse color (note that COLOR0 is clamped from 0..1)
    float2 TextureUV  : TEXCOORD0;  // vertex texture coords 
};

VS_AMB_OUTPUT RenderSceneAmbVS(
		float4 vPos : POSITION, 
		float3 vNormal : NORMAL,
		float2 vTexCoord0 : TEXCOORD0 )
{
	VS_AMB_OUTPUT Output;

	// Transform the position from object space to homogeneous projection space
    Output.Position = mul( vPos, g_mWorldView );
    Output.Position = mul( Output.Position, g_mProj );

	// Diffuse color is the object's material color multiplied by the ambient light
	Output.Diffuse = g_vMaterial * g_vLightAmbient;

	// Just copy the texture coordinate through
	Output.TextureUV = vTexCoord0; 

	return Output;    
}

struct PS_AMB_OUTPUT
{
	float4 RGBColor : COLOR0;  // Pixel color    
};

PS_AMB_OUTPUT RenderSceneAmbPS( VS_AMB_OUTPUT In ) : COLOR
{ 
	PS_AMB_OUTPUT Output;

	// Lookup mesh texture and output it
	Output.RGBColor = tex2D(ColorSampler, In.TextureUV) * In.Diffuse;
	Output.RGBColor.a = 1.0;

	return Output;
}

// ----------------------------------
// Scene generation:  spotlight projected, shadow-mapped pixels
// ----------------------------------

//-----------------------------------------------------------------------------
// Vertex Shader: VertScene
// Desc: Process vertex for scene
//-----------------------------------------------------------------------------
void VS_Scene( float4 iPos : POSITION,
                float3 iNormal : NORMAL,
                float2 iTex : TEXCOORD0,
                out float4 oPos : POSITION,
                out float2 Tex : TEXCOORD0,
                out float4 vPos : TEXCOORD1,
                out float3 vNormal : TEXCOORD2,
                out float4 vPosLight : TEXCOORD3,
                out float3 vEye : TEXCOORD4 )
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

    // calculate vector from this vertex to eye in view space
    vEye = g_vEyePos.xyz - vPos.xyz;
}


//-----------------------------------------------------------------------------
// Pixel Shader: Scene
// Desc: Spotlight Diffuse + Specular with shadow test
//-----------------------------------------------------------------------------
float4 PS_Scene( float2 Tex : TEXCOORD0,
                 float4 vPos : TEXCOORD1,
                 float3 vNormal : TEXCOORD2,
                 float4 vPosLight : TEXCOORD3,
                 float3 vEye : TEXCOORD4 ) : COLOR
{
    float4 FinalLight = {0.0f, 0.0f, 0.0f, 0.0f};

    // vLight is the unit vector from the light to this pixel
    float vLightDist = length(vPos - g_vLightPos);

    float3 vLight = normalize( float3( vPos - g_vLightPos ) );

    // Compute diffuse from the light
    if( dot( vLight, g_vLightDir ) > g_fCosTheta ) // Light must face the pixel (within Theta)
    {
        // Pixel is in lit area. Find out if it's in shadow

        //transform from RT space to texture space.
        float2 ShadowTexC = 0.5 * vPosLight.xy / vPosLight.w + float2( 0.5, 0.5 );
        ShadowTexC.y = 1.0f - ShadowTexC.y;

        // transform to texel space
        // float2 texelpos = SMAP_SIZE * ShadowTexC;

		// see if we are in shadow
		float LightAmount = (tex2D( ShadowSampler, ShadowTexC ) + SHADOW_EPSILON < vPosLight.z / vPosLight.w) ? 0.0f : 1.0f;

		// enable for bump-mapping (detail normal map modification)
		// vNormal += 2.0f * tex2D( NormalSampler, Tex*32.0f ).rgb - 1.0f;
		vNormal = normalize( vNormal );

		// calculate diffuse contribution
		vEye = normalize(vEye);
		vLight = -vLight;
		float diff = max( dot( vNormal, vLight ), 0 );

		// calculate specular contribution
		float spec = pow( max( dot( 2 * diff * vNormal - vLight, vEye ), 0 ), 32 );

		// get source light contribution
        float4 gobo = tex2D(SpotSampler, ShadowTexC);

		// calculate attenuation
		float attenuation = 1.0f / (1.0f + g_fLinearAttenuation * vLightDist + g_fQuadraticAttenuation * vLightDist * vLightDist);

        // calculate final pixel light
        FinalLight = gobo * (diff + spec) * attenuation * LightAmount * g_vMaterial * g_vLightColor;
    }

	// modulate final pixel light by destination texture map
    return tex2D( ColorSampler, Tex ) * FinalLight;
}

//-----------------------------------------------------------------------------
// Pixel Shader: SceneNoShadow
// Desc: Spotlight Diffuse + Specular, no shadow testing
//-----------------------------------------------------------------------------
float4 PS_SceneNoShadow( float2 Tex : TEXCOORD0,
                 float4 vPos : TEXCOORD1,
                 float3 vNormal : TEXCOORD2,
                 float4 vPosLight : TEXCOORD3,
                 float3 vEye : TEXCOORD4 ) : COLOR
{
    float4 FinalLight = {0.0f, 0.0f, 0.0f, 0.0f};

    // vLight is the unit vector from the light to this pixel
    float vLightDist = length(vPos - g_vLightPos);

    float3 vLight = normalize( float3( vPos - g_vLightPos ) );

    // Compute diffuse from the light
    if( dot( vLight, g_vLightDir ) > g_fCosTheta ) // Light must face the pixel (within Theta)
    {
		// enable for bump-mapping (detail normal map modification)
		// vNormal += 2.0f * tex2D( NormalSampler, Tex*32.0f ).rgb - 1.0f;
		vNormal = normalize( vNormal );

		// calculate diffuse contribution
		vEye = normalize(vEye);
		vLight = -vLight;
		float diff = max( dot( vNormal, vLight ), 0 );

		// calculate specular contribution
		float spec = pow( max( dot( 2 * diff * vNormal - vLight, vEye ), 0 ), 32 );

        //transform from RT space to texture space.
        float2 ShadowTexC = 0.5 * vPosLight.xy / vPosLight.w + float2( 0.5, 0.5 );
        ShadowTexC.y = 1.0f - ShadowTexC.y;

		// get source light contribution
        float4 gobo = tex2D(SpotSampler, ShadowTexC);

		// calculate attenuation
		float attenuation = 1.0f / (1.0f + g_fLinearAttenuation * vLightDist + g_fQuadraticAttenuation * vLightDist * vLightDist);

        // calculate final pixel light
        FinalLight = gobo * (diff + spec) * attenuation * tex2D( ColorSampler, Tex ) * g_vMaterial * g_vLightColor;
    }

    return FinalLight;
}

//-----------------------------------------------------------------------------
// Vertex Shader: LightBeam
// Desc: Process vertex for light beam effect
//-----------------------------------------------------------------------------
void VS_SceneLightBeam( float4 iPos : POSITION,
                float3 iNormal : NORMAL,
                float4 Color : COLOR,
                float2 iTex : TEXCOORD0,
                out float4 oPos : POSITION,
                out float4 oColor : COLOR0,
                out float2 Tex : TEXCOORD0,
                out float4 vPos : TEXCOORD1,
                out float4 vPosLight : TEXCOORD3)
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
    // Propagate texture coord
    //
    Tex = iTex;

	//
	// Propagate diffuse color
	//
	oColor = Color;

    //
    // Transform the position to light projection space, or the
    // projection space as if the camera is looking out from
    // the spotlight.
    //
    vPosLight = mul( vPos, g_mViewToLightProj );
}

//-----------------------------------------------------------------------------
// Pixel Shader: SceneLightBeam
// Desc: Light Beam effect with shadow, color, & attenuation applied
//-----------------------------------------------------------------------------
float4 PS_SceneLightBeam(float4 Color : COLOR0,
						 float2 Tex : TEXCOORD0,
						 float4 vPos : TEXCOORD1,
						 float4 vPosLight : TEXCOORD3) : COLOR
{
    float4 FinalLight = {0.0f, 0.0f, 0.0f, 0.0f};

    // vLight is the unit vector from the light to this pixel
    float vLightDist = length(vPos - g_vLightPos);

    // Find out if it's in shadow

    //transform from RT space to texture space.
    float2 ShadowTexC = 0.5 * vPosLight.xy / vPosLight.w + float2( 0.5, 0.5 );
    ShadowTexC.y = 1.0f - ShadowTexC.y;

	// see if we are in shadow
	float LightAmount = (tex2D( ShadowSampler, ShadowTexC ) + SHADOW_EPSILON < vPosLight.z / vPosLight.w) ? 0.0f : 1.0f;

	// calculate attenuation
	float attenuation = 1.0f / (1.0f + g_fLinearAttenuation * vLightDist + g_fQuadraticAttenuation * vLightDist * vLightDist);

	// get source light's color along this beam
    float4 gobo = tex2D(SpotSampler, ShadowTexC);
    gobo.w = max(max(gobo.x, gobo.y), gobo.z);

    // calculate final pixel light
    FinalLight = gobo * Color * attenuation * LightAmount;

	// modulate final pixel light by beam texture map
    return tex2D( ColorSampler, Tex ) * FinalLight;
}

//--------------------------------------------
// Techniques
//--------------------------------------------

technique ShadowMapGen
{
	pass p0
	{
		Lighting	= False;
		CullMode	= CCW;
		
		VertexShader = compile vs_2_0 VS_Shadow();
		PixelShader  = compile ps_2_0 PS_Shadow();
	}
}

technique SpotLightAdd
{
	pass p0
	{
        // enable alpha blending
        AlphaBlendEnable = TRUE;

        // enable additive blending
        SrcBlend         = ONE;
        DestBlend        = ONE;

		Lighting	= False;
		CullMode	= CCW;

		VertexShader = compile vs_2_0 VS_Scene();
		PixelShader  = compile ps_2_0 PS_Scene();
	}
}

technique SpotLightAddNoShadow
{
	pass p0
	{
        // enable alpha blending
        AlphaBlendEnable = TRUE;

        // enable additive blending
        SrcBlend         = ONE;
        DestBlend        = ONE;

		Lighting	= False;
		CullMode	= CCW;

		VertexShader = compile vs_2_0 VS_Scene();
		PixelShader  = compile ps_2_0 PS_SceneNoShadow();
	}
}

technique SpotLightBeam
{
	pass p0
	{
        // enable alpha blending
        AlphaBlendEnable = TRUE;

        // enable additive blending
        SrcBlend         = SRCALPHA;
        DestBlend        = INVSRCALPHA;

		Lighting	= False;
		CullMode	= NONE;
		ZWriteEnable = False;

		VertexShader = compile vs_2_0 VS_SceneLightBeam();
		PixelShader  = compile ps_2_0 PS_SceneLightBeam();
	}
}

technique RenderSceneAmb
{
    pass P0
    {          
        VertexShader = compile vs_1_1 RenderSceneAmbVS();
        PixelShader  = compile ps_1_1 RenderSceneAmbPS();
    }
}
