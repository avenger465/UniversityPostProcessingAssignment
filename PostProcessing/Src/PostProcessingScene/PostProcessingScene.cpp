#include "PostProcessingScene.h"
#include "Data/Mesh.h"
#include "Data/Model.h"
#include "BasicScene/Camera.h"
#include "Data/State.h"
#include "Shaders/Shader.h"
#include "Utility/Input.h"
#include "project/Common.h"

#include "Math/CVector2.h" 
#include "Math/CVector3.h" 
#include "Math/CMatrix4x4.h"
#include "Math/MathHelpers.h"        
#include "Utility/GraphicsHelpers.h" 
#include "Utility/ColourRGBA.h" 

#include <array>
#include <sstream>
#include <memory>

// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 1.5f;  // Radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // Units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)

//Initialisation of the class variables
PostProcessingScene::PostProcessingScene(int width, int height)
{
	m_ViewportWidth = width;
	m_ViewportHeight = height;
	resourceManager = new CResourceManager();
	MainCamera = new Camera();
	m_FisheyeCamera = new Camera();

	m_WindowFlags |= ImGuiWindowFlags_NoScrollbar;
	m_WindowFlags |= ImGuiWindowFlags_AlwaysAutoResize;
	m_WindowFlags |= ImGuiWindowFlags_NoMove;

	m_SceneTexture = 0;
	m_SecondPassTexture = 0;
	m_HorizontalBlurTexture = 0;
	m_VerticalBlurTexture = 0;
	m_CameraTexture = 0;
}

// Prepare the geometry required for the scene
// Returns true on success
bool PostProcessingScene::InitGeometry(std::string& LastError)
{
	////--------------- Load meshes ---------------////

	try
	{
		resourceManager->loadMesh(L"StarsMesh", std::string("Data/Stars.x"));
		resourceManager->loadMesh(L"GroundMesh", std::string("Data/Hills.x"));
		resourceManager->loadMesh(L"CubeMesh", std::string("Data/Cube.x"));
		resourceManager->loadMesh(L"Wall1Mesh", std::string("Data/Wall1.x"));
		resourceManager->loadMesh(L"Wall2Mesh", std::string("Data/Wall2.x"));
		resourceManager->loadMesh(L"LightMesh", std::string("Data/Light.x"));
		resourceManager->loadMesh(L"ContainerMesh", std::string("Data/CargoContainer.x"));
		resourceManager->loadMesh(L"TeapotMesh", std::string("Data/Teapot.x"));
		resourceManager->loadMesh(L"TrollMesh", std::string("Data/Troll.x"));
	}
	catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
	{
		LastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
		return false;
	}

	////--------------- Load / prepare textures & GPU states ---------------////

	try
	{
		resourceManager->loadTexture(L"StarsTexture", std::string("Media/Stars.jpg"));
		resourceManager->loadTexture(L"BricksTexture", std::string("Media/brick_35.jpg"));

		resourceManager->loadTexture(L"GroundTexture", std::string("Data/GrassDiffuseSpecular.dds"));
		resourceManager->loadTexture(L"CubeTexture", std::string("Data/StoneDiffuseSpecular.dds"));
		resourceManager->loadTexture(L"WallsTexture", std::string("Data/CargoA.dds"));
		resourceManager->loadTexture(L"LightsTexture", std::string("Media/Flare.jpg"));
		resourceManager->loadTexture(L"ContainerTexture", std::string("Data/CargoA.dds"));
		resourceManager->loadTexture(L"TeapotTexture", std::string("Data/StoneDiffuseSpecular.dds"));
		resourceManager->loadTexture(L"TrollTexture", std::string("Data/TrollDiffuseSpecular.dds"));
		
		resourceManager->loadTexture(L"NoiseMap", std::string("Media/Noise.png"));
		resourceManager->loadTexture(L"DistortMap", std::string("Media/Distort.png"));
		
		resourceManager->loadTexture(L"SpadeAlphaMap", std::string("Media/SpadeAlphaMap.png"));
		resourceManager->loadTexture(L"CloverAlphaMap", std::string("Media/CloverAlphaMap.png"));
		resourceManager->loadTexture(L"HeartAlphaMap", std::string("Media/HeartAlphaMap.png"));
	}
	catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
	{
		LastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
		return false;
	}

	////--------------- Prepare render textures, shaders and constant buffers to communicate with them ---------------////

	if (!CreateRenderTextures(LastError))
	{
		LastError = "Error loading Render Textures";
		return false;
	}
	
	// Load the shaders required for the geometry we will use (see Shader.cpp / .h)
	if (!LoadShaders(LastError))
	{
		LastError = "Error loading shaders";
		return false;
	}

	// Create GPU-side constant buffers to receive the gPerFrameConstants and PerModelConstants structures above
	// These allow us to pass data from CPU to shaders such as lighting information or matrices
	// See the comments above where these variable are declared and also the UpdateScene function
	PerFrameConstantBuffer       = CreateConstantBuffer(sizeof(PerFrameConstants));
	PerModelConstantBuffer       = CreateConstantBuffer(sizeof(gPerModelConstants));
	PostProcessingConstantBuffer = CreateConstantBuffer(sizeof(PostProcessingConstants));
	if (PerFrameConstantBuffer == nullptr || PerModelConstantBuffer == nullptr || PostProcessingConstantBuffer == nullptr)
	{
		LastError = "Error creating constant buffers";
		return false;
	}

	// Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
	if (!CreateStates(LastError))
	{
		LastError = "Error creating states";
		return false;
	}
	return true;
}

