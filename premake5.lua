workspace "PostProcessing"
	architecture "x64"
	startproject "PostProcessing"
	 
	configurations
	{
		"Debug",
		"Release",
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["ImGui"]		    = "PostProcessing/External/GUI"
IncludeDir["ImGuiBackends"] = "PostProcessing/External/GUI/backends"
IncludeDir["Assimp"]		= "PostProcessing/External/assimp/include"
IncludeDir["DirectX"]		= "PostProcessing/External/DirectXTK"


LibDir = {}
LibDir["assimp"]	= "PostProcessing/External/assimp/lib/xSixtyFour"
LibDir["DirectXTK"] = "PostProcessing/External/DirectXTK/%{cfg.buildcfg}"

include "PostProcessing/External/GUI"

project "PostProcessing"
	location "PostProcessing"
	kind "WindowedApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	--pchheader "tepch.h"
	--pchsource "DirectXEngine/Src/tepch.cpp"

	files
	{
		"%{prj.name}/Src/**.cpp",
		"%{prj.name}/Src/**.h",
		"%{prj.name}/External/GUI/backends/imgui_impl_dx11.h",
		"%{prj.name}/External/GUI/backends/imgui_impl_dx11.cpp",
		"%{prj.name}/External/GUI/backends/imgui_impl_win32.h",
		"%{prj.name}/External/GUI/backends/imgui_impl_win32.cpp",
		"%{prj.name}/Src/Shaders/Common.hlsli"

	}

		
	includedirs
	{
		"%{prj.name}/Src",
		"%{IncludeDir.Assimp}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuiBackends}",
		"%{IncludeDir.DirectX}"
	}

	libdirs
	{
		"%{LibDir.assimp}",
		"%{LibDir.DirectXTK}"
	}

	links
	{
		"ImGui",
		"d3d11.lib",
		"assimp-vc142-mt.lib",
		"DirectXTK.lib",
		"d3dcompiler.lib",
		"winmm.lib"
	}

	files("PostProcessing/Src/Shaders/*.hlsl")
	shadermodel("5.0")

	local shader_dir = "Src/Shaders/"
	
	filter("files:**.hlsl")
		shaderobjectfileoutput(shader_dir.."%{file.basename}"..".cso")
	
	filter("files:**_ps.hlsl")
		shadertype("Pixel")

	filter("files:**_gs.hlsl")
		shadertype("Geometry")

	filter("files:**_vs.hlsl")
		shadertype("Vertex")
		shaderoptions({"/WX"})

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"PPE_PLATFORM_WINDOWS",
		}

	filter "configurations:Debug"
		defines "PPE_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "PPE_Release"
		runtime "Release"
		optimize "on"	