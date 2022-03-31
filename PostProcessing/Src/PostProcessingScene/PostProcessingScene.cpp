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

	windowFlags |= ImGuiWindowFlags_NoScrollbar;
	windowFlags |= ImGuiWindowFlags_AlwaysAutoResize;
	windowFlags |= ImGuiWindowFlags_NoMove;
}

// Prepare the geometry required for the scene
// Returns true on success
bool PostProcessingScene::InitGeometry(std::string& LastError)
{
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

	// Using a helper function to load textures from files above. Here we create the scene texture manually
	// as we are creating a special kind of texture (one that we can render to). Many settings to prepare:
	D3D11_TEXTURE2D_DESC sceneTextureDesc = {};
	sceneTextureDesc.Width = m_ViewportWidth;  // Full-screen post-processing - use full screen size for texture
	sceneTextureDesc.Height = m_ViewportHeight;
	sceneTextureDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we would have to render every level)
	sceneTextureDesc.ArraySize = 1;
	sceneTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	sceneTextureDesc.SampleDesc.Count = 1;
	sceneTextureDesc.SampleDesc.Quality = 0;
	sceneTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // IMPORTANT: Indicate we will use texture as render target, and pass it to shaders
	sceneTextureDesc.CPUAccessFlags = 0;
	sceneTextureDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &m_SceneTexture)))
	{
		LastError = "Error creating scene texture";
		return false;
	}
	sceneTextureDesc.Width = 634;
	sceneTextureDesc.Height = 480;
	if (FAILED(gD3DDevice->CreateTexture2D(&sceneTextureDesc, NULL, &m_SecondPassTexture)))
	{
		LastError = "Error creating scene texture";
		return false;
	}

	// We created the scene texture above, now we get a "view" of it as a render target, i.e. get a special pointer to the texture that
	// we use when rendering to it (see RenderScene function below)
	if (FAILED(gD3DDevice->CreateRenderTargetView(m_SceneTexture, NULL, &m_SceneRenderTarget)))
	{
		LastError = "Error creating scene render target view";
		return false;
	}
	if (FAILED(gD3DDevice->CreateRenderTargetView(m_SceneTexture, NULL, &m_SecondPassRenderTarget)))
	{
		LastError = "Error creating scene render target view";
		return false;
	}

	// We also need to send this texture (resource) to the shaders. To do that we must create a shader-resource "view"
	D3D11_SHADER_RESOURCE_VIEW_DESC srDesc = {};
	srDesc.Format = sceneTextureDesc.Format;
	srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;
	if (FAILED(gD3DDevice->CreateShaderResourceView(m_SceneTexture, &srDesc, &m_SceneTextureSRV)))
	{
		LastError = "Error creating scene shader resource view";
		return false;
	}
	if (FAILED(gD3DDevice->CreateShaderResourceView(m_SceneTexture, &srDesc, &m_SecondPassTextureSRV)))
	{
		LastError = "Error creating scene shader resource view";
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

	return true;
}


// Release the geometry and scene resources created above
void PostProcessingScene::ReleaseResources()
{
	ReleaseStates();

	if (m_SceneTextureSRV)              m_SceneTextureSRV->Release();
	if (m_SceneRenderTarget)            m_SceneRenderTarget->Release();
	if (m_SceneTexture)                 m_SceneTexture->Release();

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

	else if (postProcess == PostProcess::GreyNoise)
	{
		gD3DContext->PSSetShader(gGreyNoisePostProcess, nullptr, 0);

		// Give pixel shader access to the noise texture
		ID3D11ShaderResourceView* temp = resourceManager->getTexture(L"NoiseMap");
		gD3DContext->PSSetShaderResources(1, 1, &temp);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcess::Burn)
	{
		gD3DContext->PSSetShader(gBurnPostProcess, nullptr, 0);

		// Give pixel shader access to the burn texture (basically a height map that the burn level ascends)
		ID3D11ShaderResourceView* temp = resourceManager->getTexture(L"BurnMap");
		gD3DContext->PSSetShaderResources(1, 1, &temp);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcess::Distort)
	{
		gD3DContext->PSSetShader(gDistortPostProcess, nullptr, 0);

		// Give pixel shader access to the distortion texture (containts 2D vectors (in R & G) to shift the texture UVs to give a cut-glass impression)
		ID3D11ShaderResourceView* temp = resourceManager->getTexture(L"DistortMap");
		gD3DContext->PSSetShaderResources(1, 1, &temp);
		gD3DContext->PSSetSamplers(1, 1, &gTrilinearSampler);
	}

	else if (postProcess == PostProcess::Spiral)
	{
		gD3DContext->PSSetShader(gSpiralPostProcess, nullptr, 0);
	}

	else if (postProcess == PostProcess::Underwater)
	{
		gD3DContext->PSSetShader(gHeatHazePostProcess, nullptr, 0);
	}
}