// Prepare the scene
// Returns true on success
bool PostProcessingScene::InitScene()
{
	////--------------- Set up scene ---------------////

	// Creation of Models in the scene
	m_StarsModel	 = new Model(resourceManager->getMesh(L"StarsMesh"));
	m_GroundModel	 = new Model(resourceManager->getMesh(L"GroundMesh"));
	m_CubeModel		 = new Model(resourceManager->getMesh(L"CubeMesh"));
	m_Wall1Model	 = new Model(resourceManager->getMesh(L"Wall1Mesh"));
	m_Wall2Model	 = new Model(resourceManager->getMesh(L"Wall2Mesh"));
	m_ContainerModel = new Model(resourceManager->getMesh(L"ContainerMesh"));
	m_TeapotModel	 = new Model(resourceManager->getMesh(L"TeapotMesh"));
	m_TrollModel	 = new Model(resourceManager->getMesh(L"TrollMesh"));

	// Initial positions
	
	m_Wall1Model->SetPosition({ 41.85, 0, 36.9 });
	m_Wall1Model->SetRotation({ 0.0f, ToRadians(90.0f), 0.0f });
	m_Wall1Model->SetScale(40.0f);

	m_Wall2Model->SetPosition({ 10, 0, 68 });
	m_Wall2Model->SetScale(40.0f);
	
	
	m_CubeModel->SetScale(1.5f);
	m_CubeModel->SetPosition({ m_Wall2Model->Position().x - 15, 7, m_Wall2Model->Position().z + 40 });
	m_CubeModel->SetRotation({ 0.0f, ToRadians(-110.0f), 0.0f });

	m_ContainerModel->SetPosition({90.0, 1.0f, 30.0f});
	m_ContainerModel->SetScale(5.0f);

	m_TeapotModel->SetScale(1.5f);
	m_TeapotModel->SetPosition({ 40.0f, 1.0f, 130 });

	m_TrollModel->SetScale(5.0f);
	m_TrollModel->SetPosition({5.0f, 1.0f, 5.0f});

	m_StarsModel->SetScale(8000.0f);

	// Calculate the Matrices for the polygons that will go into the windows in the wall

	m_SquareMatrix = MatrixTranslation({ m_Wall1Model->Position().x, 10, m_Wall1Model->Position().z });
	m_SquareMatrix = MatrixRotationY(ToRadians(90)) * m_SquareMatrix;

	m_HeartMatrix = MatrixTranslation({ m_Wall2Model->Position().x + 18, 10, m_Wall2Model->Position().z});

	m_SpadeMatrix = MatrixTranslation({ m_Wall2Model->Position().x - 18, 10, m_Wall2Model->Position().z});

	m_DiamondMatrix = MatrixTranslation({ m_Wall2Model->Position().x - 5, 10, m_Wall2Model->Position().z});

	m_CloverMatrix = MatrixTranslation({ m_Wall2Model->Position().x + 6, 10, m_Wall2Model->Position().z});

	// Light set-up - using an array this time
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		Lights[i].model = new Model(resourceManager->getMesh(L"LightMesh"));
	}

	Lights[0].colour = { 0.8f, 0.8f, 1.0f };
	Lights[0].strength = 10;
	Lights[0].model->SetPosition({ 30, 10, 0 });
	Lights[0].model->SetScale(pow(Lights[0].strength, 1.0f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.

	Lights[1].colour = { 1.0f, 0.8f, 0.2f };
	Lights[1].strength = 40;
	Lights[1].model->SetPosition({ 17.5f, 30, 100 });
	Lights[1].model->SetScale(pow(Lights[1].strength, 1.0f));

	Lights[2].colour = { 1.0f, 0.1f, 0.7f };
	Lights[2].strength = 40;
	Lights[2].model->SetPosition({ 90, 30, 30 });
	Lights[2].model->SetScale(pow(Lights[1].strength, 1.0f));

	////--------------- Set up cameras ---------------////

	MainCamera->SetPosition({ 25, 18, -45 });
	MainCamera->SetRotation({ ToRadians(10.0f), ToRadians(7.0f), 0.0f });

	m_FisheyeCamera->SetPosition({ m_Wall2Model->Position().x - 18, 10, m_Wall2Model->Position().z + 1 });
	
	//Set up the Post-Processing constants that do not need to be changed during the scene
	gPostProcessingConstants.tintColour1 = m_Colour1;
	gPostProcessingConstants.tintColour2 = m_Colour2;	
	gPostProcessingConstants.LuminanceWeights = CVector3(0.2126f, 0.7152f, 0.0722f);

	return true;
}

// Release the geometry and scene resources created above
void PostProcessingScene::ReleaseResources()
{
	ReleaseStates();

	if (PostProcessingConstantBuffer)  PostProcessingConstantBuffer->Release();
	if (PerModelConstantBuffer)        PerModelConstantBuffer->Release();
	if (PerFrameConstantBuffer)        PerFrameConstantBuffer->Release();

	if (m_SceneTexture)			   m_SceneTexture->Shutdown();
	if (m_SecondPassTexture)       m_SecondPassTexture->Shutdown();
	if (m_CameraTexture)           m_CameraTexture->Shutdown();
	if (m_SquareHolePostProcessTexture)   m_SquareHolePostProcessTexture->Shutdown();
	if (m_HorizontalBlurTexture)   m_HorizontalBlurTexture->Shutdown();
	if (m_VerticalBlurTexture)     m_VerticalBlurTexture->Shutdown();
	
	ReleaseShaders();

	resourceManager->~CResourceManager();

	// See note in InitGeometry about why we're not using unique_ptr and having to manually delete
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		delete Lights[i].model;  Lights[i].model = nullptr;
	}
	delete MainCamera;  MainCamera = nullptr;
	delete m_FisheyeCamera;  m_FisheyeCamera = nullptr;
	delete m_Wall1Model;   m_Wall1Model = nullptr;
	delete m_Wall2Model;   m_Wall2Model = nullptr;
	delete m_CubeModel;    m_CubeModel = nullptr;
	delete m_GroundModel;  m_GroundModel = nullptr;
	delete m_StarsModel;   m_StarsModel = nullptr;
}

