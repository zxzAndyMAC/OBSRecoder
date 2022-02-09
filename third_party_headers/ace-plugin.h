#ifndef _OBS_ACE_PLUGIN_H_
#define _OBS_ACE_PLUGIN_H_

#define EXPORT __declspec(dllexport)

#ifdef __cplusplus
#define MODULE_EXPORT extern "C" EXPORT
#define MODULE_EXTERN extern "C"
#else
#define MODULE_EXPORT EXPORT
#define MODULE_EXTERN extern
#endif

MODULE_EXPORT void ace_copy_frame_data(uint8_t **data, uint32_t *w, uint32_t *h);

#endif // _OBS_ACE_PLUGIN_H_
