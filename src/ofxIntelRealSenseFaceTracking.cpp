#include "ofxIntelRealSenseFaceTracking.h"

//--------------------------------------------------------------
ofxIntelRealSenseFaceTracking::~ofxIntelRealSenseFaceTracking() {
	waitForThread();
	stopThread();

	delete[] colorImageBuffer;
	delete[] persons;
	delete[] drawFixSizeFaceRect;

	delete[] YBuffer;
	delete[] UVBuffer;

	pSenseManager->Close();
	pSenseManager->Release();
}

//--------------------------------------------------------------
void ofxIntelRealSenseFaceTracking::setup(string pathConfig){
	ofJson json = ofLoadJson(pathConfig);
	settings.colorWidth = json["color_width"];
	settings.colorHeight = json["color_height"];
	settings.colorFps = json["color_fps"];
	settings.colorFormat = settings.mapColorFormat[json["color_format"]];
	settings.maxFaces = json["max_faces"];
	settings.mirrorMode = settings.mapMirrorMode[json["mirror_mode"]];
	settings.smoothingLevel = settings.mapSmoothingLevel[json["faces_moothing_level"]];
	settings.landmarksEnabled = settings.mapLandmarksEnabled[json["landmarks_enabled"]];

	mode = settings.mode;
	COLOR_WIDTH = settings.colorWidth;
	COLOR_HEIGHT = settings.colorHeight;
	COLOR_FORMAT = settings.colorFormat;
	COLOR_FPS = settings.colorFps;
	MAXFACES = settings.maxFaces;
	LANDMARKS_ENABLED = settings.landmarksEnabled;

	// Create a pipeline construct
	pSenseManager = PXCSenseManager::CreateInstance();
	if (!pSenseManager) {
		wprintf_s(L"Error: PXCSenseManager is unavailable\n");
	}

	pxcStatus result = PXC_STATUS_NO_ERROR;
	// Enable the Color Stream or Playbadk
	if (mode == LIVE) {
		result = pSenseManager->EnableStream(PXCCapture::StreamType::STREAM_TYPE_COLOR, COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS);
		if (result < PXC_STATUS_NO_ERROR) {
			wprintf_s(L"Error: Start Strem failed (%d)\n", result);
		}
	}
	else if (mode == PLAYBACK) {
		pxcCHAR fileName[] = L"data\\test720p.rssdk";
		pCaptureManager = pSenseManager->QueryCaptureManager();
		result = pCaptureManager->SetFileName(fileName, false);
		if (result < PXC_STATUS_NO_ERROR) {
			wprintf_s(L"Error: Start Strem failed (%d)\n", result);
		}
		pSenseManager->QueryCaptureManager()->SetRealtime(true);
	}

	// Enable the Face module
	result = pSenseManager->EnableFace();
	if (result < PXC_STATUS_NO_ERROR) {
		wprintf_s(L"Error: Face failed (%d)\n", result);
	}

	PXCFaceModule* faceModule = pSenseManager->QueryFace();
	if (faceModule == NULL){
		assert(faceModule);
		return;
	}

	FaceConfiguration* config = faceModule->CreateActiveConfiguration();
	if (config == NULL){
		assert(config);
		return;
	}

	config->SetTrackingMode(FaceConfiguration::TrackingModeType::FACE_MODE_COLOR);
	config->strategy = FaceConfiguration::TrackingStrategyType::STRATEGY_CLOSEST_TO_FARTHEST;
	config->detection.isEnabled = true;
	config->detection.maxTrackedFaces = MAXFACES;
	config->landmarks.isEnabled = LANDMARKS_ENABLED;
	config->landmarks.smoothingLevel = settings.smoothingLevel;
	config->landmarks.maxTrackedFaces = MAXFACES;
	config->QueryExpressions()->Enable();
	config->QueryExpressions()->EnableAllExpressions();
	config->QueryExpressions()->properties.maxTrackedFaces = MAXFACES;
	config->ApplyChanges();
	config->Release();

	faceData = faceModule->CreateOutput();

	// Initialize and report the resulting stream source and profile to stdout
	result = pSenseManager->Init();
	if (result < PXC_STATUS_NO_ERROR) {
		if (result == PXC_STATUS_ITEM_UNAVAILABLE) {
			wprintf_s(L"Error: Init failed (%d). Check that your camera supports the selected profile.\n", result);
		}
		else {
			wprintf_s(L"Error: Init failed (%d)\n", result);
		}
	}

	// Get pointer to active device
	device = pSenseManager->QueryCaptureManager()->QueryDevice();
	if (!device) {
		wprintf_s(L"Error: QueryDevice failed on line %d\n", __LINE__);
	}

	// Mirror mode
	device->SetMirrorMode(settings.mirrorMode);

	
	Intel::RealSense::bool32_t desabled = 0;

	// Turn off auto exposure
	device->SetColorAutoExposure(desabled);

	// Turn off auto white balance
	device->SetColorAutoWhiteBalance(desabled);

	// Report the device info
	PXCCapture::DeviceInfo device_info;
	pSenseManager->QueryCaptureManager()->QueryCapture()->QueryDeviceInfo(0, &device_info);
	wprintf_s(L"Streaming from: %s\nDidx: %d\nFirmware: %d.%d.%d.%d\n",
		device_info.name,
		device_info.didx,
		device_info.firmware[0],
		device_info.firmware[1],
		device_info.firmware[2],
		device_info.firmware[3]
	);

	PXCCapture::Device::StreamProfileSet profile = {};
	int numProfiles = device->QueryStreamProfileSetNum(PXCCapture::STREAM_TYPE_COLOR);
	for (int i = 0; i < numProfiles; i++) {
		PXCCapture::Device::StreamProfileSet profiles = {};
		auto sts = device->QueryStreamProfileSet(PXCCapture::STREAM_TYPE_COLOR, i, &profiles);
		wcout << Profile2String(&profiles.color) << endl;
		if (profiles.color.imageInfo.format == COLOR_FORMAT &&
			profiles.color.imageInfo.width == COLOR_WIDTH &&
			profiles.color.imageInfo.height == COLOR_HEIGHT &&
			profiles.color.frameRate.max == COLOR_FPS) {
			profile = profiles; 
		}
		if (sts < PXC_STATUS_NO_ERROR) {
			break;
		}
	}
	device->SetStreamProfileSet(&profile);

	// Report the resulting profile
	PXCCapture::Device::StreamProfileSet active_profile;
	result = device->QueryStreamProfileSet(&active_profile);
	if (result < PXC_STATUS_NO_ERROR) {
		cout << "ACTIVE PROFILE ERROR" << endl;
	}
	else {
		// Report the profiles to stdout
		wprintf_s(L"Color: %dx%dx%0.f \nDepth: %dx%dx%0.f \n", 
			active_profile.color.imageInfo.width, 
			active_profile.color.imageInfo.height, 
			active_profile.color.frameRate.max, 
			active_profile.depth.imageInfo.width, 
			active_profile.depth.imageInfo.height, 
			active_profile.depth.frameRate.max
		);
	}

	if (COLOR_FORMAT == PXCImage::PIXEL_FORMAT_NV12) {
		YBuffer = new unsigned char[COLOR_WIDTH * COLOR_HEIGHT];
		UVBuffer = new unsigned char[COLOR_WIDTH * COLOR_HEIGHT / 2];
		for (int i = 0; i < COLOR_WIDTH * COLOR_HEIGHT; ++i){
			YBuffer[i] = 0;
		}
		for (int i = 0; i < COLOR_WIDTH * COLOR_HEIGHT / 2; ++i){
			UVBuffer[i] = 0;
		}
		decoder.setup(COLOR_WIDTH, COLOR_HEIGHT);
	}
	else {
		colorImageBuffer = new unsigned char[COLOR_WIDTH * COLOR_HEIGHT * 3 * sizeof(pxcBYTE)];
		colorTexture.allocate(COLOR_WIDTH, COLOR_HEIGHT, GL_RGB);
	}

	persons = new personalData[MAXFACES];
	drawFixSizeFaceRect = new ofRectangle[MAXFACES];

	startThread();
}