// Render everything in the scene from the given camera
void PostProcessingScene::RenderSceneFromCamera(Camera* camera)
{
	// Set camera matrices in the constant buffer and send over to GPU
	PerFrameConstants.cameraMatrix = camera->WorldMatrix();
	PerFrameConstants.viewMatrix = camera->ViewMatrix();
	PerFrameConstants.projectionMatrix = camera->ProjectionMatrix();
	PerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
	UpdateConstantBuffer(PerFrameConstantBuffer, PerFrameConstants);

	// Indicate that the constant buffer we just updated is for use in the vertex shader (VS), geometry shader (GS) and pixel shader (PS)
	gD3DContext->VSSetConstantBuffers(0, 1, &PerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
	gD3DContext->GSSetConstantBuffers(0, 1, &PerFrameConstantBuffer);
	gD3DContext->PSSetConstantBuffers(0, 1, &PerFrameConstantBuffer);

	gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);

	////--------------- Render ordinary models ---------------///

	// Render lit models, only change textures for each one
	m_GroundModel->Setup(gPixelLightingVertexShader, gPixelLightingPixelShader);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)
	m_GroundModel->SetStates(gNoBlendingState, gUseDepthBufferState, gCullBackState);
	gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);
	
	m_GroundModel->SetShaderResources(0, resourceManager->getTexture(L"GroundTexture"));
	m_GroundModel->Render(PerModelConstantBuffer, gPerModelConstants);

	m_Wall1Model->SetShaderResources(0, resourceManager->getTexture(L"BricksTexture"));
	m_Wall1Model->Render(PerModelConstantBuffer, gPerModelConstants);
	
	m_Wall2Model->SetShaderResources(0, resourceManager->getTexture(L"BricksTexture"));
	m_Wall2Model->Render(PerModelConstantBuffer, gPerModelConstants);

	m_CubeModel->SetShaderResources(0, resourceManager->getTexture(L"CubeTexture"));
	m_CubeModel->Render(PerModelConstantBuffer, gPerModelConstants);
	
	m_ContainerModel->SetShaderResources(0, resourceManager->getTexture(L"ContainerTexture"));
	m_ContainerModel->Render(PerModelConstantBuffer, gPerModelConstants);

	m_TeapotModel->SetShaderResources(0, resourceManager->getTexture(L"TeapotTexture"));
	m_TeapotModel->Render(PerModelConstantBuffer, gPerModelConstants);
	
	m_TrollModel->SetShaderResources(0, resourceManager->getTexture(L"TrollTexture"));
	m_TrollModel->Render(PerModelConstantBuffer, gPerModelConstants);


	////--------------- Render sky ---------------////

	// Select which shaders to use next

	// Using a pixel shader that tints the texture - don't need a tint on the sky so set it to white
	gPerModelConstants.objectColour = { 1, 1, 1 };

	// Render sky
	m_StarsModel->Setup(gBasicTransformVertexShader, gTintedTexturePixelShader);
	m_StarsModel->SetStates(gNoBlendingState, gUseDepthBufferState, gCullNoneState);
	m_StarsModel->SetShaderResources(0, resourceManager->getTexture(L"StarsTexture"));
	m_StarsModel->Render(PerModelConstantBuffer, gPerModelConstants);

	////--------------- Render lights ---------------////
	// Render all the lights in the array
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		Lights[i].model->Setup(gBasicTransformVertexShader, gTintedTexturePixelShader);
		Lights[i].model->SetStates(gAdditiveBlendingState, gDepthReadOnlyState, gCullNoneState);
		Lights[i].model->SetShaderResources(0, resourceManager->getTexture(L"LightsTexture"));
		gPerModelConstants.objectColour = Lights[i].colour; // Set any per-model constants apart from the world matrix just before calling render (light colour here)
		Lights[i].model->Render(PerModelConstantBuffer, gPerModelConstants);
	}
}

