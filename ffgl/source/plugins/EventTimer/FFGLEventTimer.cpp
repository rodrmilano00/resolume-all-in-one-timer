// Include windows.h first so GDI text functions are available
// (FFGL.h includes it with many excludes like NOUSER, NODRAWTEXT)
#include <windows.h>
#include "FFGLEventTimer.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <cstdarg>

using namespace ffglex;

// Debug logging
static void DebugLog( const char* format, ... )
{
	static FILE* logFile = nullptr;
	if( !logFile )
		fopen_s( &logFile, "C:\\Users\\luis1\\CascadeProjects\\resolume-all-in-one-timer\\debug.log", "a" );
	if( logFile )
	{
		va_list args;
		va_start( args, format );
		vfprintf( logFile, format, args );
		va_end( args );
		fprintf( logFile, "\n" );
		fflush( logFile );
	}
}

static CFFGLPluginInfo PluginInfo(
	PluginFactory< FFGLEventTimer >,
	"AIOT",
	"All In One Timer",
	2, 1,
	1, 0,
	FF_SOURCE,
	"Customizable event timer/countdown for live performances",
	"rodrmilano00"
);

static const char vertexShaderCode[] = R"(#version 410 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

uniform vec2  uPosition;
uniform float uSize;
uniform float uAspect;
uniform float uTexAspect;

out vec2 vTexCoord;

void main()
{
	vec2 pos = aPosition;
	pos.x *= uTexAspect;
	pos *= uSize;
	pos.x /= uAspect;
	pos += uPosition;
	gl_Position = vec4(pos, 0.0, 1.0);
	vTexCoord = aTexCoord;
}
)";

static const char fragmentShaderCode[] = R"(#version 410 core
in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform vec4  uColor;
uniform vec4  uBgColor;
uniform float uShowBg;

out vec4 fragColor;

void main()
{
	vec4 texColor = texture(uTexture, vTexCoord);
	float textAlpha = texColor.a;
	vec4 textColor = vec4(uColor.rgb, uColor.a * textAlpha);
	if (uShowBg > 0.5)
	{
		fragColor = mix(uBgColor, textColor, textAlpha);
	}
	else
	{
		fragColor = textColor;
	}
}
)";

// Callback for enumerating installed fonts
static std::vector<std::string>* g_fontList = nullptr;
static int CALLBACK EnumFontProc( const LOGFONTA* lpelfe, const TEXTMETRICA* lpntme, DWORD fontType, LPARAM lParam )
{
	if( g_fontList && lpelfe )
	{
		std::string name( lpelfe->lfFaceName );
		// Avoid duplicates (Windows enumerates each font per charset)
		bool found = false;
		for( const auto& f : *g_fontList )
		{
			if( f == name ) { found = true; break; }
		}
		if( !found )
			g_fontList->push_back( name );
	}
	return 1; // continue enumeration
}

