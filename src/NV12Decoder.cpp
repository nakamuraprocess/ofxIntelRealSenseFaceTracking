#include "NV12Decoder.h"

#define STRINGIFY(A) #A

void NV12Decoder::setup(int width, int height){
	this->width = width;
	this->height = height;

	YTex.allocate(width, height, GL_R8);
	UVTex.allocate(width / 2, height / 2, GL_RG8);
	dstFbo.allocate(width, height, GL_RGB);

	if (ofIsGLProgrammableRenderer()) {
		string vert150 = "#version 150\n";
		vert150 += STRINGIFY
		(
		in vec4 position;
		in vec2 texcoord;
		out vec2 texCoordVarying;
		uniform mat4 modelViewProjectionMatrix;

		void main()
		{
			texCoordVarying = texcoord;
			gl_Position = modelViewProjectionMatrix * position;
		}
		);

		string frag150 = "#version 150\n";
		frag150 += STRINGIFY
		(
		in vec2 texCoordVarying;
		out vec4 fragColor;
		uniform sampler2DRect YTex;
		uniform sampler2DRect UVTex;
		void main(void) {
			vec2 texcoordY = texCoordVarying.xy;
			vec2 texcoordUV = floor(texcoordY / vec2(2.0));
			float y = 0.0;
			float u = 0.0;
			float v = 0.0;

			y = texture(YTex, texcoordY).r;
			u = texture(UVTex, texcoordUV).g;
			v = texture(UVTex, texcoordUV).r;

			float c = y - 0.0625;
			float d = u - 0.5;
			float e = v - 0.5;

			float r = 1.164383 * c + 1.596027 * e;
			float g = 1.164383 * c - (0.391762 * d) - (0.812968 * e);
			float b = 1.164383 * c + 2.017232 * d;

			fragColor = vec4(r, g, b, 1.0);
		}
		);

		decoder.setupShaderFromSource(GL_VERTEX_SHADER, vert150);
		decoder.setupShaderFromSource(GL_FRAGMENT_SHADER, frag150);
		decoder.bindDefaults();
		decoder.linkProgram();
	}
	else {
		string frag = "#version 120\n";
		frag += STRINGIFY
		(
		uniform sampler2DRect YTex;
		uniform sampler2DRect UVTex;

		void main(void) {
			vec2 texcoordY = gl_TexCoord[0].xy;
			vec2 texcoordUV = floor(texcoordY / vec2(2.0));
			float y = 0.0;
			float u = 0.0;
			float v = 0.0;

			y = texture2DRect(YTex, texcoordY).r;
			u = texture2DRect(UVTex, texcoordUV).g;
			v = texture2DRect(UVTex, texcoordUV).r;

			float c = y - 0.0625;
			float d = u - 0.5;
			float e = v - 0.5;

			float r = 1.164383 * c + 1.596027 * e;
			float g = 1.164383 * c - (0.391762 * d) - (0.812968 * e);
			float b = 1.164383 * c + 2.017232 * d;

			gl_FragColor = vec4(r, g, b, 1.0);
		}
		);

		decoder.setupShaderFromSource(GL_FRAGMENT_SHADER, frag);
		decoder.linkProgram();
	}
}

void NV12Decoder::update(unsigned char* YBuffer, unsigned char* UVBuffer)
{
	YTex.loadData(YBuffer, width, height, GL_RED);
	UVTex.loadData(UVBuffer, width / 2, height / 2, GL_RG);

	dstFbo.begin();
	ofClear(0);
	decoder.begin();
	decoder.setUniformTexture("YTex", YTex, 0);
	decoder.setUniformTexture("UVTex", UVTex, 1);
	YTex.draw(0, 0);
	decoder.end();
	dstFbo.end();
}

ofTexture * NV12Decoder::getTexture()
{
	return &dstFbo.getTexture();
}