ofTexture* ofxIntelRealSenseFaceTracking::getColor(){
	if (COLOR_FORMAT == PXCImage::PIXEL_FORMAT_NV12) {
		return decoder.getTexture();
	}
	else {
		return &colorTexture;
	}
}

//--------------------------------------------------------------
ofxIntelRealSenseFaceTracking::personalData* ofxIntelRealSenseFaceTracking::getPersonalData(){
	return persons;
}

ofRectangle* ofxIntelRealSenseFaceTracking::getDrawFixSizeFaceRect() {
	return drawFixSizeFaceRect;
}

//--------------------------------------------------------------
void ofxIntelRealSenseFaceTracking::updateData() {
	//  Get image from the video module

	if (mode == PLAYBACK) {
		if (pSenseManager->QueryCaptureManager()->QueryFrameIndex() >= pSenseManager->QueryCaptureManager()->QueryNumberOfFrames() -30) {
			pSenseManager->QueryCaptureManager()->SetFrameByIndex(0);
		}
	}

	pxcStatus result = pSenseManager->AcquireFrame(true);
	if (result < PXC_STATUS_NO_ERROR) {
		wprintf_s(L"Error: AcquireFrame failed (%d)\n", result);
	}

	const PXCCapture::Sample *sample = pSenseManager->QuerySample();
	if (sample) {
		if (sample->color == 0) {
			wprintf_s(L"Error: QuerySample failed (%d)\n", result);
			return;
		}

		PXCImage::ImageInfo info = sample->color->QueryInfo();
		PXCImage::ImageData imageData;
		pxcStatus result = sample->color->AcquireAccess(PXCImage::Access::ACCESS_READ, COLOR_FORMAT, PXCImage::Rotation::ROTATION_0_DEGREE, PXCImage::Option::OPTION_ANY, &imageData);
		if (result < PXC_STATUS_NO_ERROR) {
			wprintf_s(L"Error: The image was not returned\n");
		}

		if (COLOR_FORMAT == PXCImage::PIXEL_FORMAT_NV12) {
			memcpy(YBuffer, imageData.planes[0], imageData.pitches[0] * (int)info.height);
			memcpy(UVBuffer, imageData.planes[1], imageData.pitches[1] * (int)info.height / 2);
		}
		else {
			memcpy(colorImageBuffer, (unsigned char *)imageData.planes[0], (int)imageData.pitches[0] * (int)info.height * sizeof(pxcBYTE));
		}

		sample->color->ReleaseAccess(&imageData);
	}

	faceData->Update();
	iTrackedMaxFace = faceData->QueryNumberOfDetectedFaces();

	for (int i = 0; i < MAXFACES; ++i) {
		FaceData::Face* trackedFace = faceData->QueryFaceByIndex(i);
		if (trackedFace == 0) {
			persons[i].faceRect = { 0 };
			for (int j = 0; j < _countof(persons[i].landmarkPoints); ++j) {
				//FaceData::LandmarkPoint initLandmarkPoints;
				persons[i].landmarkPoints[j] = FaceData::LandmarkPoint();
				persons[i].landmarkPos[j].set(0,0,0);
			}

			persons[i].browRaiserLeft = 0;
			persons[i].browRaiserRight = 0;
			persons[i].browLowererLeft = 0;
			persons[i].browLowererRight = 0;
			persons[i].eyesClosedLeft = 0;
			persons[i].eyesClosedRight = 0;
			persons[i].kiss = 0;
			persons[i].smile = 0;
			persons[i].mouseOpen = 0;
			persons[i].tongueOut = 0;
			continue;
		}

		FaceData::DetectionData* detectionData = trackedFace->QueryDetection();
		if (detectionData != NULL) {
			detectionData->QueryBoundingRect(&persons[i].faceRect);
		}

		FaceData::LandmarksData* landmarkData = trackedFace->QueryLandmarks();
		if (landmarkData != NULL) {
			pxcI32 numPoints = landmarkData->QueryNumPoints();
			FaceData::LandmarkPoint* tmpLandmarkPoints = new FaceData::LandmarkPoint[numPoints];
			if (landmarkData->QueryPoints(tmpLandmarkPoints)) {
				for (int j = 0; j < numPoints; ++j) {
					persons[i].landmarkPoints[j] = tmpLandmarkPoints[j];
					if (persons[i].landmarkPoints[j].confidenceImage) {
						persons[i].landmarkPos[j].set(persons[i].landmarkPoints[j].image.x, persons[i].landmarkPoints[j].image.y, 0);
					}
					else {
						persons[i].landmarkPos[j].set(0,0,0);
					}
				}
			}
			delete tmpLandmarkPoints;
		}


		FaceData::ExpressionsData* expressionData = trackedFace->QueryExpressions();
		FaceData::ExpressionsData::FaceExpressionResult expressionResult;
		if (expressionData != NULL) {
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_BROW_RAISER_LEFT, &expressionResult)) {
				persons[i].browRaiserLeft = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_BROW_RAISER_RIGHT, &expressionResult)) {
				persons[i].browRaiserRight = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_BROW_LOWERER_LEFT, &expressionResult)) {
				persons[i].browLowererLeft = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_BROW_LOWERER_RIGHT, &expressionResult)) {
				persons[i].browLowererRight = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_EYES_CLOSED_LEFT, &expressionResult)) {
				persons[i].eyesClosedLeft = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_EYES_CLOSED_RIGHT, &expressionResult)) {
				persons[i].eyesClosedRight = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_KISS, &expressionResult)) {
				persons[i].kiss = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_SMILE, &expressionResult)) {
				persons[i].smile = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_MOUTH_OPEN, &expressionResult)) {
				persons[i].mouseOpen = expressionResult.intensity;
			}
			if (expressionData->QueryExpression(FaceData::ExpressionsData::EXPRESSION_TONGUE_OUT, &expressionResult)) {
				persons[i].tongueOut = expressionResult.intensity;
			}
		}
	}

	pSenseManager->ReleaseFrame();
}

