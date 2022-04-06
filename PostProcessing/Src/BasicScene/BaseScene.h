#include "project/Common.h"
#include "BasicScene/Camera.h"
#include "BasicScene/CLight.h"
#include "Utility/Input.h"
#include "Data/Mesh.h"
#include "Data/Model.h"
#include "Data/State.h"
#include "Shaders/Shader.h"

#include "Utility/ColourRGBA.h"
#include "Utility/CResourceManager.h"

#include "Math/CVector3.h"
#include "Math/CVector4.h"

#include <sstream>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"


#pragma once
class BaseScene
{
public:
	//--------------------------------------------------------------------------------------
	// Scene Geometry and Layout
	//--------------------------------------------------------------------------------------

	//Function to setup all the geometry to be used in the scene
	virtual bool InitGeometry(std::string& LastError) = 0;

	//Function to setup the scene 
	virtual bool InitScene() = 0;

	// Release the geometry resources created above
	virtual void ReleaseResources() = 0;

	//Function to render scene from the Camera's perspective
	virtual void RenderSceneFromCamera(Camera* camera) = 0;

	//--------------------------------------------------------------------------------------
	// Scene Render and Update
	//--------------------------------------------------------------------------------------
	//Function to render the scene, called every frame
	virtual void RenderScene(float frameTime) = 0;

	// frameTime is the time passed since the last frame
	virtual void UpdateScene(float frameTime, HWND HWnd) = 0;

	//Function to contain all of the ImGui code
	virtual void IMGUI() = 0;

protected:

	PerFrameConstants PerFrameConstants;
	ID3D11Buffer* PerFrameConstantBuffer;

	PerModelConstants gPerModelConstants;
	ID3D11Buffer* PerModelConstantBuffer;

	CResourceManager* resourceManager;
	Camera* MainCamera;
	Model* GroundModel;

	//-------------------//
	// Light Information //
	//-------------------//
	//CLight* Light;
	//float LightScale = 15000.0f;
	//CVector3 LightColour = { 0.9922f, 0.7217f, 0.0745f };
	//CVector3 LightPosition = { 5000.0f, 13000.0f, 5000.0f };

	// Additional light information
	CVector3 gAmbientColour = { 0.3f, 0.3f, 0.4f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
	float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app
	ColourRGBA gBackgroundColor = { 0.3f, 0.3f, 0.4f, 1.0f };
	ColourRGBA gFogColour = { 0.5f, 0.5f, 0.5f, 1.0f };

	bool lockFPS = true;
	std::string FPS_String;
	int FPS;
};