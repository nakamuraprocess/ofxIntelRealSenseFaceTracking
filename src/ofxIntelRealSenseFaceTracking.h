#pragma once

#include "ofMain.h"
#include "RealSense\Face\FaceConfiguration.h"
#include "RealSense\Face\FaceData.h"
#include "RealSense\Face\FaceModule.h"
#include "pxcsensemanager.h"
#include "NV12Decoder.h"

using namespace Intel::RealSense::Face;

class ofxIntelRealSenseFaceTracking: public ofThread {
public:
	struct personalData {
		PXCRectI32 faceRect = { 0 };
		FaceData::LandmarkPoint landmarkPoints[78];
		ofVec3f landmarkPos[78];
		short browRaiserLeft = 0;
		short browRaiserRight = 0;
		short browLowererLeft = 0;
		short browLowererRight = 0;
		short eyesClosedLeft = 0;
		short eyesClosedRight = 0;
		short kiss = 0;
		short smile = 0;
		short mouseOpen = 0;
		short tongueOut = 0;
	};

	enum MODE {
		LIVE = 0,
		PLAYBACK = 1,
		RECORD = 2,
	};

	struct Settings {
		MODE mode = LIVE;
		int colorWidth;
		int colorHeight;
		int colorFps;
		int maxFaces;
		bool landmarksEnabled;

		PXCImage::PixelFormat colorFormat;
		PXCCapture::Device::MirrorMode mirrorMode;
		FaceConfiguration::SmoothingLevelType smoothingLevel;

		map<string, PXCImage::PixelFormat> mapColorFormat;
		map<string, PXCCapture::Device::MirrorMode> mapMirrorMode;
		map<string, FaceConfiguration::SmoothingLevelType> mapSmoothingLevel;
		map<string, bool> mapLandmarksEnabled;

		Settings() {
			mapColorFormat["BGR"] = PXCImage::PixelFormat::PIXEL_FORMAT_BGR;
			mapColorFormat["NV12"] = PXCImage::PixelFormat::PIXEL_FORMAT_NV12;
			mapMirrorMode["false"] = PXCCapture::Device::MirrorMode::MIRROR_MODE_DISABLED;
			mapMirrorMode["true"] = PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL;
			mapSmoothingLevel["DISABLED"] = FaceConfiguration::SmoothingLevelType::SMOOTHING_DISABLED;
			mapSmoothingLevel["MEDIUM"] = FaceConfiguration::SmoothingLevelType::SMOOTHING_MEDIUM;
			mapSmoothingLevel["HIGH"] = FaceConfiguration::SmoothingLevelType::SMOOTHING_HIGH;
			mapLandmarksEnabled["false"] = false;
			mapLandmarksEnabled["true"] = true;
		}
	};

	~ofxIntelRealSenseFaceTracking();
	void setup(string pathConfig);
	void updateColor();
	void drawColor();
	void drawFaceData();
	ofTexture* getColor();
	personalData* getPersonalData();
	ofRectangle* getDrawFixSizeFaceRect();
	int iTrackedMaxFace;
	int MAXFACES;
	int COLOR_WIDTH;
	int COLOR_HEIGHT;
	int COLOR_FPS;
	bool LANDMARKS_ENABLED;

private:
	PXCSenseManager* pSenseManager;
	PXCCaptureManager* pCaptureManager;
	FaceData* faceData;
	PXCCapture::Device* device;
	personalData* persons;
	unsigned char* colorImageBuffer;
	ofTexture colorTexture;
	MODE mode;
	PXCImage::PixelFormat COLOR_FORMAT;
	unsigned char* YBuffer;
	unsigned char* UVBuffer;
	NV12Decoder decoder;
	ofRectangle* drawFixSizeFaceRect;
	Settings settings;
	float fScaleUp = 1.0;
	float fOffsetY = 0.0;
	float fOffsetX = 0.0;
	void updateData();
	void updateDrawFixSizeFaceRect(float scale, float offsetX, float offsetY);
	void threadedFunction();
	wstring Profile2String(PXCCapture::Device::StreamProfile *pinfo);
};