//--------------------------------------------------------------
void ofxIntelRealSenseFaceTracking::updateColor() {
	if (COLOR_FORMAT == PXCImage::PIXEL_FORMAT_NV12) {
		decoder.update(YBuffer, UVBuffer);
	}
	else{
		colorTexture.loadData(colorImageBuffer, COLOR_WIDTH, COLOR_HEIGHT, GL_BGR);
	}
	updateDrawFixSizeFaceRect(fScaleUp, fOffsetX, fOffsetY);
}

//--------------------------------------------------------------
void ofxIntelRealSenseFaceTracking::drawColor() {
	ofPushMatrix();
	ofTranslate(-fOffsetX, fOffsetY);
	ofScale(fScaleUp, fScaleUp);
	if (COLOR_FORMAT == PXCImage::PIXEL_FORMAT_NV12) {
		decoder.getTexture()->draw(0, 0);
	}
	else {
		colorTexture.draw(0, 0);
	}
	ofPopMatrix();
}

//--------------------------------------------------------------
void ofxIntelRealSenseFaceTracking::updateDrawFixSizeFaceRect(float fScale, float fOffsetX, float fOffsetY) {
	for (int i = 0; i < MAXFACES; ++i) {
		drawFixSizeFaceRect[i].set(float(persons[i].faceRect.x * fScale) - fOffsetX, float(persons[i].faceRect.y * fScale) + fOffsetY, float(persons[i].faceRect.w *fScale), float(persons[i].faceRect.h * fScale));
	}
}

