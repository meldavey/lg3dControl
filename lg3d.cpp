#include "dxstdafx.h"
#include "dxutmesh.h"

#include "lg3d.h"
#include "lg3dDXSupport.h"

struct LG3DInternalLight {
	bool				lightMoved;			// set when light move detected
	double				hash;				// hash value used to detect when key control light values have changed
	LPDIRECT3DTEXTURE9	shadowMap;			// pointer to possible light shadow map texture
	D3DXMATRIXA16		worldMat;			// light's world-space representation
	D3DXMATRIXA16		viewMat;			// light's view-space representation
	D3DXMATRIXA16		projMat;			// Projection matrix for light & shadow map
    D3DXMATRIXA16		worldViewProj;		// world * view * projection
	D3DXVECTOR4			lightDir;			// direction vector for light
	LPDIRECT3DTEXTURE9	goboMap;			// texture map for gobo spotlight projections
	float				cosTheta;			// cosine of (umbra + penumbra)
	D3DXVECTOR4			color;				// source color of this light
	int					loopId;				// determines what loop to draw this light in, loop 0 is vertex-only lights, loop 1 is per-pixel gobo, loop 2 is per-pixel gobo + shadow
	LPDIRECT3DVERTEXBUFFER9 lightBeamVB;	// light beam effect
	int					numBeams;			// number of light beam primitives
};

struct LG3DInternalObject {
	CDXUTMesh			*mesh;				// .x mesh object
	D3DXMATRIXA16		matWorld;			// world transform matrix
    D3DXMATRIXA16		worldViewProj;		// world * view * projection
	double				hash;				// hash value used to detect when key control object values have changed
	bool				moved;				// set when object move detected
};

struct LG3DScene {
	ID3DXEffect			*effect;			// D3DX effect interface
	ID3DXEffect			*vertLightEffect;	// D3DX effect interface
	LG3DInternalLight	*light;				// list of internal light data
	LG3DInternalObject	*obj;				// list of internal object data
	LPDIRECT3DSURFACE9	shadowDepthStencil;	// Depth-stencil buffer for rendering to shadow map
	D3DCOLOR			clearColor;

	LG3DScene() {memset(this, 0, sizeof(LG3DScene));}
};

HRESULT hr;
float aspectRatio = 1.0f;
ID3DXFont*              g_pFont = NULL;         // Font for drawing text
ID3DXSprite*            g_pTextSprite = NULL;   // Sprite for batching draw text calls
// ID3DXMesh*				g_arrow = NULL;
CDXUTMesh				*g_lightCan1 = NULL;	// light can mesh
LPDIRECT3DTEXTURE9	g_pSpotMap = NULL;			// generic spotlight shape
LPDIRECT3DTEXTURE9 g_pNormMap = NULL;			// bump map texture
LPDIRECT3DTEXTURE9	g_pWhiteMap = NULL;			// default white texture
D3DXMATRIX		matView;						// current camera's view matrix
D3DXMATRIX		matProj;						// scene/camera projection matrix
D3DXVECTOR4		eyeDir;							// current camera (eye) direction

static LG3DControl *lg3dControl = NULL;
static bool wantManipulate = false;

HWND			globalParent;					// parent window used to pass up unused windows events

#define MAX_BASIC_LIGHTS 48						// number of vertex-only spotlights.  Can be much larger, up to 120 in one pass with separate effects file.

// texture map overlay declarations (used in debug to display shadow maps)
typedef struct
{
    D3DXVECTOR4 p;
    FLOAT       tu, tv;
} TVERTEX;
#define TVERTEX_FVF (D3DFVF_XYZRHW | /*D3DFVF_DIFFUSE | */ D3DFVF_TEX1)
LPDIRECT3DVERTEXBUFFER9	m_pOverlayVB = NULL;
#define OVERLAY_SIZE      128
LPDIRECT3DPIXELSHADER9 m_pShowMapPS = NULL;

// light beam special effect
#define LIGHTBEAM_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)
struct LightBeam {
	D3DXVECTOR3	v;
	DWORD		color;
	FLOAT       tu, tv;
};
LPDIRECT3DTEXTURE9	lightBeamTex = NULL;			// light beam shaping texture

LG3DControl::LG3DControl(HWND _parent, LG3DControlData *_controlData)
{
	// ---------------------------------------------------------
	// initialize member variables
	// ---------------------------------------------------------
	parent = _parent;
	globalParent = parent;
	controlData = _controlData;
	scene = new LG3DScene;
	shadowMapSize = 512; // this is a power of 2 tex map size, larger for better shadow resolution, probably don't want any smaller than 256

	manipObjId = -1;
	manipLightId = -1;
	rButtonDown = false;
	lButtonDown = false;
	lastMousePos.x = 0;
	lastMousePos.y = 0;

	scene->clearColor = D3DCOLOR_ARGB(0xff, controlData->clearColor.r, controlData->clearColor.g, controlData->clearColor.b);

	// ---------------------------------------------------------
	// create our render window - it will exactly match the
	// parent's window size, essentially sitting on top of it.
	// ---------------------------------------------------------
	CreateRenderWindow();
}

bool LG3DControl::Init()
{
	// ---------------------------------------------------------
	// create D3DX9 view
	// ---------------------------------------------------------

	// set up callback handlers
    DXUTSetCallbackDeviceCreated(LG3DOnCreateDevice, this);
    DXUTSetCallbackDeviceReset(LG3DOnResetDevice, this);
    DXUTSetCallbackDeviceLost(LG3DOnLostDevice, this);
    DXUTSetCallbackDeviceDestroyed(LG3DOnDestroyDevice, this);
    DXUTSetCallbackFrameRender(LG3DOnFrameRender, this);
    DXUTSetCallbackFrameMove(LG3DOnFrameMove, this);

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    DXUTInit( false, false, true ); // Parse the command line, handle the default hotkeys, and show msgboxes
    DXUTSetWindow(lg3dWnd, lg3dWnd, lg3dWnd, false); // focus wnd, windowed wnd, fullscreen wnd
    DXUTCreateDevice( D3DADAPTER_DEFAULT, true, width, height, IsDeviceAcceptable, ModifyDeviceSettings );

	lg3dControl = this;

	return true;
}

LG3DControl::~LG3DControl()
{
	lg3dControl = NULL;

	DXUTShutdown();

	DestroyWindow(lg3dWnd);

	delete scene;
}

#define APP_ESCAPE		0x1B

