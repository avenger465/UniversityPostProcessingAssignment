//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

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
#include "Math/MathHelpers.h"     // Helper functions for maths
#include "Utility/GraphicsHelpers.h" // Helper functions to unclutter the code here
#include "Utility/ColourRGBA.h" 

#include <array>
#include <sstream>
#include <memory>


//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------

// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 1.5f;  // Radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // Units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)

//--------------------------------------------------------------------------------------
// Initialise scene geometry, constant buffers and states
//--------------------------------------------------------------------------------------

PostProcessingScene::PostProcessingScene(int width, int height)
{
	m_ViewportWidth = width;
	m_ViewportHeight = height;
	resourceManager = new CResourceManager();
	MainCamera = new Camera();
	SpadeCamera = new Camera();

	windowFlags |= ImGuiWindowFlags_NoScrollbar;
	windowFlags |= ImGuiWindowFlags_AlwaysAutoResize;
	windowFlags |= ImGuiWindowFlags_NoMove;

	m_Scenetexture = 0;
	m_SecondPasstexture = 0;
	m_DownSampledtexture = 0;
	m_HorizontalBlurTexture = 0;
	m_VerticalBlurTexture = 0;
	m_ListOfEffects.push_back(PostProcess::Copy);

}

// Prepare the geometry required for the scene
// Returns true on success
bool PostProcessingScene::InitGeometry(std::string& LastError)
{
	bool result;
	////--------------- Load meshes ---------------////

	// Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
	try
	{
		resourceManager->loadMesh(L"StarsMesh", std::string("Data/Stars.x"));
		resourceManager->loadMesh(L"GroundMesh", std::string("Data/Ground.x"));
		resourceManager->loadMesh(L"CubeMesh", std::string("Data/Cube.x"));
		resourceManager->loadMesh(L"Wall1Mesh", std::string("Data/Wall1.x"));
		resourceManager->loadMesh(L"Wall2Mesh", std::string("Data/Wall2.x"));
		resourceManager->loadMesh(L"LightMesh", std::string("Data/Light.x"));
	}
	catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
	{
		LastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
		return false;
	}


	////--------------- Load / prepare textures & GPU states ---------------////

	// Load textures and create DirectX objects for them
	// The LoadTexture function requires you to pass a ID3D11Resource* (e.g. &gCubeDiffuseMap), which manages the GPU memory for the
	// texture and also a ID3D11ShaderResourceView* (e.g. &gCubeDiffuseMapSRV), which allows us to use the texture in shaders
	// The function will fill in these pointers with usable data. The variables used here are globals found near the top of the file.


	try
	{
		resourceManager->loadTexture(L"StarsTexture", std::string("Media/Stars.jpg"));
		resourceManager->loadTexture(L"BricksTexture", std::string("Media/brick_35.jpg"));

		resourceManager->loadTexture(L"GroundTexture", std::string("Data/GrassDiffuseSpecular.dds"));
		resourceManager->loadTexture(L"CubeTexture", std::string("Data/StoneDiffuseSpecular.dds"));
		resourceManager->loadTexture(L"WallsTexture", std::string("Data/CargoA.dds"));

		resourceManager->loadTexture(L"LightsTexture", std::string("Media/Flare.jpg"));
		resourceManager->loadTexture(L"NoiseMap", std::string("Media/Noise.png"));
		resourceManager->loadTexture(L"BurnMap", std::string("Media/Burn.png"));
		resourceManager->loadTexture(L"DistortMap", std::string("Media/Distort.png"));
	}
	catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
	{
		LastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
		return false;
	}


	////--------------- Prepare shaders and constant buffers to communicate with them ---------------////

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



	//********************************************
	//**** Create Scene Texture

	// We will render the scene to this texture instead of the back-buffer (screen), then we post-process the texture onto the screen
	// This is exactly the same code we used in the graphics module when we were rendering the scene onto a cube using a texture

	m_DownSampledtexture = new CRenderTexture;
	if (!m_DownSampledtexture) return false;
	result = m_DownSampledtexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating DownSampled Texture";
		return false;
	}

	m_Scenetexture = new CRenderTexture;
	if (!m_Scenetexture) return false;
	result = m_Scenetexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
	if (!result)
	{
		LastError = "Error creating Scene Texture";
		return false;
	}

	m_SecondPasstexture = new CRenderTexture;
	if (!m_SecondPasstexture) return false;
	result = m_SecondPasstexture->Initialize(gD3DDevice, m_ViewportWidth, m_ViewportHeight);
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

	gStars  = new Model(resourceManager->getMesh(L"StarsMesh"));
	gGround = new Model(resourceManager->getMesh(L"GroundMesh"));
	gCube   = new Model(resourceManager->getMesh(L"CubeMesh"));
	gWall1  = new Model(resourceManager->getMesh(L"Wall1Mesh"));
	gWall2  = new Model(resourceManager->getMesh(L"Wall2Mesh"));

	// Initial positions
	gCube->SetPosition({ 50, 5, -10 });
	gCube->SetRotation({ 0.0f, ToRadians(-110.0f), 0.0f });
	gCube->SetScale(1.5f);
	gWall1->SetPosition({ 41.85, 0, 36.9 });
	gWall1->SetRotation({ 0.0f, ToRadians(90.0f), 0.0f });
	gWall1->SetScale(40.0f);


	gWall2->SetPosition({ 10, 0, 68 });
	
	gWall2->SetScale(40.0f);


	gStars->SetScale(8000.0f);

	polyMatrix = MatrixTranslation({ gWall1->Position().x, 10, gWall1->Position().z });
	polyMatrix = MatrixRotationY(ToRadians(90)) * polyMatrix;

	HeartMatrix = MatrixTranslation({ gWall2->Position().x + 18, 10, gWall2->Position().z});

	SpadeMatrix = MatrixTranslation({ gWall2->Position().x - 18, 10, gWall2->Position().z});

	DiamondMatrix = MatrixTranslation({ gWall2->Position().x - 5.5f, 10, gWall2->Position().z});
	DiamondMatrix = MatrixRotationZ(ToRadians(45)) * DiamondMatrix;

	CloverMatrix = MatrixTranslation({ gWall2->Position().x + 6, 10, gWall2->Position().z});

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
	Lights[1].model->SetPosition({ -70, 30, 100 });
	Lights[1].model->SetScale(pow(Lights[1].strength, 1.0f));


	////--------------- Set up camera ---------------////

	MainCamera->SetPosition({ 25, 18, -45 });
	MainCamera->SetRotation({ ToRadians(10.0f), ToRadians(7.0f), 0.0f });
	
	
	//polyCamera->SetPosition(PolyCameraPosition);
	//polyCamera->SetRotation({ToRadians(90), 0.0f, 0.0f});
	//HeartCamera->Position() = (HeartWindowPoints[0] + CVector3(4.5f, -5, 0));
	//HeartCamera->SetPosition(HeartWindowPoints[0] + CVector3(4.5f, -5, 0));
	SpadeCamera->SetPosition({ gWall2->Position().x - 18, 10, gWall2->Position().z - 15 });
	//DiamondCamera->SetPosition({});
	//CloverCamera->SetPosition({});
	

	gPostProcessingConstants.tintColour1 = Colour1;
	gPostProcessingConstants.tintColour2 = Colour2;

	return true;
}


