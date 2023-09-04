#ifndef __LG3D__
#define __LG3D__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#define DEG2RAD(d) ((d)*0.017453292519943295769236907684886)
#define DEG2RADf(d) ((d)*0.017453292519943295769236907684886f)

#if defined(LG3DCONTROLDLL_EXPORTS)
#define LG3D_DLL   __declspec( dllexport )
#elif defined (LG3DCONTROLDLL_IMPORTS)
#define LG3D_DLL   __declspec( dllimport )
#else
#define LG3D_DLL
#endif

enum LG3DPinMaskType {
	LG3DPinMask_X = (1<<0),
	LG3DPinMask_Y = (1<<1),
	LG3DPinMask_Z = (1<<2),
	LG3DPinMask_H = (1<<3),
	LG3DPinMask_P = (1<<4),
	LG3DPinMask_R = (1<<5),
	LG3DPinMask_XYZ = (LG3DPinMask_X | LG3DPinMask_Y | LG3DPinMask_Z),
};

struct LG3DPosition {
	float			x, y, z;				// Z up (meters)
};

struct LG3DOrientation {
	float			h, p, r;				// heading, pitch, roll (degrees)
};

struct LG3DLightColor {
	int				r, g, b;				// red, green, blue, 0 to 255
};

struct LG3DSceneObject {
	WCHAR			meshName[_MAX_PATH];	// Microsoft .X file format
	LG3DPosition	position;
	LG3DOrientation	orientation;
	LG3DSceneObject() {memset(this, 0, sizeof(LG3DSceneObject));}
};

struct LG3DSceneLight {
	LG3DPosition	position;
	LG3DOrientation	orientation;
	unsigned long	pinMask;				// bitwise OR of un-changeable position & orientation settings - see LG3DPinMaskType enum
	LG3DLightColor	color;					// rgb intensity can be implied here
	WCHAR			goboName[_MAX_PATH];	// bitmap (.bmp) color mask file name
	float			umbra;					// angle in degrees
	float			penumbra;				// angle in degrees
	float			att1;					// linear attenuation value
	float			att2;					// quadratic attenuation value
	bool			enabled;				// true to turn light on, false to turn light off
	bool			castsShadows;			// when set, this light causes shadows to be cast from it
	LG3DSceneLight() {memset(this, 0, sizeof(LG3DSceneLight));}
};

struct LG3DCameraObject {
	WCHAR			name[256];				// name assigned to this camera
	LG3DPosition	position;
	LG3DOrientation	orientation;
	float			fov;					// vertical field of view (horizontal derived from screen aspect)
	unsigned long	pinMask;				// bitwise OR of un-changeable position & orientation settings - see LG3DPinMaskType enum
	LG3DCameraObject() {memset(this, 0, sizeof(LG3DCameraObject));}
};

class LG3D_DLL LG3DControlData {
	public:
		int				numSceneObjects;
		LG3DSceneObject	*sceneObjectList;
		int				numSceneLights;
		LG3DSceneLight	*sceneLightList;
		LG3DLightColor	ambient;
		int				numCameras;
		LG3DCameraObject *cameraList;
		LG3DLightColor	clearColor;				// frame buffer clear to color

		int				curLight;				// the currently selected light
		int				curCamera;				// the currently selected camera

		bool			wantShadows;			// set to false to disable all shadow rendering
		bool			wantEffects;			// set to true to show light beam effects

		LG3DControlData() {
			numSceneObjects = 0;
			sceneObjectList = NULL;
			numSceneLights = 0;
			sceneLightList = NULL;
			ambient.r = ambient.g = ambient.b = 24;
			numCameras = 0;
			cameraList = NULL;
			curLight = -1;
			curCamera = -1;
			wantShadows = true;
			wantEffects = false;
		}

		virtual ~LG3DControlData() {};

		virtual void MoveLight(int lightIndex, LG3DPosition *posDelta, LG3DOrientation *orientDelta)
		{
			if (posDelta) {
				if (!(sceneLightList[lightIndex].pinMask & LG3DPinMask_X))
					sceneLightList[lightIndex].position.x += posDelta->x;
				if (!(sceneLightList[lightIndex].pinMask & LG3DPinMask_Y))
					sceneLightList[lightIndex].position.y += posDelta->y;
				if (!(sceneLightList[lightIndex].pinMask & LG3DPinMask_Z))
					sceneLightList[lightIndex].position.z += posDelta->z;
			}

			if (orientDelta) {
				if (!(sceneLightList[lightIndex].pinMask & LG3DPinMask_H))
					sceneLightList[lightIndex].orientation.h += orientDelta->h;
				if (!(sceneLightList[lightIndex].pinMask & LG3DPinMask_P))
					sceneLightList[lightIndex].orientation.p += orientDelta->p;
				if (!(sceneLightList[lightIndex].pinMask & LG3DPinMask_R))
					sceneLightList[lightIndex].orientation.r += orientDelta->r;
			}
		}

		virtual void MoveObject(int objectIndex, LG3DPosition *posDelta, LG3DOrientation *orientDelta)
		{
			if (posDelta) {
				sceneObjectList[objectIndex].position.x += posDelta->x;
				sceneObjectList[objectIndex].position.y += posDelta->y;
				sceneObjectList[objectIndex].position.z += posDelta->z;
			}

			if (orientDelta) {
				sceneObjectList[objectIndex].orientation.h += orientDelta->h;
				sceneObjectList[objectIndex].orientation.p += orientDelta->p;
				sceneObjectList[objectIndex].orientation.r += orientDelta->r;
			}
		}
};

// internal structures
struct LG3DScene;

class LG3D_DLL LG3DControl {
	public:
		LG3DControl(HWND parent, LG3DControlData *controlData);
		virtual ~LG3DControl();

		virtual bool Init();
		virtual void Draw();

		virtual void SetShadowMapSize(int size) {shadowMapSize = size;}

		HRESULT CALLBACK	OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc );
		HRESULT CALLBACK	OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc );
		void    CALLBACK	OnLostDevice( );
		void    CALLBACK	OnDestroyDevice( );
		void    CALLBACK	OnFrameMove( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime );
		void    CALLBACK	OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime );

		void				LButtonDown();
		void				LButtonUp();
		void				RButtonDown();
		void				RButtonUp();
		void				MouseMove();
	protected:

		LG3DControlData		*controlData;
		HWND				parent;
		HWND				lg3dWnd;
		int					width;
		int					height;
		int					shadowMapSize;		// defaults to 256

		LG3DScene			*scene;

		void				CreateRenderWindow();
		void				UpdateShadowMaps(IDirect3DDevice9 *pd3dDevice);
		void				ShowShadowMap(IDirect3DDevice9 *pd3dDevice, int mapIndex);

		void				Intersect();
		int					manipObjId;
		int					manipLightId;
		POINT				lastMousePos;
		bool				rButtonDown;
		bool				lButtonDown;
};

#endif /* __LG3D__ */
