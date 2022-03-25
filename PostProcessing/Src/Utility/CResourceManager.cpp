#include "CResourceManager.h"

//Constructor
CResourceManager::CResourceManager()
{
}

//Function to load a texture into the textureMap 
void CResourceManager::loadTexture(const wchar_t* uniqueID, std::string filename)
{
	HRESULT result;

	// Set the texture to the default one if this filename is not valid
	if (!doesFileExist(filename))
	{
		filename = "../Media/DefaultDiffuse.png";
	}

	std::string dds = ".dds"; //check the filename extension (case insensitive)
	if (filename.size() >= 4 &&
		std::equal(dds.rbegin(), dds.rend(), filename.rbegin(), [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); }))
	{
		result = DirectX::CreateDDSTextureFromFile(gD3DDevice, CA2CT(filename.c_str()), NULL, &texture);
	}
	else
	{
		result = DirectX::CreateWICTextureFromFile(gD3DDevice, gD3DContext, CA2CT(filename.c_str()), NULL, &texture);
	}

	textureMap.insert(std::make_pair(const_cast<wchar_t*>(uniqueID), texture));

}

//Function to load a texture into the meshMap 
void CResourceManager::loadMesh(const wchar_t* uniqueID, std::string &filename, bool requireTangents)
{
	// Set the texture to the default one if this filename is not valid
	if (!doesFileExist(filename))
	{
		filename = "Data/Teapot.x";
	}
	//Check if the Model requires tangents and if yes then create a new mesh with tangents
	//otherwise create a new mesh without tangents 
	if(requireTangents) mesh = new Mesh(filename, true);
	else mesh = new Mesh(filename);

	//Add the new mesh to the meshMap paired with the unique ID Created
	meshMap.insert(std::make_pair(const_cast<wchar_t*>(uniqueID), mesh));
}

//Function to return the Texture at the given ID in the textureMap
ID3D11ShaderResourceView* CResourceManager::getTexture(const wchar_t* uid)
{
	//Search through the textureMap for the requested texture
	//if no texture found then return the default Texture
	if (textureMap.find(const_cast<wchar_t*>(uid)) != textureMap.end())
	{
		// texture exists
		texture = textureMap.at(const_cast<wchar_t*>(uid));
		return texture;
	}
	else
	{
		return textureMap.at(L"default");
	}
}

//Function to return the Mesh at the given ID in the meshMap
Mesh* CResourceManager::getMesh(const wchar_t* uid)
{
	//Search through the textureMap for the requested texture
	//if no texture found then return the default Mesh
	if (meshMap.find(const_cast<wchar_t*>(uid)) != meshMap.end())
	{
		// texture exists
		mesh = meshMap.at(const_cast<wchar_t*>(uid));
		return mesh;
	}
	else
	{
		return meshMap.at(L"default");
	}
}

//Helper Function to check whether the file given actually exists 
bool CResourceManager::doesFileExist(std::string &fname)
{
	//Checks if the string containing the FileName is viable
	std::ifstream infile(fname);
	return infile.good();
}

//Destructor
CResourceManager::~CResourceManager()
{
	if (texture) texture->Release();
	if (mesh) mesh->~Mesh();

	for (auto it = textureMap.cbegin(), next_it = it; it != textureMap.cend(); it = next_it)
	{
		++next_it;
		textureMap.erase(it);
	}
	for (auto it = meshMap.cbegin(), next_it = it; it != meshMap.cend(); it = next_it)
	{
		++next_it;
		meshMap.erase(it);
	}
}