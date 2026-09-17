#include "FFGLEventTimer.h"
#include <cmath>
#include <cstring>

using namespace ffglex;

// 7-segment digit patterns: bit 0=a, 1=b, 2=c, 3=d, 4=e, 5=f, 6=g
const int FFGLEventTimer::s_digitPatterns[10] = {
	0x3F, // 0: a b c d e f
	0x06, // 1: b c
	0x5B, // 2: a b d e g
	0x4F, // 3: a b c d g
	0x66, // 4: b c f g
	0x6D, // 5: a c d f g
	0x7D, // 6: a c d e f g
	0x07, // 7: a b c
	0x7F, // 8: a b c d e f g
	0x6F  // 9: a b c d f g
};

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
layout(location = 1) in float aSegmentId;

uniform vec2  uPosition;
uniform float uSize;
uniform float uAspect;

out float vSegmentId;

void main()
{
	vec2 pos = aPosition;
	pos *= uSize;
	pos.x /= uAspect;
	pos += uPosition;
	gl_Position = vec4(pos, 0.0, 1.0);
	vSegmentId = aSegmentId;
}
)";

static const char fragmentShaderCode[] = R"(#version 410 core
in float vSegmentId;

uniform float uSegmentStates[46];
uniform vec4  uColor;
uniform vec4  uBgColor;
uniform float uShowBg;

out vec4 fragColor;

void main()
{
	int id = int(vSegmentId + 0.5);
	float active = uSegmentStates[id];
	vec4 segColor = mix(uBgColor, uColor, active);
	float visibility = max(active, uShowBg);
	fragColor = vec4(segColor.rgb, segColor.a * visibility);
}
)";

// Layout constants for the 7-segment display
// Digit: width=1.0, height=1.0, segment thickness=0.12
// Colon: width=0.3, height=1.0
// Spacing between characters: 0.1
// Layout: H1 H2 : M1 M2 : S1 S2
// Total width: 6*1.0 + 2*0.3 + 7*0.1 = 7.3
// Center offset: -3.65
static const float SEG_T = 0.12f;
static const float DIGIT_W = 1.0f;
static const float DIGIT_H = 1.0f;
static const float COLON_W = 0.3f;
static const float SPACING = 0.1f;
static const float TOTAL_W = 7.3f;
static const float CENTER_X = -TOTAL_W * 0.5f;

// Character x-offsets in the layout
static const float charOffsets[8] = {
	0.0f,              // H1
	1.1f,              // H2
	2.2f,              // :
	2.5f,              // M1
	3.6f,              // M2
	4.7f,              // :
	5.0f,              // S1
	6.1f               // S2
};