long FAR PASCAL LG3DWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	bool handled = false;

	switch(message) {
		case WM_CHAR:
			switch(wParam) {
				case APP_ESCAPE:
					PostQuitMessage(0);
					handled = true;
				break;

				case 'i':
					wantManipulate = !wantManipulate;
					handled = true;
				break;
			}
		break;

		case WM_PAINT:
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			handled = true;
		break;

		case WM_CLOSE:
			handled = true;
		break;

		case WM_DESTROY:
			// PostQuitMessage( 0 );
			handled = true;
		break;

		case WM_MOUSEMOVE:
			{
				SetFocus(hWnd); // so we can intercept keyboard commands

				if (wantManipulate && lg3dControl) {
					lg3dControl->MouseMove();
					handled = true;
				} else
					handled = false;
			}
		break;

		case WM_LBUTTONDOWN:
			if (wantManipulate && lg3dControl) {
				lg3dControl->LButtonDown();
				SetCapture(hWnd);
				handled = true;
			} else
				handled = false;
		break;

		case WM_LBUTTONUP:
			if (wantManipulate && lg3dControl) {
				lg3dControl->LButtonUp();
				ReleaseCapture();
				handled = true;
			} else
				handled = false;
		break;

		case WM_RBUTTONDOWN:
			if (wantManipulate && lg3dControl) {
				lg3dControl->RButtonDown();
				SetCapture(hWnd);
				handled = true;
			} else
				handled = false;
		break;

		case WM_RBUTTONUP:
			if (wantManipulate && lg3dControl) {
				lg3dControl->RButtonUp();
				ReleaseCapture();
				handled = true;
			} else
				handled = false;
		break;

		case WM_MBUTTONDOWN:
			handled = false;
		break;

		case WM_MBUTTONUP:
			handled = false;
		break;

		case WM_MOUSEWHEEL:
			{
				handled = false;
			}
		break;

		default:
			return (DefWindowProc(hWnd, message, wParam, lParam));
	}

	if (!handled)
		// return (DefWindowProc(hWnd, message, wParam, lParam));
		return SendMessage(globalParent, message, wParam, lParam);

	return handled;
}

void LG3DControl::CreateRenderWindow()
{
	WNDCLASS		wc;

    wc.style = 0; // CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = LG3DWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"LG3DControl";
    RegisterClass( &wc );

	RECT rect;
	GetClientRect(parent, &rect);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	DWORD m_dwWindowStyle = WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE;
	DWORD flags = 0; // these are the extended flags

	// Create window
    lg3dWnd = CreateWindowEx(
		flags,
		wc.lpszClassName,
		wc.lpszClassName,
		m_dwWindowStyle,
		0, 0,
		width, height,
		parent,
		NULL,
		wc.hInstance,
		NULL);
}

