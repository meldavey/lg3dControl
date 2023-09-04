#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define APP_ESCAPE		0x1B
#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))
#include <math.h>

#include "lg3d.h"

HWND	hwnd;
bool	appOK = false;
int		width = 800, height = 600;
bool	animate = true;
DWORD	ticsThen = 0;
bool	tick = false;
bool	lightToggleActive = false;
bool	cameraSelectActive = false;
bool	performanceTest = false;
int		stressTest = 0;

LG3DControl	*lg3d = NULL;
LG3DControlData *lg3dData = NULL;

void LG3DCreate()
{
	lg3dData = new LG3DControlData();

	lg3dData->ambient.r = 64;
	lg3dData->ambient.g = 64;
	lg3dData->ambient.b = 64;

	lg3dData->clearColor.r = 0;
	lg3dData->clearColor.g = 0;
	lg3dData->clearColor.b = 32;

	lg3dData->wantShadows = true;

	lg3dData->numSceneObjects = 2;
	lg3dData->sceneObjectList = new LG3DSceneObject[lg3dData->numSceneObjects];

	wcscpy_s(lg3dData->sceneObjectList[0].meshName, L".\\data\\StageFloor.x");
	// wcscpy_s(lg3dData->sceneObjectList[0].meshName, L".\\data\\Stage.x");
	lg3dData->sceneObjectList[0].position.z = 1.5f;
	wcscpy_s(lg3dData->sceneObjectList[1].meshName, L".\\data\\ModelColumns.x");
	lg3dData->sceneObjectList[1].position.z = 1.5f;

	if (stressTest > 0) {
		animate = false;
		lg3dData->numSceneLights = (stressTest == 1 ? 64 : 256);
		lg3dData->sceneLightList = new LG3DSceneLight[lg3dData->numSceneLights];
		int i;
		float dist = 6.0f;
		float angleRad, angleDeg;
		for(i=0;i<lg3dData->numSceneLights;i++) {
			angleRad = 6.283185307179586476925286766559f * i / (float)lg3dData->numSceneLights;
			angleDeg = 360.0f * i / (float)lg3dData->numSceneLights;
			lg3dData->sceneLightList[i].att1 = 0.2f;
			lg3dData->sceneLightList[i].umbra = 3.0f;
			lg3dData->sceneLightList[i].penumbra = 7.0f;
			lg3dData->sceneLightList[i].position.x = -dist * sinf(angleRad);
			lg3dData->sceneLightList[i].position.y = -dist * cosf(angleRad);
			lg3dData->sceneLightList[i].position.z = 2.6f;
			lg3dData->sceneLightList[i].orientation.h = angleDeg;
			lg3dData->sceneLightList[i].orientation.p = -20.0f - sinf(angleRad*2.5f)*10.0f;
			lg3dData->sceneLightList[i].castsShadows = (stressTest == 1 ? true : false);
			lg3dData->sceneLightList[i].enabled = true;
			lg3dData->sceneLightList[i].color.r = rand()%255;
			lg3dData->sceneLightList[i].color.g = rand()%255;
			lg3dData->sceneLightList[i].color.b = rand()%255;
			lg3dData->sceneLightList[i].pinMask = LG3DPinMask_XYZ; // lights can rotate but position is fixed
		}
	} else {
		lg3dData->numSceneLights = 4;
		lg3dData->sceneLightList = new LG3DSceneLight[lg3dData->numSceneLights];

		lg3dData->sceneLightList[0].att1 = 0.2f;
		lg3dData->sceneLightList[0].umbra = 40.0f;
		lg3dData->sceneLightList[0].penumbra = 5.0f;
		lg3dData->sceneLightList[0].position.x = 2.5f;
		lg3dData->sceneLightList[0].position.y = 0.0f;
		lg3dData->sceneLightList[0].position.z = 2.6f;
		lg3dData->sceneLightList[0].orientation.p = -10.0f;
		lg3dData->sceneLightList[0].castsShadows = true;
		lg3dData->sceneLightList[0].enabled = true;
		// wcscpy_s(lg3dData->sceneLightList[0].goboName, L".\\data\\white.bmp");
		lg3dData->sceneLightList[0].color.r = 255;
		lg3dData->sceneLightList[0].color.g = 255;
		lg3dData->sceneLightList[0].color.b = 255;
		lg3dData->sceneLightList[0].pinMask = LG3DPinMask_XYZ; // lights can rotate but position is fixed

		lg3dData->sceneLightList[1].att1 = 0.2f;
		lg3dData->sceneLightList[1].umbra = 15.0f;
		lg3dData->sceneLightList[1].penumbra = 5.0f;
		lg3dData->sceneLightList[1].position.x = 2.0f;
		lg3dData->sceneLightList[1].position.y = 2.0f;
		lg3dData->sceneLightList[1].position.z = 3.0f;
		lg3dData->sceneLightList[1].orientation.h = 245.0f;
		lg3dData->sceneLightList[1].orientation.p = -40.0f;
		lg3dData->sceneLightList[1].enabled = true;
		lg3dData->sceneLightList[1].castsShadows = true;
		wcscpy_s(lg3dData->sceneLightList[1].goboName, L".\\data\\gobo.bmp");
		lg3dData->sceneLightList[1].color.r = 255;
		lg3dData->sceneLightList[1].color.g = 255;
		lg3dData->sceneLightList[1].color.b = 255;
		lg3dData->sceneLightList[1].pinMask = LG3DPinMask_XYZ; // lights can rotate but position is fixed

		lg3dData->sceneLightList[2].att1 = 0.2f;
		lg3dData->sceneLightList[2].umbra = 12.5f;
		lg3dData->sceneLightList[2].penumbra = 3.0f;
		lg3dData->sceneLightList[2].position.x = 2.0f;
		lg3dData->sceneLightList[2].position.y = -1.0f;
		lg3dData->sceneLightList[2].position.z = 3.0f;
		lg3dData->sceneLightList[2].orientation.h = 290.0f;
		lg3dData->sceneLightList[2].orientation.p = -35.0f;
		lg3dData->sceneLightList[2].enabled = true;
		lg3dData->sceneLightList[2].castsShadows = true;
		lg3dData->sceneLightList[2].color.r = 255;
		lg3dData->sceneLightList[2].color.g = 255;
		lg3dData->sceneLightList[2].color.b = 255;
		lg3dData->sceneLightList[2].pinMask = LG3DPinMask_XYZ; // lights can rotate but position is fixed

		lg3dData->sceneLightList[3].att1 = 0.1f;
		lg3dData->sceneLightList[3].umbra = 25.0f;
		lg3dData->sceneLightList[3].penumbra = 5.0f;
		lg3dData->sceneLightList[3].position.x = -0.6f;
		lg3dData->sceneLightList[3].position.y = -4.0f;
		lg3dData->sceneLightList[3].position.z = 2.6f;
		lg3dData->sceneLightList[3].orientation.h = 350.0f;
		lg3dData->sceneLightList[3].orientation.p = -20.0f;
		lg3dData->sceneLightList[3].enabled = true;
		lg3dData->sceneLightList[3].castsShadows = true;
		lg3dData->sceneLightList[3].color.r = 0;
		lg3dData->sceneLightList[3].color.g = 255;
		lg3dData->sceneLightList[3].color.b = 0;
		lg3dData->sceneLightList[3].pinMask = LG3DPinMask_XYZ; // lights can rotate but position is fixed
	}

	lg3dData->numCameras = 4;
	lg3dData->cameraList = new LG3DCameraObject[lg3dData->numCameras];

	wcscpy_s(lg3dData->cameraList[0].name, L"default");
	lg3dData->cameraList[0].fov = 60.0f;
	if (stressTest) {
		lg3dData->cameraList[0].position.x = 8.0f;
		lg3dData->cameraList[0].position.y = -8.0f;
		lg3dData->cameraList[0].position.z = 3.5f;
	} else {
		lg3dData->cameraList[0].position.x = 4.0f;
		lg3dData->cameraList[0].position.y = -4.0f;
		lg3dData->cameraList[0].position.z = 2.5f;
	}
	lg3dData->cameraList[0].orientation.h = -45.0f;
	lg3dData->cameraList[0].orientation.p = -15.0f;

	wcscpy_s(lg3dData->cameraList[1].name, L"top");
	lg3dData->cameraList[1].fov = 60.0f;
	lg3dData->cameraList[1].position.z = 8.0f;
	lg3dData->cameraList[1].orientation.h = 0.0f;
	lg3dData->cameraList[1].orientation.p = -90.0f;

	wcscpy_s(lg3dData->cameraList[2].name, L"front");
	lg3dData->cameraList[2].fov = 45.0f;
	lg3dData->cameraList[2].position.z = 2.0f;
	lg3dData->cameraList[2].position.y = -8.0f;
	lg3dData->cameraList[2].orientation.h = 0.0f;
	lg3dData->cameraList[2].orientation.p = 0.0f;

	wcscpy_s(lg3dData->cameraList[3].name, L"side");
	lg3dData->cameraList[3].fov = 90.0f;
	lg3dData->cameraList[3].position.z = 2.0f;
	lg3dData->cameraList[3].position.x = 8.0f;
	lg3dData->cameraList[3].orientation.h = -90.0f;
	lg3dData->cameraList[3].orientation.p = 0.0f;

	lg3dData->curCamera = 0;

	lg3d = new LG3DControl(hwnd, lg3dData);
	lg3d->Init();
}