// Release the geometry and scene resources created above
void PostProcessingScene::ReleaseResources()
{
	ReleaseStates();

	//if (m_SceneTextureSRV)              m_SceneTextureSRV->Release();
	//if (m_SceneRenderTarget)            m_SceneRenderTarget->Release();
	//if (m_SceneTexture)                 m_SceneTexture->Release();

	if (PostProcessingConstantBuffer)  PostProcessingConstantBuffer->Release();
	if (PerModelConstantBuffer)        PerModelConstantBuffer->Release();
	if (PerFrameConstantBuffer)        PerFrameConstantBuffer->Release();

	ReleaseShaders();

	// See note in InitGeometry about why we're not using unique_ptr and having to manually delete
	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		delete Lights[i].model;  Lights[i].model = nullptr;
	}
	delete MainCamera;  MainCamera = nullptr;
	delete gWall1;   gWall1 = nullptr;
	delete gWall2;   gWall2 = nullptr;
	delete gCube;    gCube = nullptr;
	delete gGround;  gGround = nullptr;
	delete gStars;   gStars = nullptr;
}



//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------

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
	gGround->Setup(gPixelLightingVertexShader, gPixelLightingPixelShader);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)
	gGround->SetStates(gNoBlendingState, gUseDepthBufferState, gCullBackState);
	gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);
	gGround->SetShaderResources(0, resourceManager->getTexture(L"GroundTexture"));
	gGround->Render(PerModelConstantBuffer, gPerModelConstants);

	//gD3DContext->PSSetShaderResources(0, 1, &gBrickDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gWall1->SetShaderResources(0, resourceManager->getTexture(L"BricksTexture"));
	gWall1->Render(PerModelConstantBuffer, gPerModelConstants);
	
	//gD3DContext->PSSetShaderResources(0, 1, &gBrickDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gWall2->SetShaderResources(0, resourceManager->getTexture(L"BricksTexture"));
	gWall2->Render(PerModelConstantBuffer, gPerModelConstants);

	//gD3DContext->PSSetShaderResources(0, 1, &gCubeDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
	gCube->SetShaderResources(0, resourceManager->getTexture(L"CubeTexture"));
	gCube->Render(PerModelConstantBuffer, gPerModelConstants);


	////--------------- Render sky ---------------////

	// Select which shaders to use next

	// Using a pixel shader that tints the texture - don't need a tint on the sky so set it to white
	gPerModelConstants.objectColour = { 1, 1, 1 };

	// Render sky
	gStars->Setup(gBasicTransformVertexShader, gTintedTexturePixelShader);
	gStars->SetStates(gNoBlendingState, gUseDepthBufferState, gCullNoneState);
	gStars->SetShaderResources(0, resourceManager->getTexture(L"StarsTexture"));
	gStars->Render(PerModelConstantBuffer, gPerModelConstants);



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