HRESULT CALLBACK LG3DControl::OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc )
{
	// ---------------------------------------------------------
	// load mesh models & textures
	// ---------------------------------------------------------

	D3DXCreateTextureFromFile( pd3dDevice, L".\\data\\white.bmp", &g_pWhiteMap );
	D3DXCreateTextureFromFile( pd3dDevice, L".\\data\\lightBeam5.dds", &lightBeamTex );

	scene->obj = (LG3DInternalObject *)malloc(sizeof(LG3DInternalObject) * controlData->numSceneObjects);

	int i;
	unsigned int j;
	for(i=0;i<controlData->numSceneObjects;i++) {
		scene->obj[i].mesh = new CDXUTMesh;
		scene->obj[i].mesh->Create(pd3dDevice, (LPCWSTR)controlData->sceneObjectList[i].meshName);
		for(j=0;j<scene->obj[i].mesh->m_dwNumMaterials;j++) {
			if (scene->obj[i].mesh->m_pTextures[j] == NULL)
				scene->obj[i].mesh->m_pTextures[j] = g_pWhiteMap;
		}
		// force matrix update on first render
		scene->obj[i].moved = true;
	}

	// DXUTCreateArrowMeshFromInternalArray( pd3dDevice, &g_arrow );
	g_lightCan1 = new CDXUTMesh();
	g_lightCan1->Create(pd3dDevice, L"data/lightCan.x");
	for(j=0;j<g_lightCan1->m_dwNumMaterials;j++) {
		if (g_lightCan1->m_pTextures[j] == NULL)
			g_lightCan1->m_pTextures[j] = g_pWhiteMap;
	}

	// ---------------------------------------------------------
    // Initialize the font
	// ---------------------------------------------------------
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                         OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                         L"Arial", &g_pFont ) );

	// ---------------------------------------------------------
	// load up default spotlight projection map
	// ---------------------------------------------------------
	// D3DXCreateTextureFromFile( pd3dDevice, L".\\data\\SpotMap.dds", &g_pSpotMap );
	D3DXCreateTextureFromFile( pd3dDevice, L".\\data\\spotlight1.bmp", &g_pSpotMap );
	D3DXCreateTextureFromFile( pd3dDevice, L".\\data\\NormTest.dds", &g_pNormMap );

	// ---------------------------------------------------------
	// allocate space for shadow map list and each required shadow map
	// ---------------------------------------------------------
	scene->light = (LG3DInternalLight *)malloc(sizeof(LG3DInternalLight) * controlData->numSceneLights);
	memset(scene->light, 0, sizeof(LG3DInternalLight) * controlData->numSceneLights);
	for(i=0;i<controlData->numSceneLights;i++) {
		// if light requires a shadow map
		if (controlData->sceneLightList[i].castsShadows) {
			V_RETURN( pd3dDevice->CreateTexture( shadowMapSize, shadowMapSize,
												 1, D3DUSAGE_RENDERTARGET,
												 D3DFMT_R32F,
												 D3DPOOL_DEFAULT,
												 &scene->light[i].shadowMap,
												 NULL ) );
		}

		// force shadow map update on first render
		scene->light[i].lightMoved = true;

		// if gobo specified, load up that file
		if (controlData->sceneLightList[i].goboName[0] != 0)
			D3DXCreateTextureFromFile( pd3dDevice, controlData->sceneLightList[i].goboName, &scene->light[i].goboMap );
		else {
			// we assume its a spotlight, and point to the default spotlight shape
			// scene->light[i].goboMap = g_pSpotMap;

			// create a texture with which to model the requested umbra & penumbra values
			// D3DXCreateTexture(pd3dDevice, 256, 256, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &scene->light[i].goboMap);
			pd3dDevice->CreateTexture(256, 256, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &scene->light[i].goboMap, NULL);
			D3DSURFACE_DESC desc;
			scene->light[i].goboMap->GetLevelDesc(0, &desc);

			// generate the umbra & penumbra portions of the texture
			D3DLOCKED_RECT texRect;
			scene->light[i].goboMap->LockRect(0, &texRect, NULL, 0);
			int x, y;
			float umbra = controlData->sceneLightList[i].umbra;
			float penumbra = controlData->sceneLightList[i].penumbra;
			float totalAngle = umbra + penumbra;
			float centerX = 127.5f, centerY = 127.5f;
			float penumbraAmount;
			unsigned long penumbraVal;
			float dist;
			float pixAngle;
			float dx, dy;
			unsigned char *texPix = (unsigned char *)texRect.pBits;
			for(y=0;y<256;y++) {
				dy = centerY - y;
				for(x=0;x<256;x++) {
					dx = centerX - x;
					dist = sqrtf(dx*dx+dy*dy);
					pixAngle = totalAngle * dist / 127.0f;
					if (pixAngle > totalAngle) { // no light in this region
						*(DWORD *)(texPix+x*4+y*texRect.Pitch) = 0;
					} else if (pixAngle > umbra) { // this is the penumbra outer edge region
						penumbraAmount = 1.0f - (pixAngle-umbra)/penumbra;
						penumbraAmount = powf(penumbraAmount, 0.5f);  // the penumbra falloff is just a square root function, but we could make the 0.5 a parameter per spotlight if we wanted
						penumbraVal = (unsigned char)(penumbraAmount*255);
						*(DWORD *)(texPix+x*4+y*texRect.Pitch) = (penumbraVal<<24) | (penumbraVal<<16) | (penumbraVal<<8) | penumbraVal;
					} else { // this is the dolid light area of the umbra
						*(DWORD *)(texPix+x*4+y*texRect.Pitch) = 0xffffffff;
					}
				}
			}
			scene->light[i].goboMap->UnlockRect(0);
		}

		// determine in which loop this light should contribute to the scene
		if ((controlData->sceneLightList[i].goboName[0] == 0) && !controlData->sceneLightList[i].castsShadows)
			scene->light[i].loopId = 0;
		else if ((controlData->sceneLightList[i].goboName[0] != 0) && !controlData->sceneLightList[i].castsShadows)
			scene->light[i].loopId = 1;
		else
			scene->light[i].loopId = 2;

		// set source color of this light
		scene->light[i].color = D3DXVECTOR4(controlData->sceneLightList[i].color.r/255.0f, controlData->sceneLightList[i].color.g/255.0f, controlData->sceneLightList[i].color.b/255.0f, 1.0f);

// #define LIGHT_BEAM_METHOD_1
#define LIGHT_BEAM_METHOD_2
		// create the light beam effect vertex buffer
		// if (controlData->sceneLightList[i].goboName[0] == 0) {
		if (1) {
#if defined(LIGHT_BEAM_METHOD_1)
			scene->light[i].numBeams = 7; // more beams yield a better volume at the cost of a performance hit.  11 is probably a good max #, and 1 would be the minimum
			pd3dDevice->CreateVertexBuffer(scene->light[i].numBeams * 3 * sizeof(LightBeam), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &scene->light[i].lightBeamVB, NULL);
			LightBeam *lightBeam;
			scene->light[i].lightBeamVB->Lock(0, 0, (void**)&lightBeam, 0);
			const float beamDist = 10.0f; // max dist beam projects
			const float r = beamDist * sinf(DEG2RADf(controlData->sceneLightList[i].umbra + controlData->sceneLightList[i].penumbra)*0.5f);

			int lbi;
			for (lbi=0;lbi<scene->light[i].numBeams;lbi++) {
				lightBeam[lbi*3+0].v = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
				lightBeam[lbi*3+0].color = D3DCOLOR_ARGB(0x30, controlData->sceneLightList[i].color.r, controlData->sceneLightList[i].color.g, controlData->sceneLightList[i].color.b);
				lightBeam[lbi*3+0].tu = 0.5f;
				lightBeam[lbi*3+0].tv = 0.0f;

				// x*x + y*y = r*r; where r == 5.0f
				// 1.8f is a factor determining how near the top/bottom the beginning/ending ray will be.
				// 2.0f would yield exactly the top/bottom, 1.0f would yield 3/4 to the top/bottom.
				float t = 1.8f * (lbi-scene->light[i].numBeams/2)/(float)(scene->light[i].numBeams-1);
				float y1 = t * r;
				float x1 = sqrtf(r*r - y1*y1);
				float y2 = t * -r;
				float x2 = -x1;

				lightBeam[lbi*3+1].v = D3DXVECTOR3(x1, y1, beamDist);
				lightBeam[lbi*3+1].color = D3DCOLOR_ARGB(0x00, controlData->sceneLightList[i].color.r, controlData->sceneLightList[i].color.g, controlData->sceneLightList[i].color.b);
				lightBeam[lbi*3+1].tu = 0.0f;
				lightBeam[lbi*3+1].tv = 1.0f;

				lightBeam[lbi*3+2].v = D3DXVECTOR3(x2, y2, beamDist);
				lightBeam[lbi*3+2].color = D3DCOLOR_ARGB(0x00, controlData->sceneLightList[i].color.r, controlData->sceneLightList[i].color.g, controlData->sceneLightList[i].color.b);
				lightBeam[lbi*3+2].tu = 1.0f;
				lightBeam[lbi*3+2].tv = 1.0f;
			}
#elif defined(LIGHT_BEAM_METHOD_2)
			scene->light[i].numBeams = 14; // number of horizontal plus vertical slices
			pd3dDevice->CreateVertexBuffer(scene->light[i].numBeams * 3 * sizeof(LightBeam), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &scene->light[i].lightBeamVB, NULL);
			LightBeam *lightBeam;
			scene->light[i].lightBeamVB->Lock(0, 0, (void**)&lightBeam, 0);
			const float beamDist = 10.0f; // max dist beam projects
			const float r = beamDist * sinf(DEG2RADf(controlData->sceneLightList[i].umbra + controlData->sceneLightList[i].penumbra)*0.5f);

			int lbi;
			for (lbi=0;lbi<scene->light[i].numBeams;lbi+=2) {
				lightBeam[lbi*3+0].v = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
				lightBeam[lbi*3+0].color = D3DCOLOR_ARGB(0x20, controlData->sceneLightList[i].color.r, controlData->sceneLightList[i].color.g, controlData->sceneLightList[i].color.b);
				lightBeam[lbi*3+0].tu = 0.5f;
				lightBeam[lbi*3+0].tv = 0.0f;

				// x*x + y*y = r*r; where r == 5.0f
				// 1.8f is a factor determining how near the top/bottom the beginning/ending ray will be.
				// 2.0f would yield exactly the top/bottom, 1.0f would yield 3/4 to the top/bottom.
				float t = 1.8f * (lbi/2-scene->light[i].numBeams/4)/(float)(scene->light[i].numBeams/2-1);
				float y1 = t * r;
				float x1 = sqrtf(r*r - y1*y1);
				// horizontal slices
				float y2 = y1;
				float x2 = -x1;

				lightBeam[lbi*3+1].v = D3DXVECTOR3(x1, y1, beamDist);
				lightBeam[lbi*3+1].color = D3DCOLOR_ARGB(0x00, controlData->sceneLightList[i].color.r, controlData->sceneLightList[i].color.g, controlData->sceneLightList[i].color.b);
				lightBeam[lbi*3+1].tu = 0.0f;
				lightBeam[lbi*3+1].tv = 1.0f;

				lightBeam[lbi*3+2].v = D3DXVECTOR3(x2, y2, beamDist);
				lightBeam[lbi*3+2].color = D3DCOLOR_ARGB(0x00, controlData->sceneLightList[i].color.r, controlData->sceneLightList[i].color.g, controlData->sceneLightList[i].color.b);
				lightBeam[lbi*3+2].tu = 1.0f;
				lightBeam[lbi*3+2].tv = 1.0f;

				lightBeam[lbi*3+3] = lightBeam[lbi*3+0];
				lightBeam[lbi*3+4] = lightBeam[lbi*3+1];
				lightBeam[lbi*3+5] = lightBeam[lbi*3+2];

				// vertical slices
				x1 = t * r;
				y1 = sqrtf(r*r - x1*x1);
				x2 = x1;
				y2 = -y1;
				lightBeam[lbi*3+4].v = D3DXVECTOR3(x1, y1, beamDist);
				lightBeam[lbi*3+5].v = D3DXVECTOR3(x2, y2, beamDist);
			}
#endif /* LIGHT_BEAM_METHOD_X */

			scene->light[i].lightBeamVB->Unlock();
		} else {
			scene->light[i].lightBeamVB = NULL;
			scene->light[i].numBeams = 0;
		}
	}

	// ---------------------------------------------------------
	// load the fx file (our vertex & pixel shaders)
	// ---------------------------------------------------------

	DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;
#ifdef DEBUG_VS
    dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
#endif
#ifdef DEBUG_PS
    dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