// Select the appropriate shader plus any additional textures required for a given post-process
// Helper function shared by full-screen, area and polygon post-processing functions below
void PostProcessingScene::SelectPostProcessShaderAndTextures(PostProcess postProcess)
{
	if (postProcess == PostProcess::Copy)
	{
		gD3DContext->PSSetShader(gCopyPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::Gradient)
	{
		gD3DContext->PSSetShader(gColourGradientPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::GreyNoise)
	{	
		gD3DContext->PSSetShader(gGreyNoisePostProcess, nullptr, 0);
		ID3D11ShaderResourceView* temp = resourceManager->getTexture(L"NoiseMap");
		gD3DContext->PSSetShaderResources(1, 1, &temp);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
		temp = resourceManager->getTexture(L"HeartAlphaMap");
		gD3DContext->PSSetShaderResources(2, 1, &temp);
		
	}
	else if (postProcess == PostProcess::Distort)
	{
		gD3DContext->PSSetShader(gDistortPostProcess, nullptr, 0);
		ID3D11ShaderResourceView* temp = resourceManager->getTexture(L"DistortMap");
		gD3DContext->PSSetShaderResources(1, 1, &temp);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
		temp = resourceManager->getTexture(L"CloverAlphaMap");
		gD3DContext->PSSetShaderResources(2, 1, &temp);
	}
	else if (postProcess == PostProcess::Fisheye)
	{
		gD3DContext->PSSetShader(gFishEyeShader, nullptr, 0);
	}
	else if (postProcess == PostProcess::Saturation)
	{
		gD3DContext->PSSetShader(gSaturationPostProcess, nullptr, 0);
		ID3D11ShaderResourceView* temp = resourceManager->getTexture(L"SpadeAlphaMap");
		gD3DContext->PSSetShaderResources(1, 1, &temp);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}
	else if (postProcess == PostProcess::Underwater)
	{
		gD3DContext->PSSetShader(gUnderWaterPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::Pixelation)
	{
		gD3DContext->PSSetShader(gPixelationPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::Vignette)
	{
		gD3DContext->PSSetShader(gVignettePostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::HorizontalBlur)
	{
		gD3DContext->PSSetShader(gHorizontalBlurPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::HorizontalBlur)
	{
		gD3DContext->PSSetShader(gVerticalBlurPostProcess, nullptr, 0);
	}
	else
	{
		gD3DContext->PSSetShader(gCopyPostProcess, nullptr, 0);
	}
}

//Common rendering settings when rendering a post-process
void PostProcessingScene::FirstRender(ID3D11VertexShader* VertexShader)
{
	gD3DContext->PSSetSamplers(0, 1, &gPointSampler); // Use point sampling (no bilinear, trilinear, mip-mapping etc. for most post-processes)

	// Using special vertex shader that creates its own data for a 2D screen quad
	gD3DContext->VSSetShader(VertexShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)

	// States - no blending, don't write to depth buffer and ignore back-face culling
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gNoDepthBufferState, 0);
	gD3DContext->RSSetState(gCullNoneState);

	// No need to set vertex/index buffer (see 2D quad vertex shader), just indicate that the quad will be created as a triangle strip
	gD3DContext->IASetInputLayout(NULL); // No vertex data
	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// Select shader and textures needed for the required post-processes (helper function above)

	// Set 2D area for full-screen post-processing (coordinates in 0->1 range)
	gPostProcessingConstants.area2DTopLeft = { 0, 0 }; // Top-left of entire screen
	gPostProcessingConstants.area2DSize = { 1, 1 }; // Full size of screen
	gPostProcessingConstants.area2DDepth = 0;        // Depth buffer value for full screen is as close as possible


	// Pass over the above post-processing settings (also the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
}

//Create the 2D texture for the post-processes to be rendered to
bool PostProcessingScene::CreateRenderTextures(std::string& LastError)
{
	bool result;

	m_SceneTexture = new CRenderTexture;
	if (!m_SceneTexture) return false;
	result = m_SceneTexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating Scene Texture";
		return false;
	}

	m_SecondPassTexture = new CRenderTexture;
	if (!m_SecondPassTexture) return false;
	result = m_SecondPassTexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating SecondPass Texture";
		return false;
	}
	m_HorizontalBlurTexture = new CRenderTexture;
	if (!m_HorizontalBlurTexture) return false;
	result = m_HorizontalBlurTexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating Horizontal blur texture";
		return false;
	}
	m_VerticalBlurTexture = new CRenderTexture;
	if (!m_VerticalBlurTexture) return false;
	result = m_VerticalBlurTexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating Vertical blur texture";
		return false;
	}
	m_CameraTexture = new CRenderTexture;
	if (!m_CameraTexture) return false;
	result = m_CameraTexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating Vertical blur texture";
		return false;
	}

	m_SquareHolePostProcessTexture = new CRenderTexture;
	if (!m_SquareHolePostProcessTexture) return false;
	result = m_SquareHolePostProcessTexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating Vertical blur texture";
		return false;
	}
	return true;
}