FFGLEventTimer::FFGLEventTimer() :
	vao( 0 ), vbo( 0 ), ibo( 0 ),
	uColorLocation( -1 ), uBgColorLocation( -1 ),
	uSegmentStatesLocation( -1 ), uPositionLocation( -1 ),
	uSizeLocation( -1 ), uAspectLocation( -1 ),
	uShowBgLocation( -1 ), uNumDigitsLocation( -1 )
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

	// Countdown time (0-3600s mapped to 0-1, default 60s = 60/3600 ≈ 0.0167)
	SetParamInfo( PT_COUNTDOWN_TIME, "Countdown Time", FF_TYPE_STANDARD, 60.0f / 3600.0f );
	SetParamRange( PT_COUNTDOWN_TIME, 0.0f, 1.0f );

	// Text color
	SetParamInfof( PT_RED, "Red", FF_TYPE_RED );
	SetParamInfof( PT_GREEN, "Green", FF_TYPE_GREEN );
	SetParamInfof( PT_BLUE, "Blue", FF_TYPE_BLUE );
	SetParamInfof( PT_ALPHA, "Alpha", FF_TYPE_ALPHA );

	// Size and position
	SetParamInfo( PT_SIZE, "Size", FF_TYPE_STANDARD, 0.5f );
	SetParamRange( PT_SIZE, 0.1f, 2.0f );
	SetParamInfo( PT_XPOS, "X Position", FF_TYPE_XPOS, 0.5f );
	SetParamInfo( PT_YPOS, "Y Position", FF_TYPE_YPOS, 0.5f );

	// Background
	SetParamInfo( PT_SHOW_BG, "Show BG", FF_TYPE_BOOLEAN, false );
	SetParamInfof( PT_BG_RED, "BG Red", FF_TYPE_RED );
	SetParamInfof( PT_BG_GREEN, "BG Green", FF_TYPE_GREEN );
	SetParamInfof( PT_BG_BLUE, "BG Blue", FF_TYPE_BLUE );
	SetParamInfo( PT_BG_ALPHA, "BG Alpha", FF_TYPE_ALPHA, 0.5f );

	// Parameter groups
	SetParamGroup( PT_START_PAUSE, "Timer Controls" );
	SetParamGroup( PT_RESET, "Timer Controls" );
	SetParamGroup( PT_FORMAT, "Timer Controls" );
	SetParamGroup( PT_MODE, "Timer Controls" );
	SetParamGroup( PT_COUNTDOWN_TIME, "Timer Controls" );
	SetParamGroup( PT_RED, "Text Color" );
	SetParamGroup( PT_GREEN, "Text Color" );
	SetParamGroup( PT_BLUE, "Text Color" );
	SetParamGroup( PT_ALPHA, "Text Color" );
	SetParamGroup( PT_SIZE, "Display" );
	SetParamGroup( PT_XPOS, "Display" );
	SetParamGroup( PT_YPOS, "Display" );
	SetParamGroup( PT_SHOW_BG, "Background" );
	SetParamGroup( PT_BG_RED, "Background" );
	SetParamGroup( PT_BG_GREEN, "Background" );
	SetParamGroup( PT_BG_BLUE, "Background" );
	SetParamGroup( PT_BG_ALPHA, "Background" );

	memset( m_segmentStates, 0, sizeof( m_segmentStates ) );
}

void FFGLEventTimer::BuildGeometry()
{
	// Each segment is a quad: 4 vertices + 6 indices
	// 6 digits * 7 segments + 2 colons * 2 dots = 46 segments
	// Total vertices: 46 * 4 = 184
	// Total indices: 46 * 6 = 276

	struct Vertex
	{
		float x, y, segId;
	};

	Vertex vertices[184];
	unsigned short indices[276];
	int vIdx = 0;
	int iIdx = 0;
	int segId = 0;

	auto addQuad = [&]( float x0, float y0, float x1, float y1, int sid )
	{
		vertices[vIdx] = { x0, y0, (float)sid };
		vertices[vIdx + 1] = { x1, y0, (float)sid };
		vertices[vIdx + 2] = { x1, y1, (float)sid };
		vertices[vIdx + 3] = { x0, y1, (float)sid };

		indices[iIdx]     = vIdx;
		indices[iIdx + 1] = vIdx + 1;
		indices[iIdx + 2] = vIdx + 2;
		indices[iIdx + 3] = vIdx;
		indices[iIdx + 4] = vIdx + 2;
		indices[iIdx + 5] = vIdx + 3;

		vIdx += 4;
		iIdx += 6;
	};

	// Build 7 segments for a digit at given x-offset
	auto addDigit = [&]( float xOffset, int baseSegId )
	{
		float w = DIGIT_W;
		float h = DIGIT_H;
		float t = SEG_T;

		// Segment a (top horizontal)
		addQuad( xOffset + t, h - t, xOffset + w - t, h, baseSegId + 0 );
		// Segment b (top-right vertical)
		addQuad( xOffset + w - t, h * 0.5f, xOffset + w, h - t, baseSegId + 1 );
		// Segment c (bottom-right vertical)
		addQuad( xOffset + w - t, t, xOffset + w, h * 0.5f, baseSegId + 2 );
		// Segment d (bottom horizontal)
		addQuad( xOffset + t, 0.0f, xOffset + w - t, t, baseSegId + 3 );
		// Segment e (bottom-left vertical)
		addQuad( xOffset, t, xOffset + t, h * 0.5f, baseSegId + 4 );
		// Segment f (top-left vertical)
		addQuad( xOffset, h * 0.5f, xOffset + t, h - t, baseSegId + 5 );
		// Segment g (middle horizontal)
		addQuad( xOffset + t, h * 0.5f - t * 0.5f, xOffset + w - t, h * 0.5f + t * 0.5f, baseSegId + 6 );
	};

	// Build colon (2 dots) at given x-offset
	auto addColon = [&]( float xOffset, int baseSegId )
	{
		float w = COLON_W;
		float h = DIGIT_H;
		float t = SEG_T;
		float dotSize = t * 1.5f;
		float cx = xOffset + w * 0.5f - dotSize * 0.5f;

		// Top dot
		addQuad( cx, h * 0.65f - dotSize * 0.5f, cx + dotSize, h * 0.65f + dotSize * 0.5f, baseSegId + 0 );
		// Bottom dot
		addQuad( cx, h * 0.35f - dotSize * 0.5f, cx + dotSize, h * 0.35f + dotSize * 0.5f, baseSegId + 1 );
	};

	// Build all 8 characters with centering offset
	// Segments: H1=0-6, H2=7-13, :=14-15, M1=16-22, M2=23-29, :=30-31, S1=32-38, S2=39-45
	addDigit( CENTER_X + charOffsets[0], 0 );   // H1
	addDigit( CENTER_X + charOffsets[1], 7 );   // H2
	addColon( CENTER_X + charOffsets[2], 14 );  // :
	addDigit( CENTER_X + charOffsets[3], 16 ); // M1
	addDigit( CENTER_X + charOffsets[4], 23 ); // M2
	addColon( CENTER_X + charOffsets[5], 30 );  // :
	addDigit( CENTER_X + charOffsets[6], 32 ); // S1
	addDigit( CENTER_X + charOffsets[7], 39 ); // S2

	// Upload to GPU
	glBindVertexArray( vao );

	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_STATIC_DRAW );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( indices ), indices, GL_STATIC_DRAW );

	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*)0 );

	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 1, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*)( sizeof( float ) * 2 ) );

	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
}