FFGLEventTimer::FFGLEventTimer() :
	vao( 0 ), vbo( 0 ), ibo( 0 ),
	textTexture( 0 ), textureWidth( 0 ), textureHeight( 0 ),
	m_fontChanged( true ),
	uColorLocation( -1 ), uBgColorLocation( -1 ),
	uPositionLocation( -1 ), uSizeLocation( -1 ),
	uAspectLocation( -1 ), uShowBgLocation( -1 ),
	uTexAspectLocation( -1 ), uTextureLocation( -1 )
{
	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	// Triggers
	SetParamInfo( PT_START_PAUSE, "Start/Pause", FF_TYPE_EVENT, false );
	SetParamInfo( PT_RESET, "Reset", FF_TYPE_EVENT, false );

	// Options
	SetOptionParamInfo( PT_FORMAT, "Format", 2, 0.0f );
	SetParamElementInfo( PT_FORMAT, 0, "MM:SS", 0.0f );
	SetParamElementInfo( PT_FORMAT, 1, "HH:MM:SS", 1.0f );

	SetOptionParamInfo( PT_MODE, "Mode", 2, 0.0f );
	SetParamElementInfo( PT_MODE, 0, "Count Up", 0.0f );
	SetParamElementInfo( PT_MODE, 1, "Countdown", 1.0f );

	// Countdown time - separate hours/minutes/seconds for intuitive setting
	SetParamInfo( PT_CD_HOURS, "CD Hours", FF_TYPE_STANDARD, 0.0f );
	SetParamRange( PT_CD_HOURS, 0.0f, 23.0f );
	SetParamInfo( PT_CD_MINUTES, "CD Minutes", FF_TYPE_STANDARD, 1.0f );
	SetParamRange( PT_CD_MINUTES, 0.0f, 59.0f );
	SetParamInfo( PT_CD_SECONDS, "CD Seconds", FF_TYPE_STANDARD, 0.0f );
	SetParamRange( PT_CD_SECONDS, 0.0f, 59.0f );

	// Text color (HSB - native Resolume color picker)
	SetParamInfo( PT_HUE, "Hue", FF_TYPE_HUE, 0.33f );
	SetParamInfo( PT_SATURATION, "Saturation", FF_TYPE_SATURATION, 1.0f );
	SetParamInfo( PT_BRIGHTNESS, "Brightness", FF_TYPE_BRIGHTNESS, 1.0f );
	SetParamInfo( PT_ALPHA, "Alpha", FF_TYPE_ALPHA, 1.0f );

	// Size and position
	SetParamInfo( PT_SIZE, "Size", FF_TYPE_STANDARD, 0.3f );
	SetParamRange( PT_SIZE, 0.05f, 2.0f );
	SetParamInfo( PT_XPOS, "X Position", FF_TYPE_XPOS, 0.5f );
	SetParamInfo( PT_YPOS, "Y Position", FF_TYPE_YPOS, 0.5f );

	// Enumerate installed fonts and create dropdown
	HDC hdc = GetDC( NULL );
	g_fontList = &m_fontNames;
	LOGFONTA lf = {};
	lf.lfCharSet = DEFAULT_CHARSET;
	EnumFontFamiliesExA( hdc, &lf, EnumFontProc, 0, 0 );
	ReleaseDC( NULL, hdc );
	g_fontList = nullptr;

	// Sort fonts alphabetically
	std::sort( m_fontNames.begin(), m_fontNames.end() );

	// Find default font index (Arial)
	int defaultFontIdx = 0;
	for( size_t i = 0; i < m_fontNames.size(); i++ )
	{
		if( m_fontNames[i] == "Arial" ) { defaultFontIdx = (int)i; break; }
	}

	int numFonts = (int)m_fontNames.size();
	if( numFonts > 0 )
	{
		SetOptionParamInfo( PT_FONT_NAME, "Font", numFonts, (float)defaultFontIdx );
		for( int i = 0; i < numFonts; i++ )
		{
			// Truncate to 16 chars for FFGL spec
			char truncated[17];
			strncpy_s( truncated, sizeof( truncated ), m_fontNames[i].c_str(), _TRUNCATE );
			SetParamElementInfo( PT_FONT_NAME, i, truncated, (float)i );
		}
	}
	else
	{
		// Fallback if no fonts found
		SetOptionParamInfo( PT_FONT_NAME, "Font", 1, 0.0f );
		SetParamElementInfo( PT_FONT_NAME, 0, "Arial", 0.0f );
		m_fontNames.push_back( "Arial" );
	}

	// Background
	SetParamInfo( PT_SHOW_BG, "Show BG", FF_TYPE_BOOLEAN, false );
	SetParamInfo( PT_BG_HUE, "BG Hue", FF_TYPE_HUE, 0.0f );
	SetParamInfo( PT_BG_SATURATION, "BG Sat", FF_TYPE_SATURATION, 0.0f );
	SetParamInfo( PT_BG_BRIGHTNESS, "BG Bright", FF_TYPE_BRIGHTNESS, 0.0f );
	SetParamInfo( PT_BG_ALPHA, "BG Alpha", FF_TYPE_ALPHA, 0.5f );

	// Parameter groups
	SetParamGroup( PT_START_PAUSE, "Timer Controls" );
	SetParamGroup( PT_RESET, "Timer Controls" );
	SetParamGroup( PT_FORMAT, "Timer Controls" );
	SetParamGroup( PT_MODE, "Timer Controls" );
	SetParamGroup( PT_CD_HOURS, "Timer Controls" );
	SetParamGroup( PT_CD_MINUTES, "Timer Controls" );
	SetParamGroup( PT_CD_SECONDS, "Timer Controls" );
	SetParamGroup( PT_HUE, "Text Color" );
	SetParamGroup( PT_SATURATION, "Text Color" );
	SetParamGroup( PT_BRIGHTNESS, "Text Color" );
	SetParamGroup( PT_ALPHA, "Text Color" );
	SetParamGroup( PT_SIZE, "Display" );
	SetParamGroup( PT_XPOS, "Display" );
	SetParamGroup( PT_YPOS, "Display" );
	SetParamGroup( PT_FONT_NAME, "Display" );
	SetParamGroup( PT_SHOW_BG, "Background" );
	SetParamGroup( PT_BG_HUE, "Background" );
	SetParamGroup( PT_BG_SATURATION, "Background" );
	SetParamGroup( PT_BG_BRIGHTNESS, "Background" );
	SetParamGroup( PT_BG_ALPHA, "Background" );
}