// Perform a full-screen post process from "scene texture" to back buffer
void PostProcessingScene::FullScreenPostProcess(PostProcess postProcess)
{
	//if (postProcess == PostProcess::GreyNoise)
	//{
	//	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
	//	//gD3DContext->ClearRenderTargetView(m_SecondPassRenderTarget, &gBackgroundColor.r);

	//	// Give the pixel shader (post-processing shader) access to the scene texture 
	//	gD3DContext->PSSetShaderResources(0, 1, &m_SecondPassTextureSRV);
	//	gD3DContext->PSSetSamplers(0, 1, &gPointSampler); // Use point sampling (no bilinear, trilinear, mip-mapping etc. for most post-processes)

	//	// Using special vertex shader that creates its own data for a 2D screen quad
	//	gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
	//	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)


	//	// States - no blending, don't write to depth buffer and ignore back-face culling
	//	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	//	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	//	gD3DContext->RSSetState(gCullNoneState);


	//	// No need to set vertex/index buffer (see 2D quad vertex shader), just indicate that the quad will be created as a triangle strip
	//	gD3DContext->IASetInputLayout(NULL); // No vertex data
	//	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	//	// Select shader and textures needed for the required post-processes (helper function above)
	//	SelectPostProcessShaderAndTextures(postProcess);


	//	// Set 2D area for full-screen post-processing (coordinates in 0->1 range)
	//	gPostProcessingConstants.area2DTopLeft = { 0, 0 }; // Top-left of entire screen
	//	gPostProcessingConstants.area2DSize = { 1, 1 }; // Full size of screen
	//	gPostProcessingConstants.area2DDepth = 0;        // Depth buffer value for full screen is as close as possible


	//	// Pass over the above post-processing settings (also the per-process settings prepared in UpdateScene function below)
	//	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	//	gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	//	gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);


	//	// Draw a quad
	//	gD3DContext->Draw(4, 0);


		//gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);
		//gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);

		//// Give the pixel shader (post-processing shader) access to the scene texture 
		//gD3DContext->PSSetShaderResources(0, 1, &m_SecondPassTextureSRV);
		//gD3DContext->PSSetSamplers(0, 1, &gPointSampler); // Use point sampling (no bilinear, trilinear, mip-mapping etc. for most post-processes)

		//// Using special vertex shader that creates its own data for a 2D screen quad
		//gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
		//gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)


		//// States - no blending, don't write to depth buffer and ignore back-face culling
		//gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
		//gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
		//gD3DContext->RSSetState(gCullNoneState);


		//// No need to set vertex/index buffer (see 2D quad vertex shader), just indicate that the quad will be created as a triangle strip
		//gD3DContext->IASetInputLayout(NULL); // No vertex data
		//gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


		//// Select shader and textures needed for the required post-processes (helper function above)
		//SelectPostProcessShaderAndTextures(postProcess);
		//// Set 2D area for full-screen post-processing (coordinates in 0->1 range)
		//gPostProcessingConstants.area2DTopLeft = { 0, 0 }; // Top-left of entire screen
		//gPostProcessingConstants.area2DSize = { 1, 1 }; // Full size of screen
		//gPostProcessingConstants.area2DDepth = 0;        // Depth buffer value for full screen is as close as possible


		//// Pass over the above post-processing settings (also the per-process settings prepared in UpdateScene function below)
		//UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
		//gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
		//gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);


		//// Draw a quad
		//gD3DContext->Draw(4, 0);


//	}
	// Select the back buffer to use for rendering. Not going to clear the back-buffer because we're going to overwrite it all
	gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

	
	// Give the pixel shader (post-processing shader) access to the scene texture 
	gD3DContext->PSSetShaderResources(0, 1, &m_SceneTextureSRV);
	gD3DContext->PSSetSamplers(0, 1, &gPointSampler); // Use point sampling (no bilinear, trilinear, mip-mapping etc. for most post-processes)


	// Using special vertex shader that creates its own data for a 2D screen quad
	gD3DContext->VSSetShader(g2DQuadVertexShader, nullptr, 0);
	gD3DContext->GSSetShader(nullptr, nullptr, 0);  // Switch off geometry shader when not using it (pass nullptr for first parameter)


	// States - no blending, don't write to depth buffer and ignore back-face culling
	gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
	gD3DContext->RSSetState(gCullNoneState);


	// No need to set vertex/index buffer (see 2D quad vertex shader), just indicate that the quad will be created as a triangle strip
	gD3DContext->IASetInputLayout(NULL); // No vertex data
	gD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	// Select shader and textures needed for the required post-processes (helper function above)
	SelectPostProcessShaderAndTextures(postProcess);


	// Set 2D area for full-screen post-processing (coordinates in 0->1 range)
	gPostProcessingConstants.area2DTopLeft = { 0, 0 }; // Top-left of entire screen
	gPostProcessingConstants.area2DSize    = { 1, 1 }; // Full size of screen
	gPostProcessingConstants.area2DDepth   = 0;        // Depth buffer value for full screen is as close as possible


	// Pass over the above post-processing settings (also the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);


	// Draw a quad
	gD3DContext->Draw(4, 0);
}