//Post-process rendering full-screen
void PostProcessingScene::FullScreenPostProcess(PostProcess postProcess, ID3D11ShaderResourceView* renderResource)
{
	//Used to get a temporary pointer to the shader resource view to be used
	ID3D11ShaderResourceView* currentShaderTexture;

	//Check if the current post-process is a horizontal blur
	if (postProcess == PostProcess::HorizontalBlur)
	{
		//Perform a horizontal blur to the HorizontalBlurTexture from the scene Texture
		m_HorizontalBlurTexture->SetRenderTarget(gD3DContext, gDepthStencil);
		m_HorizontalBlurTexture->ClearRenderTarget(gD3DContext, gBackgroundColor);

		currentShaderTexture = renderResource;//m_Scenetexture->GetShaderResourceView();
		gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

		//Setup the common settings for rendering post-processes
		FirstRender(g2DQuadVertexShader);

		//Select the Horizontal blur shader
		SelectPostProcessShaderAndTextures(PostProcess::HorizontalBlur);

		//// Draw a quad
		gD3DContext->Draw(4, 0);

		//Perform a vertical blur to the VerticalBlurTexture from the Horizontal Texture
		m_VerticalBlurTexture->SetRenderTarget(gD3DContext, gDepthStencil);
		
		currentShaderTexture = m_HorizontalBlurTexture->GetShaderResourceView();
		gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);
		
		//Select the Vertical blur shader
		SelectPostProcessShaderAndTextures(PostProcess::VerticalBlur);

		//// Draw a quad
		gD3DContext->Draw(4, 0);

		//Perform a copy of the blurred texture to the SecondPass texture that will be used by the back buffer later
		m_SecondPassTexture->SetRenderTarget(gD3DContext, gDepthStencil);

		//Select the copy shader
		SelectPostProcessShaderAndTextures(PostProcess::Copy);

		currentShaderTexture = m_VerticalBlurTexture->GetShaderResourceView();
		gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

		//// Draw a quad
		gD3DContext->Draw(4, 0);
	}
	else
	{
		//Perform the selected post process to the SecondPass texture that will be used by the back buffer later
		m_SecondPassTexture->SetRenderTarget(gD3DContext, gDepthStencil);
		m_SecondPassTexture->ClearRenderTarget(gD3DContext, gBackgroundColor);
		
		currentShaderTexture = renderResource;//m_Scenetexture->GetShaderResourceView();
		gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);
		
		//Setup the common settings for rendering post-processes
		FirstRender(g2DQuadVertexShader);

		//Select the appropriate shader for the currently selected post process
		SelectPostProcessShaderAndTextures(postProcess);

		//// Draw a quad
		gD3DContext->Draw(4, 0);
		
	}
		
	//Select copy shader
	SelectPostProcessShaderAndTextures(PostProcess::Copy);
	
	//If the blur post-process is activated, add alpha blending to introduce feedback to the scene
	if (postProcess == PostProcess::HorizontalBlur)
	{
		gD3DContext->OMSetBlendState(gAlphaBlendingState, nullptr, 0xffffff);
	}
	
	//Draw the rendered texture to the back buffer
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	currentShaderTexture = m_SecondPassTexture->GetShaderResourceView();
	gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

	//// Draw a quad
	gD3DContext->Draw(4, 0);
}