FFResult FFGLEventTimer::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}

	// Create quad geometry: 4 vertices, 6 indices
	struct Vertex
	{
		float x, y, u, v;
	};

	// V=0 at top, V=1 at bottom to match top-down DIB (fixes upside-down text)
	Vertex vertices[4] = {
		{ -1.0f, -1.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 1.0f, 1.0f },
		{  1.0f,  1.0f, 1.0f, 0.0f },
		{ -1.0f,  1.0f, 0.0f, 0.0f }
	};
	unsigned short indices[6] = { 0, 1, 2, 0, 2, 3 };

	glGenVertexArrays( 1, &vao );
	glGenBuffers( 1, &vbo );
	glGenBuffers( 1, &ibo );
	glGenTextures( 1, &textTexture );

	glBindVertexArray( vao );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_STATIC_DRAW );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( indices ), indices, GL_STATIC_DRAW );

	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*)0 );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*)( sizeof( float ) * 2 ) );

	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );

	// Initial text render
	RenderTextToTexture( "00:00" );

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	uColorLocation     = shader.FindUniform( "uColor" );
	uBgColorLocation   = shader.FindUniform( "uBgColor" );
	uPositionLocation  = shader.FindUniform( "uPosition" );
	uSizeLocation      = shader.FindUniform( "uSize" );
	uAspectLocation    = shader.FindUniform( "uAspect" );
	uShowBgLocation    = shader.FindUniform( "uShowBg" );
	uTexAspectLocation = shader.FindUniform( "uTexAspect" );
	uTextureLocation   = shader.FindUniform( "uTexture" );

	return CFFGLPlugin::InitGL( vp );
}

std::string FFGLEventTimer::GetSelectedFontName()
{
	int idx = (int)( paramFontIndex + 0.5f );
	if( idx < 0 ) idx = 0;
	if( idx >= (int)m_fontNames.size() ) idx = (int)m_fontNames.size() - 1;
	if( m_fontNames.empty() ) return "Arial";
	return m_fontNames[idx];
}