void LG3DClose()
{
	if (lg3d) {
		delete lg3d;
		lg3d = NULL;
	}

	delete [] lg3dData->sceneObjectList;
	delete [] lg3dData->sceneLightList;
	delete [] lg3dData->cameraList;
}

void LG3DDraw()
{
	if (lg3d) {
		lg3d->Draw();
	}
}

long FAR PASCAL WindowProc(HWND hWnd, UINT message, 
				WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	static int lx = 0, ly = 0;
	static int rx = 0, ry = 0;
	static bool ldown = false;
	static bool rdown = false;

	switch(message) {
		case WM_CHAR:
			switch(wParam) {
				case APP_ESCAPE:
					appOK = false;
					PostQuitMessage(0);
				break;

				case ' ':
					animate = false;
					lg3dData->sceneLightList[0].orientation.h += 5.0f;
					LG3DDraw();
				break;

				case 'a':
					animate = true;
					ticsThen = 0; // to force immediate draw
				break;

				case 'm':
					lg3dData->ambient.r -= 16;
					lg3dData->ambient.g -= 16;
					lg3dData->ambient.b -= 16;
					tick = true;
				break;

				case 'M':
					lg3dData->ambient.r += 16;
					lg3dData->ambient.g += 16;
					lg3dData->ambient.b += 16;
					tick = true;
				break;

				case 's':
					lg3dData->wantShadows = !lg3dData->wantShadows;
					tick = true;
				break;

				case 'e':
					lg3dData->wantEffects = !lg3dData->wantEffects;
					tick = true;
				break;

				case 'p':
					performanceTest = !performanceTest;
				break;

				case 'l': // ell
					lightToggleActive = true;
				break;

				case '1': // one
				case '2':
				case '3':
				case '4':
					if (lightToggleActive) {
						int whichLight = wParam - '1';
						lg3dData->sceneLightList[whichLight].enabled = !lg3dData->sceneLightList[whichLight].enabled;
						lightToggleActive = false;
						tick = true;
					}

					if (cameraSelectActive) {
						int whichCamera = wParam - '1';
						if (whichCamera < 0)
							whichCamera = 0;
						if (whichCamera > (lg3dData->numCameras-1))
							whichCamera = lg3dData->numCameras-1;
						lg3dData->curCamera = whichCamera;
						cameraSelectActive = false;
						tick =true;
					}
				break;

				case 'c': // camera select
					cameraSelectActive = true;
				break;

				case 't':
					LG3DClose();
					stressTest = (stressTest + 1)%3;
					LG3DCreate();
					tick =true;
				break;
			}
		break;

		case WM_PAINT:
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		break;

		case WM_DESTROY:
			appOK = false;
			PostQuitMessage( 0 );
		break;

		case WM_LBUTTONDOWN:
			lx = GET_X_LPARAM(lParam);
			ly = GET_Y_LPARAM(lParam);
			ldown = true;
		break;

		case WM_LBUTTONUP:
			ldown = false;
		break;

		case WM_RBUTTONDOWN:
			rx = GET_X_LPARAM(lParam);
			ry = GET_Y_LPARAM(lParam);
			rdown = true;
		break;

		case WM_RBUTTONUP:
			rdown = false;
		break;

		case WM_MOUSEMOVE:
			if (ldown) {
				int dx, dy;
				int x, y;
				x = GET_X_LPARAM(lParam); 
				y = GET_Y_LPARAM(lParam); 
				dx = x - lx;
				dy = y - ly;
				lx = x;
				ly = y;
				float fdx = 0.1f * (-dx * cosf(DEG2RADf(lg3dData->cameraList[lg3dData->curCamera].orientation.h)) + dy * sinf(DEG2RADf(lg3dData->cameraList[lg3dData->curCamera].orientation.h)));
				float fdy = 0.1f * (dy * cosf(DEG2RADf(lg3dData->cameraList[lg3dData->curCamera].orientation.h)) + dx * sinf(DEG2RADf(lg3dData->cameraList[lg3dData->curCamera].orientation.h)));
				float fdz = dy * 0.1f;
				if (wParam & MK_SHIFT) {
					fdx *= 0.1f;
					fdy *= 0.1f;
					fdz *= 0.1f;
				}
				if (wParam & MK_CONTROL) {
					lg3dData->cameraList[lg3dData->curCamera].position.z += fdz; // *0.1f;
				} else {
					lg3dData->cameraList[lg3dData->curCamera].position.x += fdx;
					lg3dData->cameraList[lg3dData->curCamera].position.y += fdy;
				}
				tick = true;
			}

			if (rdown) {
				int x, y;
				x = GET_X_LPARAM(lParam);
				y = GET_Y_LPARAM(lParam); 
				int dx, dy;
				dx = x - rx;
				dy = y - ry;
				rx = x;
				ry = y;
				lg3dData->cameraList[lg3dData->curCamera].orientation.h += dx;
				lg3dData->cameraList[lg3dData->curCamera].orientation.p += dy;
				tick = true;
			}
		break;

		default:          // Passes it on if unproccessed
			return (DefWindowProc(hWnd, message, wParam, lParam));
	}

	return 0;
}