#endif

    // Preshaders are parts of the shader that the effect system pulls out of the 
    // shader and runs on the host CPU. They should be used if you are GPU limited. 
    // The D3DXSHADER_NO_PRESHADER flag disables preshaders.
	bool enablePreshader = false;
    if(!enablePreshader)
        dwShaderFlags |= D3DXSHADER_NO_PRESHADER;

	V_RETURN( D3DXCreateEffectFromFile(pd3dDevice, L".\\data\\lg3d_core.fx", NULL, NULL, dwShaderFlags, NULL, &scene->effect, NULL ) );
	V_RETURN( D3DXCreateEffectFromFile(pd3dDevice, L".\\data\\lg3d_vertLight.fx", NULL, NULL, dwShaderFlags, NULL, &scene->vertLightEffect, NULL ) );

	// Create overlay VB (used to display shadow maps in debug)
    TVERTEX* pDstT;
	pd3dDevice->CreateVertexBuffer(4 * sizeof(TVERTEX), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &m_pOverlayVB, NULL);
	m_pOverlayVB->Lock(0, 0, (void**)&pDstT, 0);
	pDstT[0].p = D3DXVECTOR4(4.5f, 4.5f + OVERLAY_SIZE, 0.0f, 1.0f);
	// pDstT[0].diffuse = 0xffffffff;
	pDstT[0].tu = 0.0f;
	pDstT[0].tv = 1.0f;
	pDstT[1].p = D3DXVECTOR4(4.5f + OVERLAY_SIZE, 4.5f + OVERLAY_SIZE, 0.0f, 1.0f);
	// pDstT[1].diffuse = 0xffffffff;
	pDstT[1].tu = 1.0f;
	pDstT[1].tv = 1.0f;
	pDstT[2].p = D3DXVECTOR4(4.5f, 4.5f, 0.0f, 1.0f);
	// pDstT[2].diffuse = 0xffffffff;
	pDstT[2].tu = 0.0f;
	pDstT[2].tv = 0.0f;
	pDstT[3].p = D3DXVECTOR4(4.5f + OVERLAY_SIZE, 4.5f, 0.0f, 1.0f);
	// pDstT[3].diffuse = 0xffffffff;
	pDstT[3].tu = 1.0f;
	pDstT[3].tv = 0.0f;
	m_pOverlayVB->Unlock();

    return S_OK;
}

HRESULT CALLBACK LG3DControl::OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc )
{
    if( scene->effect )
        V_RETURN( scene->effect->OnResetDevice() );
	if (scene->vertLightEffect)
		V_RETURN(scene->vertLightEffect->OnResetDevice());
    // if( scene->shadowMapFx )
        // V_RETURN( scene->shadowMapFx->OnResetDevice() );
    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );

    aspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

    // Create the depth-stencil buffer to be used with the shadow map
    // We do this to ensure that the depth-stencil buffer is large
    // enough and has correct multisample type/quality when rendering
    // the shadow map.  The default depth-stencil buffer created during
    // device creation will not be large enough if the user resizes the
    // window to a very small size.  Furthermore, if the device is created
    // with multisampling, the default depth-stencil buffer will not
    // work with the shadow map texture because texture render targets
    // do not support multisample.
    DXUTDeviceSettings d3dSettings = DXUTGetDeviceSettings();
    V_RETURN( pd3dDevice->CreateDepthStencilSurface( shadowMapSize,
                                                     shadowMapSize,
                                                     d3dSettings.pp.AutoDepthStencilFormat,
                                                     D3DMULTISAMPLE_NONE,
                                                     0,
                                                     TRUE,
                                                     &scene->shadowDepthStencil,
                                                     NULL ) );

	SAFE_RELEASE(m_pShowMapPS);
	LPD3DXBUFFER pShaderBuf = NULL;
    LPD3DXBUFFER pErrorBuf = NULL;
    HRESULT hr = D3DXAssembleShaderFromFile(L"data\\showmap.psh", NULL, NULL, 0, &pShaderBuf, &pErrorBuf);
	if (SUCCEEDED(hr)) {
		if (pShaderBuf) 
			hr = pd3dDevice->CreatePixelShader((DWORD*)pShaderBuf->GetBufferPointer(), &m_pShowMapPS);
		else
			hr = E_FAIL;
	}
	SAFE_RELEASE(pShaderBuf);
	SAFE_RELEASE(pErrorBuf);

    return hr;
}

void CALLBACK LG3DControl::OnLostDevice( )
{
    if( scene->effect )
        scene->effect->OnLostDevice();
    if( scene->vertLightEffect )
        scene->vertLightEffect->OnLostDevice();
	// if( scene->shadowMapFx )
        // scene->shadowMapFx->OnLostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    SAFE_RELEASE( g_pTextSprite );

	int light;
	for(light=0;light<controlData->numSceneLights;light++) {
		SAFE_RELEASE( scene->light[light].shadowMap );
	}

	SAFE_RELEASE( scene->shadowDepthStencil );
}

void CALLBACK LG3DControl::OnDestroyDevice( )
{
	int i;
	unsigned int j;
	for(i=0;i<controlData->numSceneObjects;i++) {
		for(j=0;j<scene->obj[i].mesh->m_dwNumMaterials;j++) {
			if (scene->obj[i].mesh->m_pTextures[j] == g_pWhiteMap)
				scene->obj[i].mesh->m_pTextures[j] = NULL;
		}
		delete scene->obj[i].mesh;
	}
	free(scene->obj);
    SAFE_RELEASE(scene->effect);
    SAFE_RELEASE(scene->vertLightEffect);
    // SAFE_RELEASE(scene->shadowMapFx);
    SAFE_RELEASE( g_pFont );
	// SAFE_RELEASE(g_arrow);
	for(j=0;j<g_lightCan1->m_dwNumMaterials;j++) {
		if (g_lightCan1->m_pTextures[j] == g_pWhiteMap)
			g_lightCan1->m_pTextures[j] = NULL;
	}
	delete g_lightCan1;

	int light;
	for(light=0;light<controlData->numSceneLights;light++) {
		SAFE_RELEASE( scene->light[light].shadowMap );
		if (scene->light[light].goboMap != g_pSpotMap)
			SAFE_RELEASE(scene->light[light].goboMap);
		SAFE_RELEASE(scene->light[light].lightBeamVB);
	}
	free(scene->light);

    SAFE_RELEASE(m_pOverlayVB);
	SAFE_RELEASE(m_pShowMapPS);
	SAFE_RELEASE( g_pSpotMap );
	SAFE_RELEASE(g_pNormMap);
	SAFE_RELEASE(g_pWhiteMap);
	SAFE_RELEASE(lightBeamTex);
}

void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats(true) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
	txtHelper.DrawTextLine(wantManipulate ? L"Manipulation Mode: Object/light" : L"Manipulation Mode: Camera");
    txtHelper.End();
}

static double GenerateHash(LG3DSceneLight *light)
{
	double hash =
		light->position.x + light->position.y + light->position.z +
		light->orientation.h + light->orientation.p + light->orientation.r +
		light->penumbra + light->umbra;
	return hash;
}

static double GenerateHash(LG3DSceneObject *obj)
{
	double hash =
		obj->position.x + obj->position.y + obj->position.z +
		obj->orientation.h + obj->orientation.p + obj->orientation.r;
	return hash;
}