FFResult FFGLEventTimer::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}

	glGenVertexArrays( 1, &vao );
	glGenBuffers( 1, &vbo );
	glGenBuffers( 1, &ibo );

	BuildGeometry();

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	uColorLocation        = shader.FindUniform( "uColor" );
	uBgColorLocation      = shader.FindUniform( "uBgColor" );
	uSegmentStatesLocation = shader.FindUniform( "uSegmentStates" );
	uPositionLocation     = shader.FindUniform( "uPosition" );
	uSizeLocation         = shader.FindUniform( "uSize" );
	uAspectLocation       = shader.FindUniform( "uAspect" );
	uShowBgLocation       = shader.FindUniform( "uShowBg" );

	return CFFGLPlugin::InitGL( vp );
}

FFResult FFGLEventTimer::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	// Update timer
	auto now = std::chrono::steady_clock::now();
	if( m_running )
	{
		double delta = std::chrono::duration<double>( now - m_lastTime ).count();
		m_elapsedSeconds += delta;

		// Clamp countdown at 0
		if( paramMode > 0.5f )
		{
			double countdownStart = paramCountdownTime;
			double remaining = countdownStart - m_elapsedSeconds;
			if( remaining <= 0.0 )
			{
				m_elapsedSeconds = countdownStart;
				m_running = false;
			}
		}
	}
	m_lastTime = now;

	// Calculate display time
	double displayTime;
	if( paramMode > 0.5f )
	{
		// Countdown
		displayTime = paramCountdownTime - m_elapsedSeconds;
		if( displayTime < 0.0 ) displayTime = 0.0;
	}
	else
	{
		// Count up
		displayTime = m_elapsedSeconds;
	}

	// Update segment states
	UpdateSegmentStates( displayTime );

	// Get viewport dimensions for aspect ratio
	float aspect = 1.0f;
	if( currentViewport.width > 0 && currentViewport.height > 0 )
		aspect = (float)currentViewport.width / (float)currentViewport.height;

	// Convert position params (0-1) to NDC (-1 to 1)
	float posX = paramXPos * 2.0f - 1.0f;
	float posY = paramYPos * 2.0f - 1.0f;

	// Draw
	ScopedShaderBinding shaderBinding( shader.GetGLID() );

	glUniform4f( uColorLocation, paramRed, paramGreen, paramBlue, paramAlpha );
	glUniform4f( uBgColorLocation, paramBgRed, paramBgGreen, paramBgBlue, paramBgAlpha );
	glUniform1f( uShowBgLocation, paramShowBg ? 1.0f : 0.0f );
	glUniform1fv( uSegmentStatesLocation, NUM_SEGMENTS, m_segmentStates );
	glUniform2f( uPositionLocation, posX, posY );
	glUniform1f( uSizeLocation, paramSize );
	glUniform1f( uAspectLocation, aspect );

	glBindVertexArray( vao );
	glDrawElements( GL_TRIANGLES, 276, GL_UNSIGNED_SHORT, 0 );
	glBindVertexArray( 0 );

	// Unbind buffers to satisfy FFGL SDK state validation
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

	return FF_SUCCESS;
}

