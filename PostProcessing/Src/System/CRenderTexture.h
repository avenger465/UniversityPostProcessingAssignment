#pragma once
#include "Utility\ColourRGBA.h"
#include <d3d11.h>

class CRenderTexture
{
//----------------------//
// Construction / Usage	//
//----------------------//
public:
	CRenderTexture();
	~CRenderTexture();

	//Initialise the texture and depth buffer with the required width and height
	bool Initialize(ID3D11Device*, int, int);

	//Release the resources from the class
	void Shutdown();
	
	//Set the texture that the system will render to
	void SetRenderTarget(ID3D11DeviceContext*, ID3D11DepthStencilView* depthStencil = nullptr);

	//Clear the render target
	void ClearRenderTarget(ID3D11DeviceContext*, ColourRGBA);

	//-------------------------------------
	// Data access
	//-------------------------------------

	//Get the shader resource view of the render target
	ID3D11ShaderResourceView* GetShaderResourceView();

	//Get the depth stencil view of the render target
	ID3D11DepthStencilView* GetDepthStencilView();

	//Get the Width of the texture
	int GetTextureWidth();

	//Get the Height of the texture
	int GetTextureHeight();
	
//-------------------------------------
// Private members
//-------------------------------------
private:
	int m_textureWidth, m_textureHeight;

	ID3D11Texture2D* m_renderTargetTexture;
	ID3D11RenderTargetView* m_renderTargetView;
	ID3D11ShaderResourceView* m_shaderResourceView;
	
	ID3D11Texture2D* m_depthStencilBuffer;
	ID3D11DepthStencilView* m_depthStencilView;
	
	D3D11_VIEWPORT m_viewport;
};

