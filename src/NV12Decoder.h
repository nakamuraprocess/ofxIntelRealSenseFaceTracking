#pragma once

#include "ofMain.h"

class NV12Decoder {
public:
	void setup(int width, int height);
	void update(unsigned char* YBuffer, unsigned char* UVBuffer);
	ofTexture * getTexture();
private:
	int width, height;
	ofTexture YTex, UVTex;
	ofShader decoder;
	ofFbo dstFbo;
};