void FFGLEventTimer::RenderTextToTexture( const std::string& text )
{
	const int fontSize = 400; // High resolution for crisp text
	std::string fontName = GetSelectedFontName();

	// Ensure we bind on texture unit 0
	glActiveTexture( GL_TEXTURE0 );

	HDC hdc = CreateCompatibleDC( NULL );

	// Create font to measure text
	HFONT hFont = CreateFontA(
		fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		fontName.c_str()
	);
	HFONT hOldFont = (HFONT)SelectObject( hdc, hFont );

	// Measure text size
	RECT rcMeasure = { 0, 0, 0, 0 };
	DrawTextA( hdc, text.c_str(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE );

	// Add padding to avoid cut-off
	int padX = 40;
	int padY = 20;
	int texW = rcMeasure.right + padX * 2;
	int texH = rcMeasure.bottom + padY * 2;

	// Round up to power of 2 for better GPU compatibility
	// (not strictly required for modern GPUs but good practice)
	if( texW < 64 ) texW = 64;
	if( texH < 64 ) texH = 64;

	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bi.bmiHeader.biWidth = texW;
	bi.bmiHeader.biHeight = -texH; // top-down DIB
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;

	void* bits = NULL;
	HBITMAP hBmp = CreateDIBSection( hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0 );
	HBITMAP hOldBmp = (HBITMAP)SelectObject( hdc, hBmp );

	// Clear to transparent black
	memset( bits, 0, (size_t)texW * texH * 4 );

	SetTextColor( hdc, RGB( 255, 255, 255 ) );
	SetBkMode( hdc, TRANSPARENT );

	// Draw text centered in the texture
	RECT rc = { padX, padY, texW - padX, texH - padY };
	DrawTextA( hdc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE );

	// Fix alpha channel: set alpha = brightness of each pixel
	unsigned char* pixels = (unsigned char*)bits;
	for( int i = 0; i < texW * texH; i++ )
	{
		int idx = i * 4;
		// DIB is BGRA, alpha = max of B,G,R (text is white so all channels are equal)
		pixels[idx + 3] = (unsigned char)( ( pixels[idx] + pixels[idx + 1] + pixels[idx + 2] ) / 3 );
	}

	// Upload to OpenGL texture
	glBindTexture( GL_TEXTURE_2D, textTexture );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_BGRA, GL_UNSIGNED_BYTE, bits );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindTexture( GL_TEXTURE_2D, 0 );

	// Cleanup GDI
	SelectObject( hdc, hOldFont );
	DeleteObject( hFont );
	SelectObject( hdc, hOldBmp );
	DeleteObject( hBmp );
	DeleteDC( hdc );

	textureWidth = texW;
	textureHeight = texH;
}

std::string FFGLEventTimer::GetTimeString()
{
	double countdownTotal = (double)paramCdHours * 3600.0 + (double)paramCdMinutes * 60.0 + (double)paramCdSeconds;

	double displayTime;
	if( paramMode > 0.5f )
	{
		displayTime = countdownTotal - m_elapsedSeconds;
		if( displayTime < 0.0 ) displayTime = 0.0;
	}
	else
	{
		displayTime = m_elapsedSeconds;
	}

	int hours   = (int)( displayTime / 3600.0 );
	int minutes = (int)( fmod( displayTime, 3600.0 ) / 60.0 );
	int seconds = (int)( fmod( displayTime, 60.0 ) );

	char buf[32];
	if( paramFormat > 0.5f )
	{
		snprintf( buf, sizeof( buf ), "%02d:%02d:%02d", hours, minutes, seconds );
	}
	else
	{
		snprintf( buf, sizeof( buf ), "%02d:%02d", minutes, seconds );
	}
	return std::string( buf );
}

void FFGLEventTimer::HSBtoRGB( float h, float s, float b, float& r, float& g, float& bl )
{
	float i = floor( h * 6.0f );
	float f = h * 6.0f - i;
	float p = b * ( 1.0f - s );
	float q = b * ( 1.0f - s * f );
	float t = b * ( 1.0f - s * ( 1.0f - f ) );

	switch( (int)i % 6 )
	{
		case 0: r = b; g = t; bl = p; break;
		case 1: r = q; g = b; bl = p; break;
		case 2: r = p; g = b; bl = t; break;
		case 3: r = p; g = q; bl = b; break;
		case 4: r = t; g = p; bl = b; break;
		case 5: r = b; g = p; bl = q; break;
	}
}

