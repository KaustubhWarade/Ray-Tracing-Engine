// windows header files

#include "win32Window.h"
#include "RenderEngine.h"
#include "ImGuiHelper.h"
#include "Timer.h"
#include "global.h"
#include "D3D.h"
#include "../IApplication.h"
#include "../CoreHelper Files/ResourceManager.h"
using namespace DirectX;


// Global functions declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

BOOL gbFullScreen = FALSE;
HWND ghwnd = NULL;
FILE* gpFile = NULL;
const char gszLogFileName[] = "Log.txt";

DWORD dwStyle = 0;
WINDOWPLACEMENT wpPrev = { sizeof(WINDOWPLACEMENT) };
BOOL gbActive = FALSE;

static std::unique_ptr<RenderEngine> g_pRenderEngine;

extern float m_LastRenderTime = 0.0f;
// entry point function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR LpszCmdLine, int iCmdShow)
{
	// function declaration
	HRESULT initialize(void);
	void uninitialize(void);
	void display(void);
	void update(void);

	// local variable declaration
	WNDCLASSEX wndclass;
	HWND hwnd;
	MSG msg;
	TCHAR szAppName[] = TEXT("KSWWindow");
	BOOL bDone = FALSE;
	HRESULT hr = S_OK;

	int SW = GetSystemMetrics(SM_CXSCREEN);
	int SH = GetSystemMetrics(SM_CYSCREEN);

	// code
	
#ifdef _DEBUG
// code
	fopen_s(&gpFile, gszLogFileName, "w");
	if (gpFile == NULL)
	{
		MessageBox(NULL, TEXT("Log File Can Not Be Opened."), TEXT("ERROR"), MB_OK | MB_ICONERROR);
		exit(0);
	}
	else
	{
		fprintf(gpFile, "Log File successfully Created.\n");
		// Keep it open
	}
#endif

	// WNDCLACCEX initialization
	wndclass.cbSize = sizeof(WNDCLASSEX);
	wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.lpfnWndProc = WndProc;
	wndclass.hInstance = hInstance;
	wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.lpszClassName = szAppName;
	wndclass.lpszMenuName = NULL;
	wndclass.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(MYICON));

	// Register WNDCLASSEX
	RegisterClassEx(&wndclass);

	g_pRenderEngine = std::make_unique<RenderEngine>();

	// create window
	hwnd = CreateWindowEx(WS_EX_APPWINDOW,
						  szAppName,
						  TEXT("KSW Rendering Engine"),
						  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
						  0,
						  0,
						  WIN_WIDTH,
						  WIN_HEIGHT,
						  NULL,
						  NULL,
						  hInstance,
						  NULL);

	ghwnd = hwnd;

	// initialization
	std::unique_ptr<IApplication> pApplication = CreateApplication();
	g_pRenderEngine->SetApplication(std::move(pApplication));
	hr = g_pRenderEngine->initialize();
	if (FAILED(hr))
	{
		DestroyWindow(hwnd);
	}
	EXECUTE_AND_LOG_RETURN(g_pRenderEngine->initialize_imgui());

	ShowWindow(hwnd, iCmdShow);

	SetForegroundWindow(hwnd); // agar clipchildren or clipscreen creates error incase
	SetFocus(hwnd);			   // internally calls W_SETFOCUS;

	// game loop
	while (bDone == FALSE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				bDone = TRUE;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			if (gbActive == TRUE)
			{
				Timer timer;
				// render
				g_pRenderEngine->display();
				// update
				g_pRenderEngine->update();

				m_LastRenderTime = timer.ElapsedMillis();
			}
		}
	}

	// uninitialization
	uninitialize();

	return ((int)msg.wParam);
}

// callback function
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	// function declaration
	void ToggleFullScreen(void);
	HRESULT resize(int, int);
	HRESULT hr = S_OK;

	if (ImGui::GetCurrentContext())
	{

		if (ImGui_ImplWin32_WndProcHandler(hwnd, iMsg, wParam, lParam))
			return true;

		if (g_pRenderEngine && g_pRenderEngine->HandleMessage(hwnd, iMsg, wParam, lParam))
			return DefWindowProc(hwnd, iMsg, wParam, lParam);
	}

	// code
	switch (iMsg)
	{
	case WM_SETFOCUS:
		gbActive = TRUE;
		break;

	case WM_KILLFOCUS:
		gbActive = FALSE;
		break;

	case WM_SIZE:
		if (g_pRenderEngine->GetDevice())
		{
			EXECUTE_AND_LOG(g_pRenderEngine->resize(LOWORD(lParam), HIWORD(lParam)));
		}
		break;

	case WM_ERASEBKGND:
		return (0);

	case WM_KEYDOWN:
		switch (LOWORD(wParam))
		{
		case VK_ESCAPE:
			DestroyWindow(hwnd);
			break;
		}
		break;

	case WM_CHAR:
		switch (LOWORD(wParam))
		{
		case 'F':
		case 'f':
			if (gbFullScreen == FALSE)
			{
				ToggleFullScreen();
				gbFullScreen = TRUE;
			}
			else
			{
				ToggleFullScreen();
				gbFullScreen = FALSE;
			}
			break;

		}
		break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		break;
	}
	return (DefWindowProc(hwnd, iMsg, wParam, lParam));
}

void ToggleFullScreen(void)
{
	MONITORINFO mi = {sizeof(MONITORINFO)};

	if (gbFullScreen == FALSE)
	{
		dwStyle = GetWindowLong(ghwnd, GWL_STYLE);
		if (dwStyle & WS_OVERLAPPEDWINDOW)
		{
			if (GetWindowPlacement(ghwnd, &wpPrev) && GetMonitorInfo(MonitorFromWindow(ghwnd, MONITORINFOF_PRIMARY), &mi))
			{
				SetWindowLong(ghwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
				SetWindowPos(ghwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);
			}
		}
		//ShowCursor(FALSE);
	}
	else
	{
		SetWindowPlacement(ghwnd, &wpPrev);
		SetWindowLong(ghwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPos(ghwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
		ShowCursor(TRUE);
	}
}


void uninitialize(void)
{
	// function declaration
	void ToggleFullScreen(void);
	ResourceManager::Get()->Shutdown();
	g_pRenderEngine->uninitialize();

	// fi RenderEngine  is exitting in full screen
	if (gbFullScreen == TRUE)
		ToggleFullScreen();

	// destroywindow/getrid of window handle
	if (ghwnd)
	{
		DestroyWindow(ghwnd);
		ghwnd = NULL;
	}
	// close log file
	if (gpFile)
	{
		fprintf(gpFile, "File closing...\n");
		fclose(gpFile);
		gpFile = NULL;
	}
}
