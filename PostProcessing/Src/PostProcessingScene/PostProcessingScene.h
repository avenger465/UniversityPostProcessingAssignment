//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------
#pragma once
#include "BasicScene/BaseScene.h"
#include "System/CRenderTexture.h"
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

	//Function to render the scene, called every frame
	virtual void RenderScene(float frameTime) override;

	// frameTime is the time passed since the last frame
	virtual void UpdateScene(float frameTime, HWND HWnd) override;

	//Function to contain all of the ImGui code
	virtual void IMGUI() override;

	//Function to create rendering textures
	virtual bool CreateRenderTextures(std::string& LastError) override;

public:
	int m_ViewportWidth;
	int m_ViewportHeight;
	

//-------------------------------------
// Private members
//-------------------------------------	
private: 
	
	//class of post-processes
	enum class PostProcess
	{
		None,
		Copy,
		Gradient,
	
		HorizontalBlur,
		VerticalBlur,
		Fisheye,
		GreyNoise,
		Distort,
		Saturation,
		Underwater,
		Pixelation,
		Vignette,
	};
	PostProcess CurrentPostProcess = PostProcess::Copy;

	//class of post-process modes
	enum class PostProcessMode
	{
		Fullscreen,
		Polygon,
		None
	};
	PostProcessMode CurrentPostProcessMode = PostProcessMode::Fullscreen;

	//return CurrentPostProcessMode as string
	std::string GetPostProcessModeString(PostProcessMode m)
	{
		switch (m)
		{
		case PostProcessingScene::PostProcessMode::Fullscreen:
			return "Fullscreen";
			break;
		case PostProcessingScene::PostProcessMode::Polygon:
			return  "Polygon";
			break;
		case PostProcessingScene::PostProcessMode::None:
			return "None";
			break;
		default:
			return "";
			break;
		}
	}
	
	struct Light
	{
		Model* model;
		CVector3 colour;
		float    strength;
	};
	Light Lights[3];

//-------------------------------------
// Private functions
//-------------------------------------	
private:
	
	//Post-process rendering to a polygon
	void PolygonPostProcess();

	//return the correct shader based on the post-process
	void SelectPostProcessShaderAndTextures(PostProcess postProcess);

	//Post-process rendering full-screen
	void FullScreenPostProcess(PostProcess postProcess, ID3D11ShaderResourceView* renderResource);

	//Common rendering settings when rendering a post-process
	void FirstRender(ID3D11VertexShader* VertexShader);
	
//-------------------------------------
// Private members
//-------------------------------------
private:
	
	const int NUM_LIGHTS = 3;
	
	// Variables controlling light1's orbiting of the cube
	const float LightOrbitRadius = 20.0f;
	const float LightOrbitSpeed = 0.7f;
	
	//Constants used in the post-processing effects
	PostProcessingConstants gPostProcessingConstants;
	ID3D11Buffer* PostProcessingConstantBuffer;
	
	// Dimensions of scene texture - controls quality of rendered scene
	int m_TextureWidth = 900;
	int m_TextureHeight = 900;

	// Polygon points for all of the holes in the walls 
	std::vector<CVector3> m_HeartWindowPoints = { { -5.5,5,0 }, { -5.5,-5,0 }, { 5.5,5,0 }, { 5.5,-5,0 } };
	std::vector<CVector3> m_SpadeWindowPoints = { {-5.5,6,0}, {-5.5,-6,0}, {5.5,6,0}, {5.5,-6,0} };
	std::vector<CVector3> m_DiamondWindowPoints = { {-1,4.65,0}, {-4.65,0,0}, {2.65,0,0}, {-1,-4.65,0} };
	std::vector<CVector3> m_CloverWindowPoints = { {-7.5,7.5,0}, {-7.5,-7.5,0}, {7.5,7.5,0}, {7.5,-7.5,0} };
	std::vector<CVector3> m_SquarePoints = { {-5,5,0}, {-5,-5,0}, {5,5,0}, {5,-5,0} };

	//Textures available to be rendered to
	CRenderTexture* m_SceneTexture;
	CRenderTexture* m_SecondPassTexture;
	CRenderTexture* m_CameraTexture;
	CRenderTexture* m_SquareHolePostProcessTexture;
	CRenderTexture* m_HorizontalBlurTexture;
	CRenderTexture* m_VerticalBlurTexture;
	
	

	//Models in the scene
	Model* m_StarsModel;
	Model* m_GroundModel;
	Model* m_CubeModel;
	Model* m_Wall1Model;
	Model* m_Wall2Model;
	Model* m_ContainerModel;
	Model* m_TeapotModel;
	Model* m_TrollModel;

	//Matrices for all of the holes in the walls
	CMatrix4x4 m_SquareMatrix;
	CMatrix4x4 m_HeartMatrix;
	CMatrix4x4 m_SpadeMatrix;
	CMatrix4x4 m_DiamondMatrix;
	CMatrix4x4 m_CloverMatrix;

	//Camera used to get the view of the Fisheye effect
	Camera* m_FisheyeCamera;

	//Standard size of the ImGui Button
	ImVec2 m_ButtonSize = { 162, 20 };

	// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
	bool m_LockFPS = true;

	//initial number of window flags for the ImGui Window
	ImGuiWindowFlags m_WindowFlags = 0;

	//Variables for the gradient colours
	CVector3 m_Colour1 = {0,0,1};
	CVector3 m_Colour2 = {0,1,0};

	//Variable for the saturation level in the saturation effect
	float m_SaturationLevel = 30.0f;
	
	//Variables to control the vignette effect
	float m_VignetteStrength = 1.3f;
	float m_VignetteSize = 0.6;
	float m_VignetteFalloff = 0.25f;

	//Variable to control the pixelation effect
	int m_PixelWidth = 64;

	//Variable to control the Blue effect
	int m_BlurOffset = 600;

	float m_Feedback = 0.5f;
};