FFResult FFGLEventTimer::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	// Clean up ALL OpenGL state that previous plugins may have left bound.
	// The FFGL SDK validates context state BEFORE calling ProcessOpenGL (line 355 in FFGL.cpp),
	// so if the previous plugin (e.g. Resolume text source) left textures/shaders bound,
	// the assertion will fire before our cleanup code at the end can run.
	// We must clean everything at the START to ensure the pre-validation passes.

	// Unbind any shader program left by previous plugin
	glUseProgram( 0 );

	// Unbind any VAO left by previous plugin
	glBindVertexArray( 0 );

	// Unbind any buffers left by previous plugin
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

	// Unbind ALL texture types on ALL texture units
	GLint maxTexUnits;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &maxTexUnits );

	static const GLenum texTypes[] = {
		GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D,
		GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY,
		GL_TEXTURE_RECTANGLE, GL_TEXTURE_CUBE_MAP,
		GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_BUFFER,
		GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY
	};

	for( GLint i = 0; i < maxTexUnits; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + i );
		for( size_t t = 0; t < sizeof(texTypes)/sizeof(texTypes[0]); t++ )
		{
			glBindTexture( texTypes[t], 0 );
		}
	}
	glActiveTexture( GL_TEXTURE0 );

	// Restore blend state to defaults
	glDisable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ZERO );

	// Update timer
	auto now = std::chrono::steady_clock::now();
	double delta = std::chrono::duration<double>( now - m_lastTime ).count();

	// If delta is too large, the plugin was not being rendered (layer removed from program).
	// Reset m_lastTime without advancing the timer, effectively pausing while inactive.
	if( delta > 0.5 )
	{
		m_lastTime = now;
		delta = 0.0;
	}

	if( m_running && delta > 0.0 )
	{
		m_elapsedSeconds += delta;

		if( paramMode > 0.5f )
		{
			double countdownTotal = (double)paramCdHours * 3600.0 + (double)paramCdMinutes * 60.0 + (double)paramCdSeconds;
			double remaining = countdownTotal - m_elapsedSeconds;
			if( remaining <= 0.0 )
			{
				m_elapsedSeconds = countdownTotal;
				m_running = false;
			}
		}
	}
	m_lastTime = now;

	// Re-render text only when the displayed string changes or font changed
	std::string currentText = GetTimeString();
	if( m_fontChanged || currentText != lastRenderedText )
	{
		RenderTextToTexture( currentText );
		lastRenderedText = currentText;
		m_fontChanged = false;
	}

	// Get viewport dimensions for aspect ratio
	float aspect = 1.0f;
	if( currentViewport.width > 0 && currentViewport.height > 0 )
		aspect = (float)currentViewport.width / (float)currentViewport.height;

	// Convert position params (0-1) to NDC (-1 to 1)
	float posX = paramXPos * 2.0f - 1.0f;
	float posY = paramYPos * 2.0f - 1.0f;

	// Convert HSB to RGB
	float r, g, bl;
	HSBtoRGB( paramHue, paramSaturation, paramBrightness, r, g, bl );

	float bgR, bgG, bgB;
	HSBtoRGB( paramBgHue, paramBgSaturation, paramBgBrightness, bgR, bgG, bgB );

	float texAspect = ( textureHeight > 0 ) ? (float)textureWidth / (float)textureHeight : 4.0f;

	// Enable blending for transparency
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	// Draw
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	glUniform4f( uColorLocation, r, g, bl, paramAlpha );
	glUniform4f( uBgColorLocation, bgR, bgG, bgB, paramBgAlpha );
	glUniform1f( uShowBgLocation, paramShowBg ? 1.0f : 0.0f );
	glUniform2f( uPositionLocation, posX, posY );
	glUniform1f( uSizeLocation, paramSize );
	glUniform1f( uAspectLocation, aspect );
	glUniform1f( uTexAspectLocation, texAspect );

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, textTexture );
	glUniform1i( uTextureLocation, 0 );

	glBindVertexArray( vao );
	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );
	glBindVertexArray( 0 );

	// Unbind everything to satisfy FFGL SDK state validation
	for( GLint i = 0; i < maxTexUnits; i++ )
	{
		glActiveTexture( GL_TEXTURE0 + i );
		for( size_t t = 0; t < sizeof(texTypes)/sizeof(texTypes[0]); t++ )
		{
			glBindTexture( texTypes[t], 0 );
		}
	}
	glActiveTexture( GL_TEXTURE0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

	// Restore blend state to defaults expected by FFGL SDK
	glDisable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ZERO );

	return FF_SUCCESS;
}