// Perform an area post process from "scene texture" to back buffer at a given point in the world, with a given size (world units)
void PostProcessingScene::AreaPostProcess(PostProcess postProcess, CVector3 worldPoint, CVector2 areaSize)
{
	// First perform a full-screen copy of the scene to back-buffer
	FullScreenPostProcess(PostProcess::Copy);
	

	// Now perform a post-process of a portion of the scene to the back-buffer (overwriting some of the copy above)
	// Note: The following code relies on many of the settings that were prepared in the FullScreenPostProcess call above, it only
	//       updates a few things that need to be changed for an area process. If you tinker with the code structure you need to be
	//       aware of all the work that the above function did that was also preparation for this post-process area step

	// Select shader/textures needed for required post-process
	SelectPostProcessShaderAndTextures(postProcess);

	// Enable alpha blending - area effects need to fade out at the edges or the hard edge of the area is visible
	// A couple of the shaders have been updated to put the effect into a soft circle
	// Alpha blending isn't enabled for fullscreen and polygon effects so it doesn't affect those (except heat-haze, which works a bit differently)
	gD3DContext->OMSetBlendState(gAlphaBlendingState, nullptr, 0xffffff);


	// Use picking methods to find the 2D position of the 3D point at the centre of the area effect
	auto worldPointTo2D = MainCamera->PixelFromWorldPt(worldPoint, m_ViewportWidth, m_ViewportHeight);
	CVector2 area2DCentre = { worldPointTo2D.x, worldPointTo2D.y };
	float areaDistance = worldPointTo2D.z;
	
	// Nothing to do if given 3D point is behind the camera
	if (areaDistance < MainCamera->NearClip())  return;
	
	// Convert pixel coordinates to 0->1 coordinates as used by the shader
	area2DCentre.x /= m_ViewportWidth;
	area2DCentre.y /= m_ViewportHeight;



	// Using new helper function here - it calculates the world space units covered by a pixel at a certain distance from the camera.
	// Use this to find the size of the 2D area we need to cover the world space size requested
	CVector2 pixelSizeAtPoint = MainCamera->PixelSizeInWorldSpace(areaDistance, m_ViewportWidth, m_ViewportHeight);
	CVector2 area2DSize = { areaSize.x / pixelSizeAtPoint.x, areaSize.y / pixelSizeAtPoint.y };

	// Again convert the result in pixels to a result to 0->1 coordinates
	area2DSize.x /= m_ViewportWidth;
	area2DSize.y /= m_ViewportHeight;



	// Send the area top-left and size into the constant buffer - the 2DQuad vertex shader will use this to create a quad in the right place
	gPostProcessingConstants.area2DTopLeft = area2DCentre - 0.5f * area2DSize; // Top-left of area is centre - half the size
	gPostProcessingConstants.area2DSize = area2DSize;

	// Manually calculate depth buffer value from Z distance to the 3D point and camera near/far clip values. Result is 0->1 depth value
	// We've never seen this full calculation before, it's occasionally useful. It is derived from the material in the Picking lecture
	// Having the depth allows us to have area effects behind normal objects
	gPostProcessingConstants.area2DDepth = MainCamera->FarClip() * (areaDistance - MainCamera->NearClip()) / (MainCamera->FarClip() - MainCamera->NearClip());
	gPostProcessingConstants.area2DDepth /= areaDistance;

	// Pass over this post-processing area to shaders (also sends the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);


	// Draw a quad
	gD3DContext->Draw(4, 0);

}