void CALLBACK LG3DControl::OnFrameMove( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
	// update hash values for lights
	int i;
	double hash;
	for(i=0;i<controlData->numSceneLights;i++) {
		hash = GenerateHash(&controlData->sceneLightList[i]);
		if (hash != scene->light[i].hash) {
			scene->light[i].hash = hash;
			scene->light[i].lightMoved = true;
		}
	}

	// update hash values for objects
	for(i=0;i<controlData->numSceneObjects;i++) {
		hash = GenerateHash(&controlData->sceneObjectList[i]);
		if (hash != scene->obj[i].hash) {
			scene->obj[i].hash = hash;
			scene->obj[i].moved = true;
		}
	}

	// ---------------------------------------------------------
	// see if any lights have moved (hash values different)
	// and if so, recalculate visible object list and various
	// light parameters
	// ---------------------------------------------------------
	for(i=0;i<controlData->numSceneLights;i++) {
		if (scene->light[i].lightMoved) {
			// calculate light's world, view, and projection matrices

			// first just set up the light's rotational portion of the world matrix
			// we will then use this to get our view direction for the lookat view matrix calcs
			// then we can apply the translation to the world matrix
			D3DXMATRIXA16 mHead, mPitch;
			D3DXMatrixRotationY(&mHead, DEG2RADf(controlData->sceneLightList[i].orientation.h)); // heading
			D3DXMatrixRotationX(&mPitch, -DEG2RADf(controlData->sceneLightList[i].orientation.p)); // pitch
			scene->light[i].worldMat = mPitch * mHead;

			// now rotate the look vector by the light's rotations
			D3DXVECTOR3 lookVec(0.0f, 0.0f, 1.0f); // +Z is into the screen
			D3DXVec3Transform(&scene->light[i].lightDir, &lookVec, &scene->light[i].worldMat);
			D3DXVECTOR3 vDir3(scene->light[i].lightDir.x, scene->light[i].lightDir.y, scene->light[i].lightDir.z);
			// vDir3 should still be normalized, so noneed to renormalize it

			// and setup the view matrix
			D3DXVECTOR3 vEyePt = D3DXVECTOR3(
				controlData->sceneLightList[i].position.x,
				controlData->sceneLightList[i].position.z,
				controlData->sceneLightList[i].position.y);
			D3DXVECTOR3 vLookatPt = vEyePt + vDir3; // lookat point is just our light position plus the light direction
			D3DXVECTOR3 vUpVec(0,1,0); // Y up
			D3DXMatrixLookAtLH(&scene->light[i].viewMat, &vEyePt, &vLookatPt, &vUpVec);

			// now add in the translation to complete the world matrix
			scene->light[i].worldMat._41 = controlData->sceneLightList[i].position.x;
			scene->light[i].worldMat._42 = controlData->sceneLightList[i].position.z;
			scene->light[i].worldMat._43 = controlData->sceneLightList[i].position.y;

			// calculate the projection
			D3DXMatrixPerspectiveFovLH( &scene->light[i].projMat, DEG2RADf(controlData->sceneLightList[i].umbra+controlData->sceneLightList[i].penumbra), 1.0f, 0.01f, 100.0f);

			// notice here the world matrix is omitted due to the fact that the view matrix contains all the info needed
			scene->light[i].worldViewProj = /*scene->light[i].worldMat * */ scene->light[i].viewMat * scene->light[i].projMat;

			// set the cosine of (umbra + penumbra)
			scene->light[i].cosTheta = cosf(DEG2RADf(controlData->sceneLightList[i].umbra+controlData->sceneLightList[i].penumbra));
		}
	}

	//
	// Camera space matrices
	//

	// Compute the world matrix
	// D3DXMATRIX matWorld;
	// D3DXMatrixIdentity( &matWorld );

	// Compute the view matrix
	D3DXMATRIXA16 mHead, mPitch, mOrientation;
	D3DXMatrixRotationY(&mHead, DEG2RADf(controlData->cameraList[controlData->curCamera].orientation.h)); // heading
	D3DXMatrixRotationX(&mPitch, -DEG2RADf(controlData->cameraList[controlData->curCamera].orientation.p)); // pitch
	mOrientation = mPitch * mHead;
	D3DXVECTOR3 lookVec(0.0f, 0.0f, 1.0f); // +Z is into the screen
	D3DXVECTOR4 vDir4;
	D3DXVec3Transform(&vDir4, &lookVec, &mOrientation);
	eyeDir = vDir4;

	D3DXVECTOR3 vEyePt = D3DXVECTOR3(controlData->cameraList[controlData->curCamera].position.x, controlData->cameraList[controlData->curCamera].position.z, controlData->cameraList[controlData->curCamera].position.y);
	D3DXVECTOR3 vLookatPt = vEyePt + (D3DXVECTOR3)vDir4;
	D3DXVECTOR3 vUpVec(0,1,0);
	D3DXMatrixLookAtLH(&matView, &vEyePt, &vLookatPt, &vUpVec);

	// Compute the projection matrix
	D3DXMatrixPerspectiveFovLH( &matProj, D3DXToRadian(controlData->cameraList[controlData->curCamera].fov), aspectRatio, 0.1f, 100.0f );

	// compute matrices for each object in the scene
	for(i=0;i<controlData->numSceneObjects;i++) {
		if (scene->obj[i].moved) {
			D3DXMATRIXA16 mHead, mPitch, mRoll, mWorld;
			D3DXMatrixRotationY(&mHead, DEG2RADf(controlData->sceneObjectList[i].orientation.h)); // heading
			D3DXMatrixRotationX(&mPitch, -DEG2RADf(controlData->sceneObjectList[i].orientation.p)); // pitch
			D3DXMatrixRotationZ(&mRoll, DEG2RADf(controlData->sceneObjectList[i].orientation.r)); // roll
			D3DXMatrixTranslation(&mWorld, controlData->sceneObjectList[i].position.x, controlData->sceneObjectList[i].position.z, controlData->sceneObjectList[i].position.y);

			scene->obj[i].matWorld = mWorld * mRoll * mPitch * mHead;

			scene->obj[i].worldViewProj = scene->obj[i].matWorld * matView * matProj;
		}
	}
}

void LG3DControl::UpdateShadowMaps(IDirect3DDevice9 *pd3dDevice)
{
	// NOTES:  Need to optimize by only setting the shadowDepthStencil buffer once, even when doing multiple light updates
	// NOTES:  Code cleanups:  Can probably move a lot of scene geometry rendering & world view projection matrix setup into common call

	int i;
	for(i=0;i<controlData->numSceneLights;i++) {
		if (scene->light[i].shadowMap && scene->light[i].lightMoved && controlData->sceneLightList[i].enabled) {
			LPDIRECT3DSURFACE9 pOldRT = NULL;
			V( pd3dDevice->GetRenderTarget( 0, &pOldRT ) );
			LPDIRECT3DSURFACE9 pShadowSurf;
			if( SUCCEEDED( scene->light[i].shadowMap->GetSurfaceLevel( 0, &pShadowSurf ) ) ) {
				pd3dDevice->SetRenderTarget( 0, pShadowSurf );
				SAFE_RELEASE( pShadowSurf );
			}
			LPDIRECT3DSURFACE9 pOldDS = NULL;
			if( SUCCEEDED( pd3dDevice->GetDepthStencilSurface( &pOldDS ) ) )
				pd3dDevice->SetDepthStencilSurface( scene->shadowDepthStencil );

			// just render all geometry here
			// NOTES:  Can optimize here by performing visibility culling with basic bounding sphere tests
			{
				V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 0xffffffff, 1.0f, 0L ) );

				V( scene->effect->SetTechnique( "ShadowMapGen" ) );
				scene->effect->SetMatrix( "g_mProj", &scene->light[i].projMat );

				UINT iPass, cPasses;
				V( scene->effect->Begin(&cPasses, 0) );
				for (iPass = 0; iPass < cPasses; iPass++) {
					V( scene->effect->BeginPass(iPass) );

					int obj;
					for(obj=1;obj<controlData->numSceneObjects;obj++) { // skip the 1st object, I assume its the state andwill not self-shadow
						D3DXMATRIXA16 mWorldView = scene->obj[obj].matWorld * scene->light[i].viewMat;
						scene->effect->SetMatrix( "g_mWorldView", &mWorldView );
						V( scene->effect->CommitChanges() );
						scene->obj[obj].mesh->Render(pd3dDevice, true, true);
					}

					V( scene->effect->EndPass() );
				}
				V( scene->effect->End() );
			}

			if( pOldDS ) {
				pd3dDevice->SetDepthStencilSurface( pOldDS );
				pOldDS->Release();
			}
			pd3dDevice->SetRenderTarget( 0, pOldRT );
			SAFE_RELEASE( pOldRT );
		}
	}
}

