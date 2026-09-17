#include "FFGLEventTimer.h"
using namespace ffglex;

enum ParamType : FFUInt32
{
	PT_RED,
	PT_GREEN,
	PT_BLUE,
	PT_ALPHA
};

static CFFGLPluginInfo PluginInfo(
	PluginFactory< FFGLEventTimer >,  // Create method
	"AIOT",                           // Plugin unique ID
	"All In One Timer",               // Plugin name
	2,                                // API major version number
	1,                                // API minor version number
	1,                                // Plugin major version number
	000,                              // Plugin minor version number
	FF_SOURCE,                        // Plugin type
	"Customizable event timer/countdown for live performances", // Plugin description
	"rodrmilano00"                    // Author
);

static const char vertexShaderCode[] = R"(#version 410 core
layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;

out vec2 uv;

void main()
{
	gl_Position = vPosition;
	uv = vUV;
}
)";

static const char fragmentShaderCode[] = R"(#version 410 core
uniform vec4 Color;

in vec2 uv;

out vec4 fragColor;

void main()
{
	fragColor = Color;
}
)";

FFGLEventTimer::FFGLEventTimer() :
	colorLocation( -1 )
{
	// Input properties
	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	// Parameters
	SetParamInfof( PT_RED, "Red", FF_TYPE_RED );
	SetParamInfof( PT_GREEN, "Green", FF_TYPE_GREEN );
	SetParamInfof( PT_BLUE, "Blue", FF_TYPE_BLUE );
	SetParamInfof( PT_ALPHA, "Alpha", FF_TYPE_ALPHA );

	FFGLLog::LogToHost( "Created All In One Timer" );
}

FFResult FFGLEventTimer::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	colorLocation = shader.FindUniform( "Color" );

	return CFFGLPlugin::InitGL( vp );
}

FFResult FFGLEventTimer::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	glUniform4f( colorLocation, 0.2f, 0.2f, 0.2f, 1.0f );

	quad.Draw();

	return FF_SUCCESS;
}

FFResult FFGLEventTimer::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();
	colorLocation = -1;

	return FF_SUCCESS;
}

FFResult FFGLEventTimer::SetFloatParameter( unsigned int dwIndex, float value )
{
	// TODO: Implement parameter handling
	return FF_SUCCESS;
}

float FFGLEventTimer::GetFloatParameter( unsigned int index )
{
	// TODO: Implement parameter getting
	return 0.0f;
}
