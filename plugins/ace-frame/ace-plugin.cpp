#include <obs-module.h>

#define EXPORT __declspec(dllexport)

#ifdef __cplusplus
#define MODULE_EXPORT extern "C" EXPORT
#define MODULE_EXTERN extern "C"
#else
#define MODULE_EXPORT EXPORT
#define MODULE_EXTERN extern
#endif

#include "ace-frame.h"

static OBSACE::OBSAceFrame* g_aceframe;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("ace-frame", "en-US")

MODULE_EXPORT bool obs_module_load(void) {
	g_aceframe = new OBSACE::OBSAceFrame();
	return true;
}


MODULE_EXPORT void obs_module_unload(void) {
	delete g_aceframe;
}

MODULE_EXPORT const char* obs_module_name() {
	static const char pluginName[] = "ACE Frame";
	return pluginName;
}

MODULE_EXPORT const char* obs_module_description() {
	static const char pluginDescription[] =
		"This plugin I dont know how to say~~~~~~.";
	return pluginDescription;
}