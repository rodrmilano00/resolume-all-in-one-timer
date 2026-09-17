#pragma once
#include <FFGLSDK.h>
#include <chrono>
#include <string>

class FFGLEventTimer : public CFFGLPlugin
{
public:
	FFGLEventTimer();

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;

	FFResult SetFloatParameter( unsigned int dwIndex, float value ) override;
	float GetFloatParameter( unsigned int index ) override;
	FFResult SetTextParameter( unsigned int index, const char* value ) override;
	char* GetTextParameter( unsigned int index ) override;

private:
	enum ParamType : FFUInt32
	{
		PT_START_PAUSE,    // FF_TYPE_EVENT
		PT_RESET,          // FF_TYPE_EVENT
		PT_FORMAT,         // FF_TYPE_OPTION - 0=mm:ss, 1=hh:mm:ss
		PT_MODE,           // FF_TYPE_OPTION - 0=count up, 1=countdown
		PT_COUNTDOWN_TIME, // FF_TYPE_STANDARD
		PT_HUE,            // FF_TYPE_HUE - text color
		PT_SATURATION,     // FF_TYPE_SATURATION
		PT_BRIGHTNESS,     // FF_TYPE_BRIGHTNESS
		PT_ALPHA,          // FF_TYPE_ALPHA
		PT_SIZE,           // FF_TYPE_STANDARD
		PT_XPOS,           // FF_TYPE_XPOS
		PT_YPOS,           // FF_TYPE_YPOS
		PT_FONT_NAME,      // FF_TYPE_TEXT - font name
		PT_SHOW_BG,        // FF_TYPE_BOOLEAN
		PT_BG_HUE,         // FF_TYPE_HUE - background color
		PT_BG_SATURATION,  // FF_TYPE_SATURATION
		PT_BG_BRIGHTNESS,  // FF_TYPE_BRIGHTNESS
		PT_BG_ALPHA        // FF_TYPE_ALPHA
	};

	// Color params (HSB)
	float paramHue        = 0.33f;
	float paramSaturation = 1.0f;
	float paramBrightness = 1.0f;
	float paramAlpha      = 1.0f;

	// Background color (HSB)
	float paramBgHue        = 0.0f;
	float paramBgSaturation = 0.0f;
	float paramBgBrightness = 0.0f;
	float paramBgAlpha      = 0.5f;
	bool  paramShowBg       = false;

	// Size and position
	float paramSize = 0.3f;
	float paramXPos = 0.5f;
	float paramYPos = 0.5f;

	// Timer params
	float paramFormat = 0.0f;
	float paramMode   = 0.0f;
	float paramCountdownTime = 60.0f;

	// Font
	std::string paramFontName = "Arial";

	// Timer state
	bool m_running = false;
	double m_elapsedSeconds = 0.0;
	std::chrono::steady_clock::time_point m_lastTime;

	// OpenGL resources
	ffglex::FFGLShader shader;
	GLuint vao, vbo, ibo;
	GLuint textTexture;
	int textureWidth, textureHeight;
	std::string lastRenderedText;
	bool m_fontChanged;

	GLint uColorLocation;
	GLint uBgColorLocation;
	GLint uPositionLocation;
	GLint uSizeLocation;
	GLint uAspectLocation;
	GLint uShowBgLocation;
	GLint uTexAspectLocation;
	GLint uTextureLocation;

	// Text buffer for GetTextParameter
	char m_textBuffer[256];

	void RenderTextToTexture( const std::string& text );
	void HSBtoRGB( float h, float s, float b, float& r, float& g, float& bl );
	std::string GetTimeString();
};
