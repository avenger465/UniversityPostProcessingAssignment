//--------------------------------------------------------------------------------------
// Entry point for the application
// Window creation code
//--------------------------------------------------------------------------------------
#include "System/System.h"
#include "PostProcessingScene/PostProcessingScene.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Utility/Input.h"
#include "Utility/Timer.h"
#include "project/Common.h"
#include <Windows.h>
#include <string>

//--------------------------------------------------------------------------------------
// The entry function for a Windows application is called wWinMain
//--------------------------------------------------------------------------------------
int APIENTRY wWinMain(_In_     HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_     LPWSTR    lpCmdLine,
                      _In_     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    //Create a new TerrainScene object 
    PostProcessingScene* PostEffectsScene = new PostProcessingScene(1268, 960);

    //Create a new System object with the scene to load and the Height and Width of the window
    System* system = new System(PostEffectsScene, hInstance, nCmdShow, PostEffectsScene->m_ViewportWidth, PostEffectsScene->m_ViewportHeight, false, false);

    //Run the System
    system->run();

    //At the end release the system object from memory
    delete system;
    system = 0;
    return 0;   
}