void LG3DControl::ShowShadowMap(IDirect3DDevice9 *pd3dDevice, int mapIndex)
{
    pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	pd3dDevice->SetFVF(TVERTEX_FVF);
	pd3dDevice->SetStreamSource(0, m_pOverlayVB, 0, sizeof(TVERTEX));
	pd3dDevice->SetPixelShader(m_pShowMapPS);
	// pd3dDevice->SetPixelShader(NULL);
	pd3dDevice->SetTexture(0, scene->light[mapIndex].shadowMap);
	// pd3dDevice->SetTexture(0, NULL);
	pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
	pd3dDevice->SetTexture(0, NULL);

	pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
}

static void DrawLightBeam(IDirect3DDevice9 *pd3dDevice, LPDIRECT3DVERTEXBUFFER9 lightBeamVB, int numBeams)
{
	pd3dDevice->SetFVF(LIGHTBEAM_FVF);
	pd3dDevice->SetStreamSource(0, lightBeamVB, 0, sizeof(LightBeam));
	pd3dDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, numBeams);
}

void CALLBACK LG3DControl::OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime )
{
    UINT iPass, cPasses;

	int i;

	if(SUCCEEDED(pd3dDevice->BeginScene())) {
		// update any shadow maps
		if (controlData->wantShadows)
			UpdateShadowMaps(pd3dDevice);

		// Clear the render target and the zbuffer 
		V( pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, scene->clearColor, 1.0f, 0) );

		// set effect params that don't change over the life of this frame
		scene->effect->SetMatrix( "g_mProj", &matProj );
		scene->vertLightEffect->SetMatrix( "g_mProj", &matProj );
		D3DXVECTOR4 g_vLightAmbient;
		g_vLightAmbient.x = controlData->ambient.r/255.0f;
		g_vLightAmbient.y = controlData->ambient.g/255.0f;
		g_vLightAmbient.z = controlData->ambient.b/255.0f;
		g_vLightAmbient.w = 1.0f;
		scene->effect->SetVector("g_vLightAmbient", &g_vLightAmbient);
		scene->vertLightEffect->SetVector("g_vLightAmbient", &g_vLightAmbient);

		// for all non shadow casting, no gobo lights, render them all in one pass per object using efficient vertex shader
		// so here, we load up the shader params with those lights
		int g_nNumActiveLights = 0;
		D3DXVECTOR3 g_LightDirWorld[MAX_BASIC_LIGHTS];
		D3DXVECTOR3 g_LightPosWorld[MAX_BASIC_LIGHTS];
		D3DXVECTOR3 g_LightDiffuse[MAX_BASIC_LIGHTS];
		float g_fCosThetaWorld[MAX_BASIC_LIGHTS];
		bool firstBatch = true;
		for(i=0;i<controlData->numSceneLights;i++) {
			if ((g_nNumActiveLights < MAX_BASIC_LIGHTS) && (scene->light[i].loopId == 0) && controlData->sceneLightList[i].enabled) {
				g_LightDirWorld[g_nNumActiveLights].x = scene->light[i].lightDir.x;
				g_LightDirWorld[g_nNumActiveLights].y = scene->light[i].lightDir.y;
				g_LightDirWorld[g_nNumActiveLights].z = scene->light[i].lightDir.z;
				g_LightPosWorld[g_nNumActiveLights].x = controlData->sceneLightList[i].position.x;
				g_LightPosWorld[g_nNumActiveLights].y = controlData->sceneLightList[i].position.z;
				g_LightPosWorld[g_nNumActiveLights].z = controlData->sceneLightList[i].position.y;
				g_LightDiffuse[g_nNumActiveLights].x = (float)controlData->sceneLightList[i].color.r/255.0f;
				g_LightDiffuse[g_nNumActiveLights].y = (float)controlData->sceneLightList[i].color.g/255.0f;
				g_LightDiffuse[g_nNumActiveLights].z = (float)controlData->sceneLightList[i].color.b/255.0f;
				g_fCosThetaWorld[g_nNumActiveLights] = scene->light[i].cosTheta;
				g_nNumActiveLights++;
			}

			if ((g_nNumActiveLights == MAX_BASIC_LIGHTS) || (i == (controlData->numSceneLights-1))) {
				// Render the current 'batch' of lights
				V( scene->vertLightEffect->SetValue( "g_LightDirWorld", g_LightDirWorld, sizeof(D3DXVECTOR3)*MAX_BASIC_LIGHTS ) );
				V( scene->vertLightEffect->SetValue( "g_LightPosWorld", g_LightPosWorld, sizeof(D3DXVECTOR3)*MAX_BASIC_LIGHTS ) );
				V( scene->vertLightEffect->SetValue( "g_LightDiffuse", g_LightDiffuse, sizeof(D3DXVECTOR3)*MAX_BASIC_LIGHTS ) );
				V( scene->vertLightEffect->SetValue( "g_fCosThetaWorld", g_fCosThetaWorld, sizeof(float)*MAX_BASIC_LIGHTS ) );
				V( scene->vertLightEffect->SetInt( "g_nNumActiveLights", g_nNumActiveLights ) );

				// start by adding in ambient light contribution plus all basic lights
				if (firstBatch) {
					V( scene->vertLightEffect->SetTechnique( "RenderSceneMultiLight" ) );
				} else {
					V( scene->vertLightEffect->SetTechnique( "RenderSceneMultiLightBatch" ) );
				}
				firstBatch = false;

				// draw each object, lit by current 'batch' of lights
				int obj;
				for(obj=0;obj<controlData->numSceneObjects;obj++) {
					D3DXMATRIXA16 mWorldView = scene->obj[obj].matWorld * matView;
					scene->vertLightEffect->SetMatrix( "g_mWorldView", &mWorldView );
					scene->vertLightEffect->SetMatrix( "g_mWorld", &scene->obj[obj].matWorld );
					scene->obj[obj].mesh->Render(scene->vertLightEffect, "tColorMap", "g_vMaterial");
				}

				g_nNumActiveLights = 0;
			}
		}

		// draw the light indicator objects (light cans)
		V( scene->effect->SetTechnique( "RenderSceneAmb" ) );
		scene->effect->SetTexture( "tColorMap", g_pWhiteMap );
		D3DXVECTOR4 vMaterial(1.0f, 1.0f, 1.0f, 1.0f);
		scene->effect->SetVector( "g_vMaterial", &vMaterial );

		for(i=0;i<controlData->numSceneLights;i++) {
			D3DXMATRIXA16 mWorldView = scene->light[i].worldMat * matView;
			scene->effect->SetMatrix( "g_mWorldView", &mWorldView );
			// scene->effect->SetMatrix( "g_mWorld", &scene->light[i].worldMat );
			V( scene->effect->CommitChanges() );
			if (i == controlData->curLight) {
				D3DXVECTOR4 vMaterial(1.0f, 1.0f, 1.0f, 1.0f);
				scene->effect->SetVector( "g_vMaterial", &vMaterial );
				D3DXVECTOR4 g_vLightAmbientTemp(0.66f, 0.66f, 0.66f, 1.0f);
				scene->effect->SetVector("g_vLightAmbient", &g_vLightAmbientTemp);
				g_lightCan1->Render(scene->effect, "tColorMap");
				scene->effect->SetVector("g_vLightAmbient", &g_vLightAmbient);
			} else
				g_lightCan1->Render(scene->effect, "tColorMap", "g_vMaterial");
		}

		// these effect params don't change with multiple lights
		D3DXVECTOR4 eyeDir4;
		eyeDir.w = 0.0f; // so view position has no effect
		D3DXVec4Transform( &eyeDir4, &eyeDir, &matView );
		scene->effect->SetVector( "g_vEyeDir", &eyeDir4 );

		D3DXVECTOR4 eyePos4;
		D3DXVECTOR3 eyePos = D3DXVECTOR3(controlData->cameraList[controlData->curCamera].position.x, controlData->cameraList[controlData->curCamera].position.z, controlData->cameraList[controlData->curCamera].position.y);
		D3DXVec3Transform( &eyePos4, &eyePos, &matView );
		scene->effect->SetVector( "g_vEyePos", &eyePos4 );

		scene->effect->SetTexture( "tNormalMap", g_pNormMap );

		// we will now loop on all the lights twice, first time is for no shadow lights, second time is for shadow lights
		int loopId;
		for(loopId=1;loopId<=2;loopId++) {
			// add in diffuse+specular lighting contributions with shadows per light
			if ((loopId == 2) && controlData->wantShadows) {
				V( scene->effect->SetTechnique( "SpotLightAdd" ) );
			} else {
				V( scene->effect->SetTechnique( "SpotLightAddNoShadow" ) );
			}

			int light;
			for(light=0;light<controlData->numSceneLights;light++) {
				if (controlData->sceneLightList[light].enabled && (scene->light[light].loopId == loopId)) {
					// Compute the matrix to transform from view space to
					// light projection space.  This consists of
					// the inverse of view matrix * view matrix of light * light projection matrix
					D3DXMATRIXA16 mViewToLightProj;
					mViewToLightProj = matView;
					D3DXMatrixInverse( &mViewToLightProj, NULL, &mViewToLightProj );
					D3DXMatrixMultiply( &mViewToLightProj, &mViewToLightProj, &scene->light[light].viewMat );
					D3DXMatrixMultiply( &mViewToLightProj, &mViewToLightProj, &scene->light[light].projMat );
					scene->effect->SetMatrix( "g_mViewToLightProj", &mViewToLightProj );

					D3DXVECTOR4 lightPos4;
					D3DXVECTOR3 lightPos(controlData->sceneLightList[light].position.x, controlData->sceneLightList[light].position.z, controlData->sceneLightList[light].position.y);
					D3DXVec3Transform( &lightPos4, &lightPos, &matView );
					scene->effect->SetVector( "g_vLightPos", &lightPos4 );

					D3DXVECTOR4 lightDir4;
					D3DXVECTOR4 lightDir = scene->light[light].lightDir;
					lightDir.w = 0.0f; // so view position has no effect
					D3DXVec4Transform( &lightDir4, &lightDir, &matView );
					scene->effect->SetVector( "g_vLightDir", &lightDir4 );

					scene->effect->SetFloat( "g_fCosTheta", scene->light[light].cosTheta);
					if (controlData->wantShadows && (scene->light[light].loopId == 2))
						scene->effect->SetTexture( "tShadowMap", scene->light[light].shadowMap );
					scene->effect->SetTexture( "tSpotMap", scene->light[light].goboMap );
					scene->effect->SetFloat("g_fLinearAttenuation", controlData->sceneLightList[light].att1);
					scene->effect->SetFloat("g_fQuadraticAttenuation", controlData->sceneLightList[light].att2);
					scene->effect->SetVector("g_vLightColor", &scene->light[light].color);

					int i;
					for(i=0;i<controlData->numSceneObjects;i++) {
						D3DXMATRIXA16 mWorldView = scene->obj[i].matWorld * matView;
						scene->effect->SetMatrix( "g_mWorldView", &mWorldView );
						scene->obj[i].mesh->Render(scene->effect, "tColorMap", "g_vMaterial");
					}
				} // if light enabled
			} // loop on all lights
		} // loopId

		// Add in special effects such as light beams
		if (controlData->wantEffects) {
			V( scene->effect->SetTechnique( "SpotLightBeam" ) );
			V( scene->effect->Begin(&cPasses, 0) );
			for (iPass = 0; iPass < cPasses; iPass++) {
				V( scene->effect->BeginPass(iPass) );
				int light;
				for(light=0;light<controlData->numSceneLights;light++) {
					if (controlData->sceneLightList[light].enabled && (scene->light[light].numBeams > 0)) { // (controlData->sceneLightList[light].goboName[0] == 0)) {
						// Compute the matrix to transform from view space to
						// light projection space.  This consists of
						// the inverse of view matrix * view matrix of light * light projection matrix
						D3DXMATRIXA16 mViewToLightProj;
						mViewToLightProj = matView;
						D3DXMatrixInverse( &mViewToLightProj, NULL, &mViewToLightProj );
						D3DXMatrixMultiply( &mViewToLightProj, &mViewToLightProj, &scene->light[light].viewMat );
						D3DXMatrixMultiply( &mViewToLightProj, &mViewToLightProj, &scene->light[light].projMat );
						scene->effect->SetMatrix( "g_mViewToLightProj", &mViewToLightProj );

						D3DXVECTOR4 lightPos4;
						D3DXVECTOR3 lightPos(controlData->sceneLightList[light].position.x, controlData->sceneLightList[light].position.z, controlData->sceneLightList[light].position.y);
						D3DXVec3Transform( &lightPos4, &lightPos, &matView );
						scene->effect->SetVector( "g_vLightPos", &lightPos4 );

						D3DXVECTOR4 lightDir4;
						D3DXVECTOR4 lightDir = scene->light[light].lightDir;
						lightDir.w = 0.0f; // so view position has no effect
						D3DXVec4Transform( &lightDir4, &lightDir, &matView );
						scene->effect->SetVector( "g_vLightDir", &lightDir4 );

						scene->effect->SetFloat("g_fLinearAttenuation", controlData->sceneLightList[light].att1);
						scene->effect->SetFloat("g_fQuadraticAttenuation", controlData->sceneLightList[light].att2);

						if (controlData->wantShadows)
							scene->effect->SetTexture( "tShadowMap", scene->light[light].shadowMap );

						scene->effect->SetTexture( "tSpotMap", scene->light[light].goboMap );

						D3DXMATRIXA16 mWorldView = scene->light[light].worldMat * matView;
						scene->effect->SetTexture( "tColorMap", lightBeamTex );
						scene->effect->SetMatrix( "g_mWorldView", &mWorldView );
						scene->effect->SetMatrix( "g_mWorld", &scene->light[light].worldMat );
						V( scene->effect->CommitChanges() );
						DrawLightBeam(pd3dDevice, scene->light[light].lightBeamVB, scene->light[light].numBeams);
					} // if light enabled
				} // for loop on numSceneLights

				V( scene->effect->EndPass() );
			}
			V( scene->effect->End() );
		} // effects

		// show debug text info (driver, frame rate, etc.)
        RenderText();

		// debug:  show shadow map for given light
		// ShowShadowMap(pd3dDevice, 0);

        V( pd3dDevice->EndScene() );
	}
}