// Perform an post process from "scene texture" to back buffer within the given four-point polygon and a world matrix to position/rotate/scale the polygon
void PostProcessingScene::PolygonPostProcess()
{
	// First perform a full-screen copy of the scene to back-buffer
	FullScreenPostProcess(PostProcess::Copy, m_SceneTexture->GetShaderResourceView());
	
	//Select the shader required for the Saturation effect
	SelectPostProcessShaderAndTextures(PostProcess::Saturation);

	//Get a reference to the Spade Alpha Map
	ID3D11ShaderResourceView* temporary = resourceManager->getTexture(L"SpadeAlphaMap");
	gD3DContext->PSSetShaderResources(1, 1, &temporary);
	
	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < m_SpadeWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(m_SpadeWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * m_SpadeMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	
	// Pass over the polygon points to the shaders (also sends the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);	
	gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	
	// Select the special 2D polygon post-processing vertex shader and draw the polygon
	gD3DContext->VSSetShader(g2DPolygonVertexShader, nullptr, 0);
	gD3DContext->Draw(4, 0);

	//Select the shader required for the Grey Noise effect
	SelectPostProcessShaderAndTextures(PostProcess::GreyNoise);

	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < m_HeartWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(m_HeartWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * m_HeartMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}

	// Pass over the new polygon points to the shaders 
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);

	//// Draw a quad
	gD3DContext->Draw(4, 0);
	
	//Select the shader required for the Vignette effect
	SelectPostProcessShaderAndTextures(PostProcess::Vignette);

	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < m_DiamondWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(m_DiamondWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * m_DiamondMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}

	// Pass over the new polygon points to the shaders 
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);

	//// Draw a quad
	gD3DContext->Draw(4, 0);

	//Select the shader required for the Distort effect
	SelectPostProcessShaderAndTextures(PostProcess::Distort);

	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < m_CloverWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(m_CloverWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * m_CloverMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	
	// Pass over the new polygon points to the shaders 
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);

	//// Draw a quad
	gD3DContext->Draw(4, 0);

	//Select the shader required for the Fisheye effect
	//and the Fisheye texture to use in the shader
	SelectPostProcessShaderAndTextures(PostProcess::Fisheye);
	ID3D11ShaderResourceView* temp = m_SquareHolePostProcessTexture->GetShaderResourceView();
	gD3DContext->PSSetShaderResources(0, 1, &temp);
	
	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < m_SquarePoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(m_SquarePoints[i], 1);
		CVector4 worldPosition = modelPosition * m_SquareMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}

	// Pass over the new polygon points to the shaders 
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);

	//// Draw a quad
	gD3DContext->Draw(4, 0);
}