// Perform an post process from "scene texture" to back buffer within the given four-point polygon and a world matrix to position/rotate/scale the polygon
void PostProcessingScene::PolygonPostProcess(PostProcess postProcess, std::vector<CVector3>& points, const CMatrix4x4& worldMatrix)
{
	// First perform a full-screen copy of the scene to back-buffer
	FullScreenPostProcess(PostProcess::Copy);


	// Now perform a post-process of a portion of the scene to the back-buffer (overwriting some of the copy above)
	// Note: The following code relies on many of the settings that were prepared in the FullScreenPostProcess call above, it only
	//       updates a few things that need to be changed for an area process. If you tinker with the code structure you need to be
	//       aware of all the work that the above function did that was also preparation for this post-process area step

	// Select shader/textures needed for required post-process
	SelectPostProcessShaderAndTextures(postProcess);

	// Loop through the given points, transform each to 2D (this is what the vertex shader normally does in most labs)
	for (unsigned int i = 0; i < points.size(); ++i)
	{
		CVector4 modelPosition = CVector4(points[i], 1);
		CVector4 worldPosition = modelPosition * worldMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}

	// Pass over the polygon points to the shaders (also sends the per-process settings prepared in UpdateScene function below)
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->VSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);
	gD3DContext->PSSetConstantBuffers(1, 1, &PostProcessingConstantBuffer);

	//gD3DContext->RSSetState(wireframe ? gWireframeState : gCullBackState);

	// Select the special 2D polygon post-processing vertex shader and draw the polygon
	gD3DContext->VSSetShader(g2DPolygonVertexShader, nullptr, 0);
	gD3DContext->Draw(4, 0);



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


	for (unsigned int i = 0; i < SpadeWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(SpadeWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * SpadeMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);

	for (unsigned int i = 0; i < DiamondWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(DiamondWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * DiamondMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);

	for (unsigned int i = 0; i < CloverWindowPoints.size(); ++i)
	{
		CVector4 modelPosition = CVector4(CloverWindowPoints[i], 1);
		CVector4 worldPosition = modelPosition * CloverMatrix;
		CVector4 viewportPosition = worldPosition * MainCamera->ViewProjectionMatrix();

		gPostProcessingConstants.polygon2DPoints[i] = viewportPosition;
	}
	UpdateConstantBuffer(PostProcessingConstantBuffer, gPostProcessingConstants);
	gD3DContext->Draw(4, 0);

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
	if (CurrentPostProcess == PostProcess::GreyNoise)
	{
		gD3DContext->OMSetRenderTargets(1, &m_SecondPassRenderTarget, gDepthStencil);
		gD3DContext->ClearRenderTargetView(m_SecondPassRenderTarget, &gBackgroundColor.r);
	}
	else if (CurrentPostProcess != PostProcess::None)
	{
		gD3DContext->OMSetRenderTargets(1, &m_SceneRenderTarget, gDepthStencil);
		gD3DContext->ClearRenderTargetView(m_SceneRenderTarget, &gBackgroundColor.r);
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

	////--------------- Scene completion ---------------////

	// Run any post-processing steps

	gPostProcessingConstants.Epsilon = 1e-10;
	if (CurrentPostProcess != PostProcess::None)
	{

		if (CurrentPostProcessMode == PostProcessMode::Fullscreen)
		{
			FullScreenPostProcess(CurrentPostProcess);
		}

		else if (CurrentPostProcessMode == PostProcessMode::Area)
		{
			// Pass a 3D point for the centre of the affected area and the size of the (rectangular) area in world units
			AreaPostProcess(PostProcess::Gradient, CVector3{gWall1->Position().x, 10,gWall1->Position().z }, { 5, 5 });
		}

		else if (CurrentPostProcessMode == PostProcessMode::Polygon)
		{
			// Pass an array of 4 points and a matrix. Only supports 4 points.
			PolygonPostProcess(CurrentPostProcess, SquarePoints, polyMatrix);
		}


		// These lines unbind the scene texture from the pixel shader to stop DirectX issuing a warning when we try to render to it again next frame
		ID3D11ShaderResourceView* nullSRV = nullptr;
		gD3DContext->PSSetShaderResources(0, 1, &nullSRV);

	}
	IMGUI();
	ImGui::Render();
	//gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, nullptr);
	//if (CurrentPostProcess != PostProcess::None) gD3DContext->OMSetRenderTargets(1, &m_SceneRenderTarget, gDepthStencil);
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
	if (KeyHit(Key_F2))  CurrentPostProcessMode = PostProcessMode::Area;
	if (KeyHit(Key_F3))  CurrentPostProcessMode = PostProcessMode::Polygon;

	if (KeyHit(Key_1))   CurrentPostProcess = PostProcess::Gradient;
	if (KeyHit(Key_2))   CurrentPostProcess = PostProcess::GreyNoise;
	//if (KeyHit(Key_6))   CurrentPostProcess = PostProcess::Burn;
	//if (KeyHit(Key_4))   CurrentPostProcess = PostProcess::Distort;
	//if (KeyHit(Key_5))   CurrentPostProcess = PostProcess::Spiral;
	if (KeyHit(Key_3))   CurrentPostProcess = PostProcess::Underwater;
	if (KeyHit(Key_9))   CurrentPostProcess = PostProcess::Copy;
	if (KeyHit(Key_0))   CurrentPostProcess = PostProcess::None;
	// Post processing settings - all data for post-processes is updated every frame whether in use or not (minimal cost)
	
	// Colour for tint shader
	//gPostProcessingConstants.tintColour1 = { 0.39, 0.33, 0.24 };
	//gPostProcessingConstants.tintColour2 = { 0.04, 0.13, 0.19 };	
	

	gPostProcessingConstants.tintColour1 = { 0, 0, 1 };
	gPostProcessingConstants.tintColour2 = { 1, 1, 0 };

	// Noise scaling adjusts how fine the grey noise is.
	const float grainSize = 50; // Fineness of the noise grain
	gPostProcessingConstants.noiseScale  = { m_ViewportWidth / grainSize, m_ViewportHeight / grainSize };

	// The noise offset is randomised to give a constantly changing noise effect (like tv static)
	gPostProcessingConstants.noiseOffset = { Random(0.0f, 1.0f), Random(0.0f, 1.0f) };

	// Set and increase the burn level (cycling back to 0 when it reaches 1.0f)
	const float burnSpeed = 0.2f;
	gPostProcessingConstants.burnHeight = fmod(gPostProcessingConstants.burnHeight + burnSpeed * frameTime, 1.0f);

	// Set the level of distortion
	gPostProcessingConstants.distortLevel = 0.01f;

	// Set and increase the amount of spiral - use a tweaked cos wave to animate
	static float wiggle = 0.0f;
	const float wiggleSpeed = 1.0f;
	gPostProcessingConstants.spiralLevel = ((1.0f - cos(wiggle)) * 4.0f );
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
		}
		ImGui::SameLine();
		if (ImGui::Button("Area", ButtonSize))
		{
			CurrentPostProcessMode = PostProcessMode::Area;
		}

		if (ImGui::Button("Polygon", ButtonSize))
		{
			CurrentPostProcessMode = PostProcessMode::Polygon;
		}
		ImGui::Separator();
		if (ImGui::Button("Gradient Effect", ButtonSize))
		{
			CurrentPostProcess = PostProcess::Gradient;
		}
		ImGui::SameLine();
		if (ImGui::Button("Blur Effect", ButtonSize))
		{
			CurrentPostProcess = PostProcess::GreyNoise;
		}

		if (ImGui::Button("Underwater Effect", ButtonSize))
		{
			CurrentPostProcess = PostProcess::Underwater;
		}

		ImGui::End();


	}
}