void LG3DControl::Draw()
{
	// ---------------------------------------------------------
	// basically letting DXUT handle this stuff for us, it will
	// call our frame move & frame render callbacks.
	// ---------------------------------------------------------
	DXUTRender3DEnvironment();

	// ---------------------------------------------------------
	// after things have drawn, clear any flags that may have
	// indicated extra work this frame
	// ---------------------------------------------------------
	int i;
	for(i=0;i<controlData->numSceneLights;i++) {
		scene->light[i].lightMoved = false;
	}

	for(i=0;i<controlData->numSceneObjects;i++) {
		scene->obj[i].moved = false;
	}
}

void LG3DControl::Intersect()
{
	// start by assuming nothing intersected
	manipObjId = -1;
	manipLightId = -1;

	// convert our mouse vector into world space

	// camera projection matrix
	const D3DXMATRIX *pmatProj = &matProj;

	// mouse XY in local window coords
	GetCursorPos( &lastMousePos );
	POINT localMousePos = lastMousePos;
	ScreenToClient( lg3dWnd, &localMousePos );

    // Compute the vector of the pick ray in screen space
    D3DXVECTOR3 v;
    v.x =  ( ( ( 2.0f * localMousePos.x ) / width  ) - 1 ) / pmatProj->_11;
    v.y = -( ( ( 2.0f * localMousePos.y ) / height ) - 1 ) / pmatProj->_22;
    v.z =  1.0f;

	BOOL bHit = FALSE;
	DWORD dwFace;
	FLOAT fBary1, fBary2, fDist, maxDist = 9999999.0f;
	D3DXMATRIXA16 m;

	// loop through all objects to see if we get a hit
	int i;
	for(i=0;i<controlData->numSceneObjects;i++) {
		D3DXMATRIXA16 mWorldView = scene->obj[i].matWorld * matView;
		D3DXMatrixInverse( &m, NULL, &mWorldView );

		// Transform the screen space pick ray into object's 3D space
		D3DXVECTOR3 vPickRayDir;
		D3DXVECTOR3 vPickRayOrig;
		vPickRayDir.x  = v.x*m._11 + v.y*m._21 + v.z*m._31;
		vPickRayDir.y  = v.x*m._12 + v.y*m._22 + v.z*m._32;
		vPickRayDir.z  = v.x*m._13 + v.y*m._23 + v.z*m._33;
		vPickRayOrig.x = m._41;
		vPickRayOrig.y = m._42;
		vPickRayOrig.z = m._43;

		// and intersect
		D3DXIntersect(scene->obj[i].mesh->m_pMesh, &vPickRayOrig, &vPickRayDir, &bHit, &dwFace, &fBary1, &fBary2, &fDist, NULL, NULL);
		if (bHit) {
			// OutputDebugString(L"Got an object hit!!!\n");
			if (fDist < maxDist) {
				maxDist = fDist;
				manipObjId = i;
			}
		}
	}

	// loop through all lights to see if we get a hit
	for(i=0;i<controlData->numSceneLights;i++) {
		D3DXMATRIXA16 mWorldView = scene->light[i].worldMat * matView;
		D3DXMatrixInverse( &m, NULL, &mWorldView );

		// Transform the screen space pick ray into light's 3D space
		D3DXVECTOR3 vPickRayDir;
		D3DXVECTOR3 vPickRayOrig;
		vPickRayDir.x  = v.x*m._11 + v.y*m._21 + v.z*m._31;
		vPickRayDir.y  = v.x*m._12 + v.y*m._22 + v.z*m._32;
		vPickRayDir.z  = v.x*m._13 + v.y*m._23 + v.z*m._33;
		vPickRayOrig.x = m._41;
		vPickRayOrig.y = m._42;
		vPickRayOrig.z = m._43;

		// and intersect
		D3DXIntersect(g_lightCan1->m_pMesh, &vPickRayOrig, &vPickRayDir, &bHit, &dwFace, &fBary1, &fBary2, &fDist, NULL, NULL);
		if (bHit) {
			// OutputDebugString(L"Got a light hit!!!\n");
			if (fDist < maxDist) {
				maxDist = fDist;
				manipLightId = i;
				manipObjId = -1; // it might have been set already, but this object is closer
			}
		}
	}
}