//**************************

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
		gD3DContext->PSSetShader(gTintPostProcess, nullptr, 0);
	}
	else if (postProcess == PostProcess::Fog)
	{
		
		gD3DContext->VSSetShader(gFogVertexShader, nullptr, 0);
		gD3DContext->PSSetShader(gFogPixelShader, nullptr, 0);
		
	}

	else if (postProcess == PostProcess::Distort)
	{
		gD3DContext->PSSetShader(gDistortPostProcess, nullptr, 0);

		// Give pixel shader access to the distortion texture (containts 2D vectors (in R & G) to shift the texture UVs to give a cut-glass impression)
		ID3D11ShaderResourceView* temp = resourceManager->getTexture(L"DistortMap");
		gD3DContext->PSSetShaderResources(1, 1, &temp);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}
	else if (postProcess == PostProcess::Fisheye)
	{
		//gD3DContext->VSSetShader(gFogVertexShader, nullptr, 0);
		gD3DContext->PSSetShader(gFishEyeShader, nullptr, 0);
	}

	else if (postProcess == PostProcess::Saturation)
	{
		gD3DContext->PSSetShader(gSaturationPostProcess, nullptr, 0);
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

void PostProcessingScene::FirstRender()
{
	//}
	/*else
	{
		currentShaderTexture = m_SecondPasstexture->GetShaderResourceView();
		gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);
	}*/
	gD3DContext->PSSetSamplers(0, 1, &gPointSampler); // Use point sampling (no bilinear, trilinear, mip-mapping etc. for most post-processes)


	// Using special vertex shader that creates its own data for a 2D screen quad
	gD3DContext->VSSetShader(gFogVertexShader, nullptr, 0);
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

// Perform a full-screen post process from "scene texture" to back buffer
void PostProcessingScene::FullScreenPostProcess(PostProcess postProcess, ID3D11ShaderResourceView* renderResource)
{
	ID3D11ShaderResourceView* currentShaderTexture;

	for (int i = 0; i < m_ListOfEffects.size(); ++i)
	{
		/*if (m_ListOfEffects[i] == PostProcess::Copy)
		{
			m_SecondPasstexture->SetRenderTarget(gD3DContext);
			currentShaderTexture = m_Scenetexture->GetShaderResourceView();
			gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);
			SelectPostProcessShaderAndTextures(PostProcess::Copy);
				
			FirstRender();
			gD3DContext->Draw(4, 0);
		}*/
		if (m_ListOfEffects[i] == PostProcess::HorizontalBlur)
		{
			//Horizontal Blur
			m_HorizontalBlurTexture->SetRenderTarget(gD3DContext);
			m_HorizontalBlurTexture->ClearRenderTarget(gD3DContext, 0.0f, 1.0f, 0.0f, 1.0f);

			currentShaderTexture = m_Scenetexture->GetShaderResourceView();
			gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

			

			FirstRender();

			SelectPostProcessShaderAndTextures(PostProcess::HorizontalBlur);

			gD3DContext->Draw(4, 0);

			//Vertical Blur
			m_VerticalBlurTexture->SetRenderTarget(gD3DContext);
			SelectPostProcessShaderAndTextures(PostProcess::VerticalBlur);

			currentShaderTexture = m_HorizontalBlurTexture->GetShaderResourceView();
			gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

			gD3DContext->Draw(4, 0);


			//Second Pass
			m_SecondPasstexture->SetRenderTarget(gD3DContext);

			SelectPostProcessShaderAndTextures(PostProcess::Copy);

			currentShaderTexture = m_VerticalBlurTexture->GetShaderResourceView();
			gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

			gD3DContext->Draw(4, 0);
		}
		else
		{
			m_SecondPasstexture->SetRenderTarget(gD3DContext);
			m_SecondPasstexture->ClearRenderTarget(gD3DContext, 0.0f, 1.0f, 0.0f, 1.0f);

			/*if (m_NumberOfFullscreenEffects > 1)
			{
				currentShaderTexture = m_DownSampledtexture->GetShaderResourceView();
				gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);
			}
			else
			{*/
				currentShaderTexture = m_Scenetexture->GetShaderResourceView();
				gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);
			//}

			FirstRender();

			// Select shader and textures needed for the required post-processes (helper function above)
			SelectPostProcessShaderAndTextures(postProcess);

			//// Draw a quad
			gD3DContext->Draw(4, 0);
			//
			/*m_SecondPasstexture->SetRenderTarget(gD3DContext);
			SelectPostProcessShaderAndTextures(PostProcess::Copy);
			currentShaderTexture = m_DownSampledtexture->GetShaderResourceView();
			gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

			gD3DContext->Draw(4, 0);*/
		}
		
		SelectPostProcessShaderAndTextures(PostProcess::Copy);
		m_DownSampledtexture->SetRenderTarget(gD3DContext);
		currentShaderTexture = m_SecondPasstexture->GetShaderResourceView();
		gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);
		gD3DContext->Draw(4, 0);	
	}

	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	currentShaderTexture = m_DownSampledtexture->GetShaderResourceView();
	gD3DContext->PSSetShaderResources(0, 1, &currentShaderTexture);

	gD3DContext->Draw(4, 0);
}


