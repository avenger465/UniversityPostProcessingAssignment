#pragma once
#include "GraphicsHelpers.h"
#include "Data/Mesh.h"
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include <cctype>
#include <map>
#include <atlbase.h>
#include <fstream>

class CResourceManager
{
//----------------------//
// Construction / Usage	//
//----------------------//
public:
	//Constructor
	CResourceManager();

	//Destructor
	~CResourceManager();

	//Function to load a texture into the textureMap 
	void loadTexture(const wchar_t* uniqueID, std::string filename);

	//Function to load a texture into the meshMap 
	void loadMesh(const wchar_t* uniqueID, std::string &filename, bool requireTangents = false);

	//Function to return the Texture at the given ID in the textureMap
	ID3D11ShaderResourceView* getTexture(const wchar_t* uid);

	//Function to return the Mesh at the given ID in the meshMap
	Mesh* getMesh(const wchar_t* uid);

//--------------------------//
// Private helper functions	//
//--------------------------//
private:
	//Helper Function to check whether the file given actually exists 
	bool doesFileExist(std::string &fileName);

//-------------//
// Member data //
//-------------//
private:
	ID3D11ShaderResourceView* texture;
	Mesh* mesh;

	std::map<wchar_t*, ID3D11ShaderResourceView*> textureMap;
	std::map<wchar_t*, Mesh*> meshMap;
};