FFResult FFGLEventTimer::DeInitGL()
{
	if( textTexture ) { glDeleteTextures( 1, &textTexture ); textTexture = 0; }
	if( vao ) { glDeleteVertexArrays( 1, &vao ); vao = 0; }
	if( vbo ) { glDeleteBuffers( 1, &vbo ); vbo = 0; }
	if( ibo ) { glDeleteBuffers( 1, &ibo ); ibo = 0; }

	shader.FreeGLResources();

	uColorLocation = -1;
	uBgColorLocation = -1;
	uPositionLocation = -1;
	uSizeLocation = -1;
	uAspectLocation = -1;
	uShowBgLocation = -1;
	uTexAspectLocation = -1;
	uTextureLocation = -1;

	return FF_SUCCESS;
}

FFResult FFGLEventTimer::SetFloatParameter( unsigned int dwIndex, float value )
{
	switch( dwIndex )
	{
		case PT_START_PAUSE:
			if( value > 0.5f )
			{
				if( !m_running )
				{
					m_running = true;
					m_lastTime = std::chrono::steady_clock::now();
				}
				else
				{
					m_running = false;
				}
			}
			break;
		case PT_RESET:
			if( value > 0.5f )
			{
				m_running = false;
				m_elapsedSeconds = 0.0;
			}
			break;
		case PT_FORMAT:
			paramFormat = value;
			break;
		case PT_MODE:
			paramMode = value;
			break;
		case PT_CD_HOURS:
			paramCdHours = value;
			break;
		case PT_CD_MINUTES:
			paramCdMinutes = value;
			break;
		case PT_CD_SECONDS:
			paramCdSeconds = value;
			break;
		case PT_HUE:
			paramHue = value;
			break;
		case PT_SATURATION:
			paramSaturation = value;
			break;
		case PT_BRIGHTNESS:
			paramBrightness = value;
			break;
		case PT_ALPHA:
			paramAlpha = value;
			break;
		case PT_SIZE:
			paramSize = value;
			break;
		case PT_FONT_NAME:
			paramFontIndex = value;
			m_fontChanged = true;
			break;
		case PT_XPOS:
			paramXPos = value;
			break;
		case PT_YPOS:
			paramYPos = value;
			break;
		case PT_SHOW_BG:
			paramShowBg = value > 0.5f;
			break;
		case PT_BG_HUE:
			paramBgHue = value;
			break;
		case PT_BG_SATURATION:
			paramBgSaturation = value;
			break;
		case PT_BG_BRIGHTNESS:
			paramBgBrightness = value;
			break;
		case PT_BG_ALPHA:
			paramBgAlpha = value;
			break;
	}
	return FF_SUCCESS;
}

float FFGLEventTimer::GetFloatParameter( unsigned int index )
{
	switch( index )
	{
		case PT_FORMAT:         return paramFormat;
		case PT_MODE:           return paramMode;
		case PT_CD_HOURS:       return paramCdHours;
		case PT_CD_MINUTES:     return paramCdMinutes;
		case PT_CD_SECONDS:     return paramCdSeconds;
		case PT_HUE:            return paramHue;
		case PT_SATURATION:     return paramSaturation;
		case PT_BRIGHTNESS:     return paramBrightness;
		case PT_ALPHA:          return paramAlpha;
		case PT_SIZE:           return paramSize;
		case PT_FONT_NAME:     return paramFontIndex;
		case PT_XPOS:           return paramXPos;
		case PT_YPOS:           return paramYPos;
		case PT_SHOW_BG:        return paramShowBg ? 1.0f : 0.0f;
		case PT_BG_HUE:         return paramBgHue;
		case PT_BG_SATURATION:  return paramBgSaturation;
		case PT_BG_BRIGHTNESS:  return paramBgBrightness;
		case PT_BG_ALPHA:       return paramBgAlpha;
	}
	return 0.0f;
}

