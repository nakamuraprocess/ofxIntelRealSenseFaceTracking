meta:
	ADDON_NAME = ofxIntelRealSenseFaceTracking
	ADDON_DESCRIPTION = OF wrapper for Intel RealSense
	ADDON_AUTHOR = TakayukiNakamura
	ADDON_TAGS = "RealSense"
	ADDON_URL = 

vs:
	ADDON_INCLUDES = $(RSSDK_DIR)include
	ADDON_INCLUDES += ../../../addons/ofxIntelRealSenseFaceTracking/src
	ADDON_LIBS = $(RSSDK_DIR)lib/$(PlatformName)/libpxc_d.lib