#ifndef _OBS_BASE_SOURCE_H_
#define _OBS_BASE_SOURCE_H_

#include "OBSCommon.h"
#include <string>

using namespace std;


namespace OBS {
	class BaseSource
	{
	public:
		BaseSource();
		BaseSource(int channel, const char* flag);
		virtual ~BaseSource();

		virtual void	create(const char* name, obs_scene_t *scene);
		virtual void	setVisible(bool visible);
		virtual void    orderItem(obs_order_movement oom);
	public:
		int						_channel;
		string					_flag;
		string					_name;
		obs_scene_t				*_scene;
		obs_source_t			*_source;
		obs_properties_t		*_properties;
		obs_sceneitem_t			*_sceneitem;
	};
}

#endif