// Rendering the scene
void PostProcessingScene::RenderScene(float frameTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	//// Common settings ////

	// Set up the light information in the constant buffer
	// Don't send to the GPU yet, the function RenderSceneFromCamera will do that
	PerFrameConstants.light1Colour   = Lights[0].colour * Lights[0].strength;
	PerFrameConstants.light1Position = Lights[0].model->Position();
	PerFrameConstants.light2Colour   = Lights[1].colour * Lights[1].strength;
	PerFrameConstants.light2Position = Lights[1].model->Position();
	PerFrameConstants.light3Colour	 = Lights[2].colour * Lights[2].strength;
	PerFrameConstants.light3Position = Lights[2].model->Position();

	PerFrameConstants.ambientColour  = gAmbientColour;
	PerFrameConstants.specularPower  = gSpecularPower;
	PerFrameConstants.cameraPosition = MainCamera->Position();

	PerFrameConstants.viewportWidth  = static_cast<float>(m_ViewportWidth);
	PerFrameConstants.viewportHeight = static_cast<float>(m_ViewportHeight);
	
	//Check if a post-process is currently selected
	if (CurrentPostProcess != PostProcess::None)
	{
		m_SceneTexture->SetRenderTarget(gD3DContext, gDepthStencil);
		m_SceneTexture->ClearRenderTarget(gD3DContext, gBackgroundColor);
	}
	else
	{
		gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
		gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
	}
	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Setup the viewport to the size of the main window
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(m_ViewportWidth);
	vp.Height = static_cast<FLOAT>(m_ViewportHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	gD3DContext->RSSetViewports(1, &vp);

	// Render the scene from the main camera
	RenderSceneFromCamera(MainCamera);


	// Run any post-processing steps

	gPostProcessingConstants.Epsilon = 1e-10;

	//Check if a post-process is currently selected
	if (CurrentPostProcess != PostProcess::None)
	{
		if (CurrentPostProcessMode == PostProcessMode::Fullscreen)
		{		
			//Render the current post-processing effect to the screen
			FullScreenPostProcess(CurrentPostProcess, m_SceneTexture->GetShaderResourceView());
		}

		else if (CurrentPostProcessMode == PostProcessMode::Polygon)
		{				
			//Render the scene from the Fisheye cameras perspective to the CameraTexture
			m_CameraTexture->SetRenderTarget(gD3DContext);
			m_CameraTexture->ClearRenderTarget(gD3DContext, gBackgroundColor);

			m_FisheyeCamera->SetPosition({ m_Wall1Model->Position().x, 10, m_Wall1Model->Position().z });
			m_FisheyeCamera->SetRotation(CVector3(0.0f, ToRadians(90), 0.0f));
			RenderSceneFromCamera(m_FisheyeCamera);
			
			//Use this new texture as the source for the post-processing of the fisheye polygon
			m_SquareHolePostProcessTexture->SetRenderTarget(gD3DContext, m_CameraTexture->GetDepthStencilView());
			FirstRender(g2DQuadVertexShader);
			SelectPostProcessShaderAndTextures(PostProcess::Fisheye);
			ID3D11ShaderResourceView* temp = m_CameraTexture->GetShaderResourceView();
			gD3DContext->PSSetShaderResources(0, 1, &temp);
			gD3DContext->Draw(4, 0);

			//render the post-processes to all the polygons 
			PolygonPostProcess();
		}
		
		// These lines unbind the scene texture from the pixel shader to stop DirectX issuing a warning when we try to render to it again next frame
		ID3D11ShaderResourceView* nullSRV = nullptr;
		gD3DContext->PSSetShaderResources(0, 1, &nullSRV);
	}
	
	//Render the ImGui window
	IMGUI();
	ImGui::Render();
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
	// Set first parameter to 1 to lock to vsync
	gSwapChain->Present(m_LockFPS ? 1 : 0, 0);
}

// Update models and camera. frameTime is the time passed since the last frame
void PostProcessingScene::UpdateScene(float frameTime, HWND HWnd)
{
	// Select post process on keys
	if (KeyHit(Key_F1))  CurrentPostProcessMode = PostProcessMode::Fullscreen;
	if (KeyHit(Key_F2))  CurrentPostProcessMode = PostProcessMode::Polygon;

	if (KeyHit(Key_1))   CurrentPostProcess = PostProcess::Gradient;
	if (KeyHit(Key_2))   CurrentPostProcess = PostProcess::HorizontalBlur;
	if (KeyHit(Key_3))   CurrentPostProcess = PostProcess::Underwater;
	if (KeyHit(Key_4))   CurrentPostProcess = PostProcess::Pixelation;
	if (KeyHit(Key_0))   CurrentPostProcess = PostProcess::None;

	// Noise scaling adjusts how fine the grey noise is.
	const float grainSize = 50; // Fineness of the noise grain
	gPostProcessingConstants.noiseScale  = { m_ViewportWidth / grainSize, m_ViewportHeight / grainSize };

	// The noise offset is randomised to give a constantly changing noise effect (like tv static)
	gPostProcessingConstants.noiseOffset = { Random(0.0f, 1.0f), Random(0.0f, 1.0f) };

	// Set the level of distortion
	gPostProcessingConstants.distortLevel = 0.01f;

	//Updating the number of the pixels in the post-process
	gPostProcessingConstants.PixelWidth = m_PixelWidth;
	gPostProcessingConstants.PixelHeight = m_PixelWidth;

	//Updating the level of blur
	gPostProcessingConstants.BlurOffset = m_BlurOffset;

	gPostProcessingConstants.Feedback = m_Feedback;
	
	//Updating the resolution of the blur to the Blur offset
	gPostProcessingConstants.BlurHeight = m_BlurOffset;
	gPostProcessingConstants.BlurWidth = m_BlurOffset * 1.3;
	
	//Updating the vignette effects
	gPostProcessingConstants.vignetteStrength = m_VignetteStrength;
	gPostProcessingConstants.vignetteSize = m_VignetteSize;
	gPostProcessingConstants.vignetteFalloff = m_VignetteFalloff;

	//Updating the level of saturation used in the saturation effect
	gPostProcessingConstants.SaturationLevel = m_SaturationLevel;
	static float wiggle = 0.0f;
	const float wiggleSpeed = 1.0f;
	wiggle += wiggleSpeed * frameTime;

	// Update the UnderwaterEffect timer
	gPostProcessingConstants.UnderwaterEffect = wiggle;

	// Orbit one light - a bit of a cheat with the static variable [ask the tutor if you want to know what this is]
	static float lightRotate = 0.0f;
	static bool go = true;
	Lights[0].model->SetPosition({ 20 + cos(lightRotate) * LightOrbitRadius, 10, 20 + sin(lightRotate) * LightOrbitRadius });
	if (go)  lightRotate -= LightOrbitSpeed * frameTime;
	if (KeyHit(Key_L))  go = !go;

	// Control of camera
	MainCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D);

	// Toggle FPS limiting
	if (KeyHit(Key_P))  m_LockFPS = !m_LockFPS;

	// Show frame time / FPS in the window title //
	const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
	static float totalFrameTime = 0;
	static int frameCount = 0;
	totalFrameTime += frameTime;
	++frameCount;
	if (totalFrameTime > fpsUpdateTime)
	{
		// Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
		float avgFrameTime = totalFrameTime / frameCount;
		std::ostringstream frameTimeMs;
		frameTimeMs.precision(2);
		frameTimeMs << std::fixed << avgFrameTime * 1000;
		std::string windowTitle = "Post Processing - Frame Time: " + frameTimeMs.str() +
			"ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
		SetWindowTextA(HWnd, windowTitle.c_str());
		totalFrameTime = 0;
		frameCount = 0;
	}
}

