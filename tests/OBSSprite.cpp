
#include "OBSSprite.h"
#include <stdio.h>
#include <iostream>

using namespace std;
using namespace OBS;

OBSSprite* OBSSprite::create()
{
	OBSSprite *pVlcSprite = new OBSSprite();
	if (pVlcSprite && pVlcSprite->init())
	{
		pVlcSprite->autorelease();
		return pVlcSprite;
	}
	CC_SAFE_DELETE(pVlcSprite);
	return NULL;
}

OBSSprite::OBSSprite()
{

}

OBSSprite::~OBSSprite()
{
	OBSStudio::release();
}

bool OBSSprite::init()
{
	if (!Sprite::init())
	{
		return false;
	}

	IOBS *obs = OBSStudio::createInstance();
	const Cameras cams = obs->getCameras();
	for (auto cam : cams)
	{
		cout << "Cameras name:" << cam.first << "  id:" << cam.second << endl;
	}
	//const AuxAudios auxs = obs->getAuxAudios();

	//for (auto aux : auxs)
	//{
	//	cout << "AuxAudios id:" << aux.first << "  name:" << aux.second << endl;
	//}

	//cout << " " << endl;
	//cout << "========================================" << endl;
	//cout << " " << endl;

	//const AuxAudios dass = obs->getDesktopAudios();
	//for (auto das : dass)
	//{
	//	cout << "DesktopAudios id:" << das.first << "  name:" << das.second << endl;
	//}

	//cout << " " << endl;
	//cout << "========================================" << endl;
	//cout << " " << endl;

	//const Cameras cams = obs->getCameras();
	//for (auto cam : cams)
	//{
	//	cout << "Cameras name:" << cam.first << "  id:" << cam.second << endl;
	//}

	//obs->set_cam_device(cams[0].second);

	//cout << " " << endl;
	//cout << "========================================" << endl;
	//cout << " " << endl;

	//const CameraResolutionLists crls = obs->getCameraResolutionLists();
	//for (auto crl : crls)
	//{
	//	cout << "CameraResolution id:" << crl.first << "  name:" << crl.second << endl;
	//}

	//obs->set_cam_resolution(crls[1].first);

	//cout << " " << endl;
	//cout << "========================================" << endl;
	//cout << " " << endl;

	//const OutputResulutions opls = obs->getOutputResolutions();
	//for (auto fps : opls)
	//{
	//	cout << "OutputResulutions name:" << fps << endl;
	//}

	//cout << " " << endl;
	//cout << "========================================" << endl;
	//cout << " " << endl;

	//const CamerasFPS fpss = obs->getFPSlists();
	//for (auto fps : fpss)
	//{
	//	cout << "CamerasFPS Frame_Interval:" << fps.first << "  name:" << fps.second << endl;
	//}
	//cout << " " << endl;
	//cout << "======== looks everything is ok=============" << endl;
	return true;
}

void OBSSprite::setdevice() 
{
	IOBS *obs = OBSStudio::getInstance();
	const Cameras cams = obs->getCameras();
	obs->set_cam_device(cams[1].second);
}

void OBSSprite::set_cam_res(int index)
{
	IOBS *obs = OBSStudio::getInstance();
	const CameraResolutionLists crls = obs->getCameraResolutionLists();
	obs->set_cam_resolution(crls[index].first);
}

bool OBSSprite::openPreview()
{
	if (OBSStudio::getInstance()->openPreview())
	{
		//OBSStudio::getInstance()->setBackgroundColorVisible(true);
		//OBSStudio::getInstance()->setBackgroundColor(0xFF00FF00);
		//OBSStudio::getInstance()->setChromaEnable(true);
		return true;
	}
	cout << "======== openpreview failed=============" << endl;
	return false;
}

bool OBSSprite::closePreview()
{
	OBSStudio::getInstance()->closePreview();
	return true;
}

void OBSSprite::record(bool re)
{
	if (re)
	{
		OBSStudio::getInstance()->setVideoSafeDir("E:/ace-Â¼Ïñ");
		OBSStudio::getInstance()->startRecording("cocos");
	}
	else
		OBSStudio::getInstance()->stopRecording();
	
}

void OBSSprite::draw(Renderer *renderer, const Mat4 &transform, uint32_t flags)
{
	if (start)
	{
		PixelInfo *pi = OBSStudio::getInstance()->getRawData();
		if (pi && pi->rgba && pi->width >0 && pi->height >0)
		{
			if (first)
			{
				first = false;
				CCTexture2D *tex = new CCTexture2D;
				CCImage* img = new CCImage;
				img->initWithRawData(pi->rgba, pi->width * pi->height * 4, pi->width, pi->height, 8);
				tex->initWithImage(img);
				initWithTexture(tex);
				tex->autorelease();
				CC_SAFE_DELETE(img);
			}
			else
				_texture->updateWithData(pi->rgba, 0, 0, pi->width, pi->height);
		}
	}
	Sprite::draw(renderer, transform, flags);
}