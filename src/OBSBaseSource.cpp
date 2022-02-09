
#include "OBSBaseSource.h"


namespace OBS {

	static void addSource(void *_data, obs_scene_t *scene)
	{
		BaseSource *bs = static_cast<BaseSource*>(_data);

		bs->_sceneitem = obs_scene_add(scene, bs->_source);
		obs_sceneitem_set_visible(bs->_sceneitem, true);
	}

	BaseSource::BaseSource() 
		:_properties(nullptr),
		_sceneitem(nullptr),
		_source(nullptr)
	{

	}

	BaseSource::BaseSource(int channel, const char* flag)
		:BaseSource()
	{
		_channel = channel;
		_flag = flag;
	}

	BaseSource::~BaseSource()
	{
		if (nullptr != _properties)
		{
			obs_properties_destroy(_properties);
			_properties = nullptr;
		}

		if (nullptr != _sceneitem)
		{
			obs_sceneitem_remove(_sceneitem);
			_sceneitem = nullptr;
		}
	}

	void BaseSource::setVisible(bool visible)
	{
		if (_sceneitem != nullptr)
		{
			obs_sceneitem_set_visible(_sceneitem, visible);
		}
	}

	void BaseSource::create(const char* name, obs_scene_t *scene)
	{
		/* set the scene as the primary draw source and go */
		_name = name;
		_scene = scene;
		_source = obs_source_create(_flag.c_str(), name, NULL, nullptr);

		obs_enter_graphics();
		obs_scene_atomic_update(scene, addSource, this);
		obs_leave_graphics();
		obs_source_release(_source);
	}

	void BaseSource::orderItem(obs_order_movement oom)
	{
		obs_sceneitem_set_order(_sceneitem, oom);
	}

}