//creation of the ImGui window elements 
void PostProcessingScene::IMGUI()
{
	//Start an ImGui window
	ImGui::Begin("Information", 0, m_WindowFlags);
	
	//Information about the camera's position and rotation
	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", MainCamera->Position().x, MainCamera->Position().y, MainCamera->Position().z);
	ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)", MainCamera->Rotation().x, MainCamera->Rotation().y, MainCamera->Rotation().z);
	ImGui::Separator();
	ImGui::Text("");

	//Activate the post-processes for full-screen
	if (ImGui::Button("(F1)Fullscreen", m_ButtonSize))
	{
		CurrentPostProcessMode = PostProcessMode::Fullscreen;
		CurrentPostProcess = PostProcess::Copy;
	}
	ImGui::SameLine();
	
	//Activate all the processes for the windows in the walls
	if (ImGui::Button("(F2)Polygon", m_ButtonSize))
	{
		CurrentPostProcessMode = PostProcessMode::Polygon;
		CurrentPostProcess = PostProcess::Copy;
	}
	ImGui::Separator();
	ImGui::Text("");
	//Activate the Gradient effect when the button is pressed
	if (ImGui::Button("(1)Gradient Effect", m_ButtonSize))
	{
		CurrentPostProcess = PostProcess::Gradient;

	}
	ImGui::SameLine();
	
	//Activate the Blur effect when the button is pressed
	if (ImGui::Button("(2)Blur Effect", m_ButtonSize))
	{
		CurrentPostProcess = PostProcess::HorizontalBlur;

	}

	//Activate the Underwater effect when the button is pressed
	if (ImGui::Button("(3)Underwater Effect", m_ButtonSize))
	{
		CurrentPostProcess = PostProcess::Underwater;

	}
	ImGui::SameLine();

	//Activate the Gameboy effect when the button is pressed
	if (ImGui::Button("(4)Gameboy Effect", m_ButtonSize))
	{
		CurrentPostProcess = PostProcess::Pixelation;

	}
	ImGui::Separator();
	ImGui::Text("");
		
	//Sliders to update the Blur post processing constants
	ImGui::SliderInt("Blur divider", &m_BlurOffset, 100, 960);
	ImGui::SliderFloat("Feedback", &m_Feedback, 0.0f, 1.0);
	ImGui::Separator();		

	//Slider to update the Gameboy post processing constants
	ImGui::Text("");
	ImGui::SliderInt("Pixel Size", &m_PixelWidth, 40, 128);
	ImGui::Separator();
	
	//Slider to update the Saturation post processing constants
	ImGui::Text("");
	ImGui::SliderFloat("Saturation Level", &m_SaturationLevel, -1.0f, 50.0f);
	ImGui::Separator();

	//Sliders to update the Vignette post processing constants
	ImGui::Text("");
	ImGui::SliderFloat("Vignette Strength", &m_VignetteStrength, 0.0f, 2.5f);
	ImGui::SliderFloat("Vignette Size", &m_VignetteSize, 0.0f, 2.5f);
	ImGui::SliderFloat("Vignette Falloff", &m_VignetteFalloff, 0.0f, 2.5f);
	ImGui::Separator();

	//Reset the current post process 
	ImGui::Text("");
	if (ImGui::Button("Reset Effects", m_ButtonSize))
	{
		CurrentPostProcess = PostProcess::None;
	}	

	//Start the ImGui window
	ImGui::End();
}