//--------------------------------------------------------------
void ofxIntelRealSenseFaceTracking::drawFaceData() {
	ofPushStyle();
	ofNoFill();
	for (int i = 0; i < MAXFACES; ++i) {
		ofSetColor(255);

		if (drawFixSizeFaceRect[i].isZero()) {
			continue;
		}

		if (LANDMARKS_ENABLED) {
			for (int j = 0; j < _countof(persons[i].landmarkPoints); ++j) {
				if (persons[i].landmarkPoints[j].confidenceImage) {
					ofSetColor(255);
				}
				else {
					ofSetColor(255, 0, 0);
				}
				ofCircle(persons[i].landmarkPoints[j].image.x, persons[i].landmarkPoints[j].image.y, 1);
			}
		}

		ofRect(drawFixSizeFaceRect[i]);

		ofSetColor(0, 255, 0);
		ofDrawBitmapString("BROW RAISER LEFT:" + ofToString(persons[i].browRaiserLeft), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 20);
		ofDrawBitmapString("BROW RAISER RIGHT:" + ofToString(persons[i].browRaiserRight), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 40);
		ofDrawBitmapString("BROW LOWERER LEFT:" + ofToString(persons[i].browLowererLeft), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 60);
		ofDrawBitmapString("BROW LOWERER RIGHT:" + ofToString(persons[i].browLowererRight), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 80);
		ofDrawBitmapString("EYES CLOSED LEFT:" + ofToString(persons[i].eyesClosedLeft), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 100);
		ofDrawBitmapString("EYES CLOSED RIGHT:" + ofToString(persons[i].eyesClosedRight), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 120);
		ofDrawBitmapString("KISS:" + ofToString(persons[i].kiss), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 140);
		ofDrawBitmapString("SMILE:" + ofToString(persons[i].smile), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 160);
		ofDrawBitmapString("MOUSE OPEN:" + ofToString(persons[i].mouseOpen), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 180);
		ofDrawBitmapString("TONGUE OUT:" + ofToString(persons[i].tongueOut), drawFixSizeFaceRect[i].getBottomLeft().x, drawFixSizeFaceRect[i].getBottomLeft().y + 200);
	}
	ofPopStyle();
}

