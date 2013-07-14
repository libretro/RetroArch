#ifndef RETRO_LIB_HHH
#define RETRO_LIB_HHH

#include <dlfcn.h>
#include <exception>
#include <android/log.h>
#include "libretro.h"

#define LOG(...) __android_log_print(ANDROID_LOG_ERROR, "Andretro", __VA_ARGS__) 

class Library
{
    public:
        Library(const char* aPath) :
            handle(openLib(aPath)),
            set_environment((void (*)(retro_environment_t))getFn("retro_set_environment")),
            set_video_refresh((void (*)(retro_video_refresh_t))getFn("retro_set_video_refresh")),
            set_audio_sample((void (*)(retro_audio_sample_t))getFn("retro_set_audio_sample")),
            set_audio_sample_batch((void (*)(retro_audio_sample_batch_t))getFn("retro_set_audio_sample_batch")),
            set_input_poll((void (*)(retro_input_poll_t))getFn("retro_set_input_poll")),
            set_input_state((void (*)(retro_input_state_t))getFn("retro_set_input_state")),
            init((void (*)(void))getFn("retro_init")),
            deinit((void (*)(void))getFn("retro_deinit")),
            api_version((unsigned (*)(void))getFn("retro_api_version")),
            get_system_info((void (*)(struct retro_system_info *info))getFn("retro_get_system_info")),
            get_system_av_info((void (*)(struct retro_system_av_info *info))getFn("retro_get_system_av_info")),
            set_controller_port_device((void (*)(unsigned port, unsigned device))getFn("retro_set_controller_port_device")),
            reset((void (*)(void))getFn("retro_reset")),
            run((void (*)(void))getFn("retro_run")),
            serialize_size((size_t (*)(void))getFn("retro_serialize_size")),
            serialize((bool (*)(void *data, size_t size))getFn("retro_serialize")),
            unserialize((bool (*)(const void *data, size_t size))getFn("retro_unserialize")),
            cheat_reset((void (*)(void))getFn("retro_cheat_reset")),
            cheat_set((void (*)(unsigned index, bool enabled, const char *code))getFn("retro_cheat_set")),
            load_game((bool (*)(const struct retro_game_info *game))getFn("retro_load_game")),
            load_game_special((bool (*)(unsigned game_type,const struct retro_game_info *info, size_t num_info))getFn("retro_load_game_special")),
            unload_game((void (*)(void))getFn("retro_unload_game")),
            get_region((unsigned (*)(void))getFn("retro_get_region")),
            get_memory_data((void *(*)(unsigned id))getFn("retro_get_memory_data")),
            get_memory_size((size_t (*)(unsigned id))getFn("retro_get_memory_size"))
        {
        
        }

        ~Library()
        {
            if(handle)
            {
                dlclose(handle);
            }
        }
            
        void* const handle;

        void (* const set_environment)(retro_environment_t);
        void (* const set_video_refresh)(retro_video_refresh_t);
        void (* const set_audio_sample)(retro_audio_sample_t);
        void (* const set_audio_sample_batch)(retro_audio_sample_batch_t);
        void (* const set_input_poll)(retro_input_poll_t);
        void (* const set_input_state)(retro_input_state_t);
        void (* const init)(void);
        void (* const deinit)(void);
        unsigned (* const api_version)(void);
        void (* const get_system_info)(struct retro_system_info *info);
        void (* const get_system_av_info)(struct retro_system_av_info *info);
        void (* const set_controller_port_device)(unsigned port, unsigned device);
        void (* const reset)(void);
        void (* const run)(void);
        size_t (* const serialize_size)(void);
        bool (* const serialize)(void *data, size_t size);
        bool (* const unserialize)(const void *data, size_t size);        
        void (* const cheat_reset)(void);
        void (* const cheat_set)(unsigned index, bool enabled, const char *code);
        bool (* const load_game)(const struct retro_game_info *game);
        bool (* const load_game_special)(unsigned game_type,const struct retro_game_info *info, size_t num_info);
        void (* const unload_game)(void);
        unsigned (* const get_region)(void);
        void *(* const get_memory_data)(unsigned id);
        size_t (* const get_memory_size)(unsigned id);
        
    private:
        void* openLib(const char* aPath)
        {
            void* result = dlopen(aPath, RTLD_NOW | RTLD_LOCAL);
            
            if(!result)
            {
                LOG("openLib failed: %s\n", dlerror());
                throw std::exception();
            }
            
            return result;
        }
        
        void* getFn(const char* aName)
        {
            void* result = dlsym(handle, aName);
            
            if(!result)
            {
                LOG("getFn failed: %s\n", dlerror());
                throw std::exception();
            }
            
            return result;
        }    
};

#endif