FFResult FFGLEventTimer::DeInitGL()
{
	if( vao ) { glDeleteVertexArrays( 1, &vao ); vao = 0; }
	if( vbo ) { glDeleteBuffers( 1, &vbo ); vbo = 0; }
	if( ibo ) { glDeleteBuffers( 1, &ibo ); ibo = 0; }

	shader.FreeGLResources();

	uColorLocation = -1;
	uBgColorLocation = -1;
	uSegmentStatesLocation = -1;
	uPositionLocation = -1;
	uSizeLocation = -1;
	uAspectLocation = -1;
	uShowBgLocation = -1;

	return FF_SUCCESS;
}

void FFGLEventTimer::UpdateSegmentStates( double totalSeconds )
{
	memset( m_segmentStates, 0, sizeof( m_segmentStates ) );

	if( totalSeconds < 0.0 ) totalSeconds = 0.0;

	int hours   = (int)( totalSeconds / 3600.0 );
	int minutes = (int)( fmod( totalSeconds, 3600.0 ) / 60.0 );
	int seconds = (int)( fmod( totalSeconds, 60.0 ) );

	bool showHours = ( paramFormat > 0.5f );

	// Set digit segments
	auto setDigit = [&]( int digitValue, int baseSegId )
	{
		int pattern = s_digitPatterns[digitValue % 10];
		for( int s = 0; s < 7; s++ )
		{
			m_segmentStates[baseSegId + s] = ( pattern & ( 1 << s ) ) ? 1.0f : 0.0f;
		}
	};

	if( showHours )
	{
		setDigit( hours / 10, 0 );   // H1
		setDigit( hours % 10, 7 );    // H2
		m_segmentStates[14] = 1.0f;   // colon 1 top
		m_segmentStates[15] = 1.0f;   // colon 1 bottom
	}
	else
	{
		// In MM:SS mode, hide H1, H2 and first colon
	}

	setDigit( minutes / 10, 16 );  // M1
	setDigit( minutes % 10, 23 );  // M2
	m_segmentStates[30] = 1.0f;     // colon 2 top
	m_segmentStates[31] = 1.0f;     // colon 2 bottom
	setDigit( seconds / 10, 32 );  // S1
	setDigit( seconds % 10, 39 );   // S2
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
		case PT_COUNTDOWN_TIME:
			// Map 0-1 to 0-3600 seconds
			paramCountdownTime = value * 3600.0f;
			break;
		case PT_RED:
			paramRed = value;
			break;
		case PT_GREEN:
			paramGreen = value;
			break;
		case PT_BLUE:
			paramBlue = value;
			break;
		case PT_ALPHA:
			paramAlpha = value;
			break;
		case PT_SIZE:
			paramSize = value;
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
		case PT_BG_RED:
			paramBgRed = value;
			break;
		case PT_BG_GREEN:
			paramBgGreen = value;
			break;
		case PT_BG_BLUE:
			paramBgBlue = value;
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
		case PT_COUNTDOWN_TIME: return paramCountdownTime / 3600.0f;
		case PT_RED:            return paramRed;
		case PT_GREEN:          return paramGreen;
		case PT_BLUE:           return paramBlue;
		case PT_ALPHA:          return paramAlpha;
		case PT_SIZE:           return paramSize;
		case PT_XPOS:           return paramXPos;
		case PT_YPOS:           return paramYPos;
		case PT_SHOW_BG:        return paramShowBg ? 1.0f : 0.0f;
		case PT_BG_RED:         return paramBgRed;
		case PT_BG_GREEN:       return paramBgGreen;
		case PT_BG_BLUE:        return paramBgBlue;
		case PT_BG_ALPHA:       return paramBgAlpha;
	}
	return 0.0f;
}
