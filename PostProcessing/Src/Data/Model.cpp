//--------------------------------------------------------------------------------------
// Class encapsulating a model
//--------------------------------------------------------------------------------------
// Holds a pointer to a mesh as well as position, rotation and scaling, which are converted to a world matrix when required
// This is more of a convenience class, the Mesh class does most of the difficult work.

#include "Model.h"
#include "Mesh.h"
#include "Utility/GraphicsHelpers.h"
#include "project/Common.h"


Model::Model(Mesh* mesh, CVector3 position /*= { 0,0,0 }*/, CVector3 rotation /*= { 0,0,0 }*/, float scale /*= 1*/)
    : mMesh(mesh)
{
    // Set default matrices from mesh
    mWorldMatrices.resize(mesh->NumberNodes());
    for (int i = 0; i < mWorldMatrices.size(); ++i)
        mWorldMatrices[i] = mesh->GetNodeDefaultMatrix(i);
}



// The render function simply passes this model's matrices over to Mesh:Render.
// All other per-frame constants must have been set already along with shaders, textures, samplers, states etc.
void Model::Render(ID3D11Buffer* buffer, PerModelConstants& ModelConstants)
{
    mMesh->Render(mWorldMatrices, buffer, ModelConstants);
}


// Control a given node in the model using keys provided. Amount of motion performed depends on frame time
void Model::Control(int node, float frameTime, KeyCode turnUp, KeyCode turnDown, KeyCode turnLeft, KeyCode turnRight,
                                               KeyCode turnCW, KeyCode turnCCW, KeyCode moveForward, KeyCode moveBackward)
{
    auto& matrix = mWorldMatrices[node]; // Use reference to node matrix to make code below more readable

	if (KeyHeld( turnUp ))
	{
		matrix = MatrixRotationX(ROTATION_SPEED * frameTime) * matrix;
	}
	if (KeyHeld( turnDown ))
	{
		matrix = MatrixRotationX(-ROTATION_SPEED * frameTime) * matrix;
	}
	if (KeyHeld( turnRight ))
	{
		matrix = MatrixRotationY(ROTATION_SPEED * frameTime) * matrix;
	}
	if (KeyHeld( turnLeft ))
	{
		matrix = MatrixRotationY(-ROTATION_SPEED * frameTime) * matrix;
	}
	if (KeyHeld( turnCW ))
	{
		matrix = MatrixRotationZ(ROTATION_SPEED * frameTime) * matrix;
	}
	if (KeyHeld( turnCCW ))
	{
		matrix = MatrixRotationZ(-ROTATION_SPEED * frameTime) * matrix;
	}

	// Local Z movement - move in the direction of the Z axis, get axis from world matrix
    CVector3 localZDir = Normalise(matrix.GetRow(2)); // normalise axis in case world matrix has scaling
	if (KeyHeld( moveForward ))
	{

		matrix.SetRow(3, matrix.GetRow(3) + localZDir * MOVEMENT_SPEED * frameTime);
	}
	if (KeyHeld( moveBackward ))
	{
		matrix.SetRow(3, matrix.GetRow(3) - localZDir * MOVEMENT_SPEED * frameTime);
	}
}

//----------------//
//    New Code    //
//----------------//

//Set the states that DirectX will use when rendering this model
void Model::SetStates(ID3D11BlendState* BlendState, ID3D11DepthStencilState* DepthStencilState, ID3D11RasterizerState* Rasterizerstate)
{
	gD3DContext->OMSetBlendState(BlendState, nullptr, 0xffffff);
	gD3DContext->OMSetDepthStencilState(DepthStencilState, 0);
	gD3DContext->RSSetState(Rasterizerstate);
}

//Set the resources that the Pixel shader will need to render this model
void Model::SetShaderResources(UINT TextureSlot, ID3D11ShaderResourceView* Texture)
{
	gD3DContext->PSSetShaderResources(TextureSlot, 1, &Texture);
}

//Set the resources that the Pixel shader will need to render this model.
//Adds a normal map if the model requires one
void Model::SetShaderResources(UINT TextureSlot, ID3D11ShaderResourceView* Texture, UINT NormalMapSlot, ID3D11ShaderResourceView* NormalMap)
{
	gD3DContext->PSSetShaderResources(TextureSlot, 1, &Texture);
	gD3DContext->PSSetShaderResources(NormalMapSlot, 1, &NormalMap);
}

//Function overloading for the different scenarios of setting the shaders
void Model::Setup(ID3D11VertexShader* VertexShader)
{
	gD3DContext->VSSetShader(VertexShader, nullptr, 0);
}

void Model::Setup(ID3D11PixelShader* PixelShader)
{
	gD3DContext->PSSetShader(PixelShader, nullptr, 0);
}

void Model::Setup(ID3D11VertexShader* VertexShader, ID3D11PixelShader* PixelShader)
{
	gD3DContext->VSSetShader(VertexShader, nullptr, 0);
	gD3DContext->PSSetShader(PixelShader, nullptr, 0);
}