void LG3DControl::LButtonDown()
{
	lButtonDown = true;
	Intersect();
	controlData->curLight = manipLightId;
}

void LG3DControl::LButtonUp()
{
	lButtonDown = false;
	manipObjId = -1;
	manipLightId = -1;
	controlData->curLight = manipLightId;
}

void LG3DControl::RButtonDown()
{
	rButtonDown = true;
	Intersect();
	controlData->curLight = manipLightId;
}

void LG3DControl::RButtonUp()
{
	rButtonDown = false;
	manipObjId = -1;
	manipLightId = -1;
	controlData->curLight = manipLightId;
}

void LG3DControl::MouseMove()
{
	if (manipObjId > -1 || manipLightId > -1) {
		POINT curMousePos;
		GetCursorPos(&curMousePos);
		int dx, dy, dz;
		bool ctrlDown = false;
		ctrlDown = (GetAsyncKeyState(VK_CONTROL) != 0);
		if (ctrlDown) {
			dx = 0;
			dy = 0;
			dz = curMousePos.y - lastMousePos.y;
		} else {
			dx = curMousePos.x - lastMousePos.x;
			dy = curMousePos.y - lastMousePos.y;
			dz = 0;
		}
		lastMousePos = curMousePos;

		LG3DPosition posDelta = {0};
		LG3DOrientation orientDelta = {0};

		if (lButtonDown) {
			// calculate deltas based on rotated camera
			float dx2, dy2, dz2;
			dx2 = -sinf(DEG2RADf(controlData->cameraList[controlData->curCamera].orientation.h)) * dy + cosf(DEG2RADf(controlData->cameraList[controlData->curCamera].orientation.h)) * dx;
			dy2 = -cosf(DEG2RADf(controlData->cameraList[controlData->curCamera].orientation.h)) * dy - sinf(DEG2RADf(controlData->cameraList[controlData->curCamera].orientation.h)) * dx;
			dz2 = dz * -1.0f;

			posDelta.x = dx2 * 0.025f;
			posDelta.y = dy2 * 0.025f;
			posDelta.z = dz2 * 0.025f;
		}

		if (rButtonDown) {
			orientDelta.h = dx * 1.0f;
			orientDelta.p = dy * -1.0f;
			orientDelta.r = 0.0f;
		}

		if (manipLightId > -1) {
			controlData->MoveLight(manipLightId, &posDelta, &orientDelta);
			scene->light[manipLightId].lightMoved = true;
		}

		if (manipObjId > -1) {
			controlData->MoveObject(manipObjId, &posDelta, &orientDelta);
			// when an object moves, all objects need to be re-tested against lights for visibility using new object position
			// for now, just trivially cause all lights to re-generate shadow maps
			int i;
			for(i=0;i<controlData->numSceneLights;i++) {
				scene->light[i].lightMoved = true;
			}
		}

		Draw();
	}
}
