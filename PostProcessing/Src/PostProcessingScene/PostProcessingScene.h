//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------
#pragma once
#include "BasicScene/BaseScene.h"
#include "System/System.h"


class PostProcessingScene : public BaseScene
{
//----------------------//
// Construction / Usage	//
//----------------------//
public:

	PostProcessingScene(int width, int height);
	//Function to setup all the geometry to be used in the scene
	virtual bool InitGeometry(std::string& LastError) override;

	//Function to setup the scene 
	virtual bool InitScene() override;

	// Release the geometry resources created above
	virtual void ReleaseResources() override;

	//Function to render scene from the Camera's perspective
	virtual void RenderSceneFromCamera(Camera* camera) override;

	//--------------------------------------------------------------------------------------
	// Scene Render and Update
	//--------------------------------------------------------------------------------------

	//Function to render the scene, called every frame
	virtual void RenderScene(float frameTime) override;

	// frameTime is the time passed since the last frame
	virtual void UpdateScene(float frameTime, HWND HWnd) override;

	//Function to contain all of the ImGui code
	virtual void IMGUI() override;

public:
	int m_ViewportWidth;
	int m_ViewportHeight;
	

private: 
	enum class PostProcess
	{
		None,
		Copy,
		Gradient,
		GreyNoise,
		Burn,
		Distort,
		Spiral,
		Underwater,
	};
	PostProcess CurrentPostProcess = PostProcess::None;

	enum class PostProcessMode
	{
		Fullscreen,
		Area,
		Polygon,
	};
	PostProcessMode CurrentPostProcessMode = PostProcessMode::Fullscreen;

	struct Light
	{
		Model* model;
		CVector3 colour;
		float    strength;
	};
	Light Lights[2];

private:
	void PolygonPostProcess(PostProcess postProcess, std::vector<CVector3>& points, const CMatrix4x4& worldMatrix);

	void SelectPostProcessShaderAndTextures(PostProcess postProcess);

	void FullScreenPostProcess(PostProcess postProcess);

	void AreaPostProcess(PostProcess postProcess, CVector3 worldPoint, CVector2 areaSize);



private:
	//****************************
// Post processing textures
	const int NUM_LIGHTS = 2;

	// Dimensions of scene texture - controls quality of rendered scene
	int textureWidth = 900;
	int textureHeight = 900;

	std::vector<CVector3> HeartWindowPoints = { { -4.5,5,0 }, { -4.5,-5,0 }, { 4.5,5,0 }, { 4.5,-5,0 } };
	std::vector<CVector3> SpadeWindowPoints = { {-4.5,5,0}, {-4.5,-5,0}, {4.5,5,0}, {4.5,-5,0} };
	std::vector<CVector3> DiamondWindowPoints = { {-3,3,0}, {-4,-4,0}, {4,4,0}, {3,-3,0} };
	std::vector<CVector3> CloverWindowPoints = { {-4.5,5,0}, {-4.5,-5,0}, {4.5,5,0}, {4.5,-5,0} };
	std::vector<CVector3> SquarePoints = { {-5,5,0}, {-5,-5,0}, {5,5,0}, {5,-5,0} };


// This texture will have the scene renderered on it. Then the texture is then used for post-processing
	ID3D11Texture2D*		  m_SceneTexture	  = nullptr;			   // This object represents the memory used by the texture on the GPU
	ID3D11RenderTargetView*   m_SceneRenderTarget = nullptr; // This object is used when we want to render to the texture above
	ID3D11ShaderResourceView* m_SceneTextureSRV   = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

	ID3D11Texture2D*		  m_SecondPassTexture	   = nullptr;
	ID3D11RenderTargetView*   m_SecondPassRenderTarget = nullptr;
	ID3D11ShaderResourceView* m_SecondPassTextureSRV   = nullptr;


	PostProcessingConstants gPostProcessingConstants;
	ID3D11Buffer* PostProcessingConstantBuffer;

	// Meshes, models and cameras, same meaning as TL-Engine. Meshes prepared in InitGeometry function, Models & camera in InitScene
	Model* gStars;
	Model* gGround;
	Model* gCube;
	Model* gWall1;
	Model* gWall2;

	CMatrix4x4 polyMatrix;
	CMatrix4x4 HeartMatrix;
	CMatrix4x4 SpadeMatrix;
	CMatrix4x4 DiamondMatrix;
	CMatrix4x4 CloverMatrix;
	//Standard size of the ImGui Button
	ImVec2 ButtonSize = { 162, 20 };

	// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
	bool lockFPS = true;

	//initial number of window flags for the ImGui Window
	ImGuiWindowFlags windowFlags = 0;
	float wi = 0;

	// Variables controlling light1's orbiting of the cube
	const float LightOrbitRadius = 20.0f;
	const float LightOrbitSpeed = 0.7f;

};