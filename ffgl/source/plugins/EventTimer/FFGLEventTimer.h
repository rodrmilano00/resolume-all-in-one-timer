#pragma once
#include <FFGLSDK.h>
#include <chrono>

class FFGLEventTimer : public CFFGLPlugin
{
public:
	FFGLEventTimer();

	//CFFGLPlugin
	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;

	FFResult SetFloatParameter( unsigned int dwIndex, float value ) override;
	float GetFloatParameter( unsigned int index ) override;

private:
	enum ParamType : FFUInt32
	{
		PT_START_PAUSE,    // FF_TYPE_EVENT - trigger to start/pause
		PT_RESET,          // FF_TYPE_EVENT - trigger to reset
		PT_FORMAT,         // FF_TYPE_OPTION - 0=mm:ss, 1=hh:mm:ss
		PT_MODE,           // FF_TYPE_OPTION - 0=count up, 1=countdown
		PT_COUNTDOWN_TIME, // FF_TYPE_STANDARD - countdown start time (0-3600s mapped 0-1)
		PT_RED,            // FF_TYPE_RED - text color
		PT_GREEN,          // FF_TYPE_GREEN
		PT_BLUE,           // FF_TYPE_BLUE
		PT_ALPHA,          // FF_TYPE_ALPHA
		PT_SIZE,           // FF_TYPE_STANDARD - digit size
		PT_XPOS,           // FF_TYPE_XPOS
		PT_YPOS,           // FF_TYPE_YPOS
		PT_SHOW_BG,        // FF_TYPE_BOOLEAN - show/hide background
		PT_BG_RED,         // FF_TYPE_RED - background color
		PT_BG_GREEN,       // FF_TYPE_GREEN
		PT_BG_BLUE,        // FF_TYPE_BLUE
		PT_BG_ALPHA        // FF_TYPE_ALPHA
	};

	// Parameter values
	float paramRed   = 0.0f;
	float paramGreen = 1.0f;
	float paramBlue  = 0.0f;
	float paramAlpha = 1.0f;
	float paramSize  = 0.5f;
	float paramXPos  = 0.5f;
	float paramYPos  = 0.5f;
	float paramFormat = 0.0f;  // 0=mm:ss, 1=hh:mm:ss
	float paramMode   = 0.0f;  // 0=count up, 1=countdown
	float paramCountdownTime = 60.0f;
	bool   paramShowBg = false;
	float paramBgRed   = 0.0f;
	float paramBgGreen = 0.0f;
	float paramBgBlue  = 0.0f;
	float paramBgAlpha = 0.5f;

	// Timer state
	bool m_running = false;
	double m_elapsedSeconds = 0.0;
	std::chrono::steady_clock::time_point m_lastTime;

	// 7-segment digit patterns: segments a,b,c,d,e,f,g
	// bit 0=a(top), 1=b(top-right), 2=c(bottom-right), 3=d(bottom),
	// 4=e(bottom-left), 5=f(top-left), 6=g(middle)
	static const int s_digitPatterns[10];

	// OpenGL resources
	ffglex::FFGLShader shader;
	GLuint vao;
	GLuint vbo;
	GLuint ibo;
	GLint uColorLocation;
	GLint uBgColorLocation;
	GLint uSegmentStatesLocation;
	GLint uPositionLocation;
	GLint uSizeLocation;
	GLint uAspectLocation;
	GLint uShowBgLocation;
	GLint uNumDigitsLocation;

	// Segment geometry: 6 digits * 7 segments + 2 colons * 2 dots = 46 segments
	static const int NUM_SEGMENTS = 46;
	float m_segmentStates[NUM_SEGMENTS];

	void BuildGeometry();
	void UpdateSegmentStates( double totalSeconds );
	int  GetDigit( double totalSeconds, int position );
};