// Perform an post process from "scene texture" to back buffer within the given four-point polygon and a world matrix to position/rotate/scale the polygon
void PostProcessingScene::PolygonPostProcess(PostProcess postProcess, std::vector<CVector3>& points, const CMatrix4x4& worldMatrix, Camera* camera, ID3D11ShaderResourceView* renderResource)
{
	// First perform a full-screen copy of the scene to back-buffer
	FullScreenPostProcess(PostProcess::Copy, renderResource);

	gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	// Select the special 2D polygon post-processing vertex shader and draw the polygon
	gD3DContext->VSSetShader(g2DPolygonVertexShader, nullptr, 0);

	// Select shader/textures needed for required post-process
	SelectPostProcessShaderAndTextures(postProcess);

	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < SpadeWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(SpadeWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * SpadeMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}

	// Pass over the polygon points to the shaders (also sends the per-process settings prepared in UpdateScene function below)
	//PerFrameConstants.cameraPosition = SpadeCamera->Position();
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);


	SelectPostProcessShaderAndTextures(PostProcess::Pixelation);
	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < HeartWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(HeartWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * HeartMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);
	//
	SelectPostProcessShaderAndTextures(PostProcess::Vignette);
	for (unsigned int i = 0; i < DiamondWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(DiamondWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * DiamondMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);


	SelectPostProcessShaderAndTextures(PostProcess::Distort);
	for (unsigned int i = 0; i < CloverWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(CloverWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * CloverMatrix;
		/*SpadeCamera->SetPosition({ worldPosition.x + 4.5f, worldPosition.y - 5, worldPosition.z });*/
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);

	
	SelectPostProcessShaderAndTextures(PostProcess::Fisheye);
	for (unsigned int i = 0; i < SquarePoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(SquarePoints[i], 1);
		CVector4 worldPosition = modelPosition * polyMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);
	// These lines unbind the scene texture from the pixel shader to stop DirectX issuing a warning when we try to render to it again next frame
	/*ID3D11ShaderResourceView* nullSRV = nullptr;
	gD3DContext->PSSetShaderResources(0, 1, &nullSRV);*/


}


//**************************


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

	PerFrameConstants.ambientColour  = gAmbientColour;
	PerFrameConstants.specularPower  = gSpecularPower;
	PerFrameConstants.cameraPosition = MainCamera->Position();

	PerFrameConstants.viewportWidth  = static_cast<float>(m_ViewportWidth);
	PerFrameConstants.viewportHeight = static_cast<float>(m_ViewportHeight);


	// Set the target for rendering and select the main depth buffer.
	// If using post-processing then render to the scene texture, otherwise to the usual back buffer
	// Also clear the render target to a fixed colour and the depth buffer to the far distance
	/*if (CurrentPostProcess == PostProcess::GreyNoise)
	{
		gD3DContext->OMSetRenderTargets(1, &m_SecondPassRenderTarget, gDepthStencil);
		gD3DContext->ClearRenderTargetView(m_SecondPassRenderTarget, &gBackgroundColor.r);
	}*/


	//// Portal scene rendering ////
	D3D11_VIEWPORT vp;
	// Set the portal texture and portal depth buffer as the targets for rendering
	// The portal texture will later be used on models in the main scene
	
	if (CurrentPostProcess != PostProcess::None)
	{
		m_Scenetexture->SetRenderTarget(gD3DContext);
		m_Scenetexture->ClearRenderTarget(gD3DContext, gBackgroundColor.r, gBackgroundColor.g, gBackgroundColor.b, gBackgroundColor.a);	
	}
	else
	{
		gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
		gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
	}
	gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Setup the viewport to the size of the main window
	vp.Width = static_cast<FLOAT>(m_ViewportWidth);
	vp.Height = static_cast<FLOAT>(m_ViewportHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	gD3DContext->RSSetViewports(1, &vp);

	// Render the scene from the main camera
	RenderSceneFromCamera(MainCamera);


	////--------------- Scene completion ---------------////

	// Run any post-processing steps

	gPostProcessingConstants.Epsilon = 1e-10;

	if (CurrentPostProcess != PostProcess::None)
	{
		if (CurrentPostProcessMode == PostProcessMode::Fullscreen)
		{
			//if (m_ListOfEffects.size() == 0)
			//{
				FullScreenPostProcess(CurrentPostProcess, m_Scenetexture->GetShaderResourceView());
			//}
			/*else
			{
				for (int i = 0; i < m_NumberOfFullscreenEffects; ++i)
				{
					FullScreenPostProcess(m_ListOfEffects[i], m_Scenetexture->GetShaderResourceView());
				}
			}*/
		}

		else if (CurrentPostProcessMode == PostProcessMode::Polygon)
		{		
			FullScreenPostProcess(CurrentPostProcess, m_Scenetexture->GetShaderResourceView());
			// Pass an array of 4 points and a matrix. Only supports 4 points.
			PolygonPostProcess(PostProcess::Saturation, SpadeWindowPoints, SpadeMatrix, SpadeCamera, m_Scenetexture->GetShaderResourceView());
		}
		else
		{
			
//			PolygonPostProcess(PostProcess::Gradient, CloverWindowPoints, CloverMatrix, MainCamera, m_SceneTextureSRV);
		}

		
		// These lines unbind the scene texture from the pixel shader to stop DirectX issuing a warning when we try to render to it again next frame
		ID3D11ShaderResourceView* nullSRV = nullptr;
		gD3DContext->PSSetShaderResources(0, 1, &nullSRV);

	}
	//FullScreenPostProcess(PostProcess::Copy, m_SecondPassTextureSRV, gBackBufferRenderTarget);
	/*gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);*/
	
	IMGUI();
	ImGui::Render();
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
	// Set first parameter to 1 to lock to vsync
	gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------


// Update models and camera. frameTime is the time passed since the last frame
void PostProcessingScene::UpdateScene(float frameTime, HWND HWnd)
{
	//***********

	// Select post process on keys
	if (KeyHit(Key_F1))  CurrentPostProcessMode = PostProcessMode::Fullscreen;
	if (KeyHit(Key_F2))  CurrentPostProcessMode = PostProcessMode::Polygon;

	if (KeyHit(Key_1))   CurrentPostProcess = PostProcess::Gradient;
	if (KeyHit(Key_2))   CurrentPostProcess = PostProcess::HorizontalBlur;
	if (KeyHit(Key_6))   CurrentPostProcess = PostProcess::Pixelation;
	//if (KeyHit(Key_4))   CurrentPostProcess = PostProcess::Fog;
	if (KeyHit(Key_5))   CurrentPostProcess = PostProcess::Saturation;
	if (KeyHit(Key_3))   CurrentPostProcess = PostProcess::Underwater;
	if (KeyHit(Key_9))   CurrentPostProcess = PostProcess::Copy;
	if (KeyHit(Key_M))   CurrentPostProcess = PostProcess::Vignette;
	if (KeyHit(Key_0))   CurrentPostProcess = PostProcess::None;

	// Post processing settings - all data for post-processes is updated every frame whether in use or not (minimal cost)
		
	
	float a = (1.0 - saturationLevel) * rwgt + saturationLevel;
	float b = (1.0 - saturationLevel) * rwgt;
	float c = (1.0 - saturationLevel) * rwgt;
	float d = (1.0 - saturationLevel) * gwgt;
	float e = (1.0 - saturationLevel) * gwgt + saturationLevel;
	float f = (1.0 - saturationLevel) * gwgt;
	float g = (1.0 - saturationLevel) * bwgt;
	float h = (1.0 - saturationLevel) * bwgt;
	float i = (1.0 - saturationLevel) * bwgt + saturationLevel;

	// Noise scaling adjusts how fine the grey noise is.
	const float grainSize = 50; // Fineness of the noise grain
	gPostProcessingConstants.noiseScale  = { m_ViewportWidth / grainSize, m_ViewportHeight / grainSize };

	// The noise offset is randomised to give a constantly changing noise effect (like tv static)
	gPostProcessingConstants.noiseOffset = { Random(0.0f, 1.0f), Random(0.0f, 1.0f) };

	// Set the level of distortion
	gPostProcessingConstants.distortLevel = 0.01f;
	gPostProcessingConstants.PixelWidth = m_PixelWidth;
	gPostProcessingConstants.PixelHeight = m_PixelHeight;

	gPostProcessingConstants.BlurOffset = m_BlurOffset;
	

	gPostProcessingConstants.vignetteStrength = m_vignetteStrength;
	gPostProcessingConstants.vignetteSize = m_vignetteSize;
	gPostProcessingConstants.vignetteFalloff = m_vignetteFalloff;

	// Set and increase the amount of spiral - use a tweaked cos wave to animate
	static float wiggle = 0.0f;
	const float wiggleSpeed = 1.0f;



	gPostProcessingConstants.SaturationLevel = saturationLevel;

	gPostProcessingConstants.BlurHeight = m_BlurOffset;
	gPostProcessingConstants.BlurWidth = m_BlurOffset * 1.3;

	wiggle += wiggleSpeed * frameTime;

	// Update heat haze timer
	wi = wi + frameTime;
	gPostProcessingConstants.heatHazeTimer = wi;

	//***********


	// Orbit one light - a bit of a cheat with the static variable [ask the tutor if you want to know what this is]
	static float lightRotate = 0.0f;
	static bool go = true;
	Lights[0].model->SetPosition({ 20 + cos(lightRotate) * LightOrbitRadius, 10, 20 + sin(lightRotate) * LightOrbitRadius });
	if (go)  lightRotate -= LightOrbitSpeed * frameTime;
	if (KeyHit(Key_L))  go = !go;

	// Control of camera
	MainCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D);

	// Toggle FPS limiting
	if (KeyHit(Key_P))  lockFPS = !lockFPS;

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
	if (postProcessingTimer <= 0.0f && !postProcessing)
	{
		CurrentPostProcessMode = PostProcessMode::Polygon;
		postProcessing = true;
	}
	else postProcessingTimer -= frameTime;
}

void PostProcessingScene::IMGUI()
{
	{
		ImGui::Begin("Information", 0, windowFlags);
		ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", MainCamera->Position().x, MainCamera->Position().y, MainCamera->Position().z);
		ImGui::Text("Camera Rotation: (%.2f, %.2f, %.2f)", MainCamera->Rotation().x, MainCamera->Rotation().y, MainCamera->Rotation().z);
		ImGui::Text("");
		ImGui::ColorEdit3("Ambient Colour",&gAmbientColour.x);
		ImGui::Separator();
		if (ImGui::Button("Fullscreen", ButtonSize))
		{
			CurrentPostProcessMode = PostProcessMode::Fullscreen;
			CurrentPostProcess = PostProcess::Copy;
		}
		ImGui::SameLine();

		if (ImGui::Button("Polygon", ButtonSize))
		{
			CurrentPostProcessMode = PostProcessMode::Polygon;
			CurrentPostProcess = PostProcess::Copy;
		}
		
		if (ImGui::Button("None", ButtonSize))
		{
			CurrentPostProcessMode = PostProcessMode::None;
			CurrentPostProcess = PostProcess::None;
		}
		ImGui::Separator();
		if (ImGui::Button("Gradient Effect", ButtonSize))
		{
			CurrentPostProcess = PostProcess::Gradient;
			m_ListOfEffects.push_back(PostProcess::Gradient);
			++m_NumberOfFullscreenEffects;
		}
		ImGui::SameLine();
		if (ImGui::Button("Blur Effect", ButtonSize))
		{
			CurrentPostProcess = PostProcess::HorizontalBlur;
			m_ListOfEffects.push_back(PostProcess::HorizontalBlur);
			++m_NumberOfFullscreenEffects;
		}

		if (ImGui::Button("Underwater Effect", ButtonSize))
		{
			CurrentPostProcess = PostProcess::Underwater;
			m_ListOfEffects.push_back(PostProcess::Underwater);
			++m_NumberOfFullscreenEffects;
		}

		ImGui::SliderFloat("Saturation Level",&saturationLevel, -1.0f, 50.0f);

		ImGui::SliderFloat("Vignette Strength", &m_vignetteStrength, 0.0f, 2.5f);
		ImGui::SliderFloat("Vignette Size", &m_vignetteSize, 0.0f, 2.5f);
		ImGui::SliderFloat("Vignette Falloff", &m_vignetteFalloff, 0.0f, 2.5f);

		ImGui::SliderInt("Pixel Width", &m_PixelWidth, 4, 128);
		ImGui::SliderInt("Pixel Height", &m_PixelHeight, 4, 128);
		ImGui::SliderInt("Blur offset", &m_BlurOffset, 100, 960);

		if (ImGui::Button("Reset Effects", ButtonSize))
		{
			m_ListOfEffects.clear();
			m_ListOfEffects.push_back(PostProcess::Copy);
			m_NumberOfFullscreenEffects = 0;
			CurrentPostProcess = PostProcess::None;
		}
		std::string m = GetPostProcessModeString(CurrentPostProcessMode);
		const char* c = m.c_str();
		ImGui::Text("Amount of Effects active: %i", m_ListOfEffects.size() - 1);
		ImGui::Text("Amount of Effects active: %s", c);
		
		
		ImGui::End();


	}
}