void Idle()
{
	DWORD ticsNow = GetTickCount();
	DWORD deltaTics = ticsNow - ticsThen;
	if (!performanceTest && (deltaTics < 60)) { // 60 is roughtly 16fps
		Sleep(0); // give priority to other threads since we don't need it right now
		return;
	}

	ticsThen = ticsNow;

	if (animate || tick || performanceTest) {
		if (animate)
			lg3dData->sceneLightList[0].orientation.h += 5.0f;
		LG3DDraw();
		tick = false;
	} else
		WaitMessage();
}

void EventLoop()
{
	MSG      msg;
	bool	forever = true;

	while (forever) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				return;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			if (appOK)
				Idle();
			else
				WaitMessage();
		}
	}
}

int PASCAL WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	lpCmdLine; // shhh
	hPrevInstance; // shhh

	WNDCLASS		wc;
	int				x, y;

    wc.style = 0; // CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"LG3D Test";
    RegisterClass( &wc );

	DWORD m_dwWindowStyle = 0;
	m_dwWindowStyle = WS_CAPTION; // WS_OVERLAPPEDWINDOW;
	// make sure in windowed mode, that we don't obscure any toobar type windows
	RECT rect;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
	x = rect.left;
	y = rect.top;

	DWORD flags = 0;

    // Calculate actual window width & height needed
	RECT rc;
	SetRect(&rc, 0, 0, width, height);
	AdjustWindowRect(&rc, m_dwWindowStyle, FALSE);

	// Create window
    hwnd = CreateWindowEx(
		flags,
		wc.lpszClassName,
		wc.lpszClassName,
		m_dwWindowStyle,
		x, y,
		(rc.right-rc.left), (rc.bottom-rc.top),
		NULL,
		NULL,
		hInstance,
		NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

	LG3DCreate();

	// main window should have focus when we begin
	SetFocus(hwnd);
	// cause main window to redraw
	InvalidateRect(hwnd, NULL, FALSE);

	appOK = true;
	EventLoop();

	LG3DClose();

	return 0;
}
