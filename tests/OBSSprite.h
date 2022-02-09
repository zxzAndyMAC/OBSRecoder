
#ifndef _OBS_SPRITE_H_
#define _OBS_SPRITE_H_

#include "cocos2d.h"
#include <IOBSStudio.h>

USING_NS_CC;

class OBSSprite : public Sprite
{
public:
	static OBSSprite* create();
	OBSSprite();
	~OBSSprite();

	virtual void draw(Renderer *renderer, const Mat4 &transform, uint32_t flags) override final;
	virtual bool init() override final;

	void setStart(bool s) { start = s; }
	void setdevice();
	void set_cam_res(int index);
	bool openPreview();
	bool closePreview();
	void record(bool re);
private:
	bool first = true;
	bool start = false;
};



#endif