//--------------------------------------------------------------
void ofxIntelRealSenseFaceTracking::threadedFunction() {
	while (isThreadRunning()) {
		lock();
		updateData();
		unlock();
	}
}

//--------------------------------------------------------------
wstring ofxIntelRealSenseFaceTracking::Profile2String(PXCCapture::Device::StreamProfile *pinfo) {
	pxcCHAR line[256] = L"";
	if (pinfo->frameRate.min && pinfo->frameRate.max && pinfo->frameRate.min != pinfo->frameRate.max) {
		swprintf_s<sizeof(line) / sizeof(pxcCHAR)>(line, L"%s %dx%dx%d-%d %s",
			PXCImage::PixelFormatToString(pinfo->imageInfo.format),
			pinfo->imageInfo.width,
			pinfo->imageInfo.height,
			(int)pinfo->frameRate.min,
			(int)pinfo->frameRate.max,
			pinfo->options & PXCCapture::Device::StreamOption::STREAM_OPTION_UNRECTIFIED ? L"RAW" : L"");
	}
	else {
		pxcF32 frameRate = pinfo->frameRate.min ? pinfo->frameRate.min : pinfo->frameRate.max;
		swprintf_s<sizeof(line) / sizeof(pxcCHAR)>(line, L"%s %dx%dx%d %s",
			PXCImage::PixelFormatToString(pinfo->imageInfo.format),
			pinfo->imageInfo.width,
			pinfo->imageInfo.height,
			(int)frameRate,
			pinfo->options & PXCCapture::Device::StreamOption::STREAM_OPTION_UNRECTIFIED ? L"RAW" : L"");
	}
	return wstring(line);
}