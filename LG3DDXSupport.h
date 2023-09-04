#ifndef __LG3DDXSupport__
#define __LG3DDXSupport__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

// #define DEBUG_VS
// #define DEBUG_PS

HRESULT CALLBACK LG3DOnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK LG3DOnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void    CALLBACK LG3DOnLostDevice( void* pUserContext );
void    CALLBACK LG3DOnDestroyDevice( void* pUserContext );
void    CALLBACK LG3DOnFrameMove( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void    CALLBACK LG3DOnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
bool    CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
bool    CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, const D3DCAPS9* pCaps, void* pUserContext );

#endif /* __LG3DDXSupport__ */
