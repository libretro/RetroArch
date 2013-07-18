#include <jni.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include <memory>

#include "Common.h"
#include "../../../libretro.h"
#include "Library.h"

#include "Rewinder.h"

namespace
{
    Library* module;
    
    FileReader ROM;
    Rewinder rewinder;
    
    bool dontProcess;

    char* systemDirectory;

    JNIEnv* env;

    jobject videoFrame;
    jint rotation;

    retro_system_info systemInfo;
    retro_system_av_info avInfo;

    std::auto_ptr<JavaClass> avInfo_class;
    std::auto_ptr<JavaClass> systemInfo_class;
    std::auto_ptr<JavaClass> frame_class;
}

namespace VIDEO
{
	typedef void (*andretro_video_refresh)(const void* data, unsigned width, unsigned height, size_t pitch);
	unsigned pixelFormat;

    template<typename T, int FORMAT, int TYPE>
    static void refresh_noconv(const void *data, unsigned width, unsigned height, size_t pitch)
    {
        const unsigned pixelPitch = pitch / sizeof(T);

        if(pixelPitch == width)
        {
        	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, FORMAT, TYPE, data);
        }
        else
        {
        	T outPixels[width * height];
        	T* outPixels_t = outPixels;
            const T* inPixels = (const T*)data;

        	for(int i = 0; i != height; i ++, inPixels += pixelPitch, outPixels_t += width)
        	{
        		memcpy(outPixels_t, inPixels, width * sizeof(T));
        	}

        	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, FORMAT, TYPE, outPixels);
        }
    }

    // retro_video_refresh for 0RGB1555: deprecated
    static void refresh_15(const void *data, unsigned width, unsigned height, size_t pitch)
    {
    	uint16_t outPixels[width * height];
    	uint16_t* outPixels_t = outPixels;
        const uint16_t* inPixels = (const uint16_t*)data;
        const unsigned pixelPitch = pitch / 2;

    	for(int i = 0; i != height; i ++, inPixels += pixelPitch - width)
    	{
    		for(int j = 0; j != width; j ++)
    		{
    			(*outPixels_t++) = (*inPixels++) << 1;
    		}
    	}

    	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, outPixels);
    }

    static void retro_video_refresh_imp(const void *data, unsigned width, unsigned height, size_t pitch)
    {
    	if(!dontProcess)
    	{
			static const andretro_video_refresh refreshByMode[3] = {&refresh_15, &refresh_noconv<uint32_t, GL_RGBA, GL_UNSIGNED_BYTE>, &refresh_noconv<uint16_t, GL_RGB, GL_UNSIGNED_SHORT_5_6_5>};

			if(data)
			{
				refreshByMode[pixelFormat](data, width, height, pitch);
			}

			env->SetIntField(videoFrame, (*frame_class)["width"], width);
			env->SetIntField(videoFrame, (*frame_class)["height"], height);
			env->SetIntField(videoFrame, (*frame_class)["pixelFormat"], VIDEO::pixelFormat);
			env->SetIntField(videoFrame, (*frame_class)["rotation"], rotation);
			env->SetFloatField(videoFrame, (*frame_class)["aspect"], avInfo.geometry.aspect_ratio);
    	}
    }

    static void createTexture()
    {
    	const GLenum formats[3] = {GL_RGBA, GL_RGBA, GL_RGB};
    	const GLenum types[3] = {GL_UNSIGNED_SHORT_5_5_5_1, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT_5_6_5};

    	if(pixelFormat < 3)
    	{
    		glTexImage2D(GL_TEXTURE_2D, 0, formats[pixelFormat], 1024, 1024, 0, formats[pixelFormat], types[pixelFormat], 0);
    	}
    }
}

namespace INPUT
{
	jint keyboard[RETROK_LAST];
	jint joypads[8];

	int16_t touchData[3];

	static void retro_input_poll_imp(void)
	{
		// Joystick
		jintArray js = (jintArray)env->GetObjectField(videoFrame, (*frame_class)["buttons"]);
		env->GetIntArrayRegion(js, 0, 8, joypads);

		// Keyboard
		jintArray kbd = (jintArray)env->GetObjectField(videoFrame, (*frame_class)["keyboard"]);
		env->GetIntArrayRegion(kbd, 0, RETROK_LAST, keyboard);

		// Pointer
		touchData[0] = env->GetShortField(videoFrame, (*frame_class)["touchX"]);
		touchData[1] = env->GetShortField(videoFrame, (*frame_class)["touchY"]);
		touchData[2] = env->GetShortField(videoFrame, (*frame_class)["touched"]) ? 1 : 0;
	}

	static int16_t retro_input_state_imp(unsigned port, unsigned device, unsigned index, unsigned id)
	{
		switch(device)
		{
			case RETRO_DEVICE_JOYPAD:    return (joypads[port] >> id) & 1;
			case RETRO_DEVICE_KEYBOARD:  return (id < RETROK_LAST) ? keyboard[id] : 0;
			case RETRO_DEVICE_POINTER:   return (id < 3) ? touchData[id] : 0;
		}

		return 0;
	}
}

namespace AUDIO
{
	static uint32_t audioLength;
	static int16_t buffer[48000];

	static inline void prepareFrame()
	{
		audioLength = 0;
	}

	static inline void endFrame()
	{
		if(!dontProcess)
		{
			jshortArray audioData = (jshortArray)env->GetObjectField(videoFrame, (*frame_class)["audio"]);
			env->SetShortArrayRegion(audioData, 0, audioLength, buffer);

			env->SetIntField(videoFrame, (*frame_class)["audioSamples"], audioLength);
		}
	}

	static void retro_audio_sample_imp(int16_t left, int16_t right)
	{
		if(!dontProcess)
		{
			buffer[audioLength++] = left;
			buffer[audioLength++] = right;
		}
	}

	static size_t retro_audio_sample_batch_imp(const int16_t *data, size_t frames)
	{
		if(!dontProcess)
		{
			memcpy(&buffer[audioLength], data, frames * 4);
			audioLength += frames * 2;
		}
		return frames;
	}
}

// Callbacks
//
// Environment callback. Gives implementations a way of performing uncommon tasks. Extensible.
static bool retro_environment_imp(unsigned cmd, void *data)
{
	if(RETRO_ENVIRONMENT_SET_ROTATION == cmd)
	{
		rotation = (*(unsigned*)data) & 3;
		return true;
	}
	else if(RETRO_ENVIRONMENT_GET_OVERSCAN == cmd)
	{
		// TODO: true causes snes9x-next to shit a brick
		*(uint8_t*)data = false;
		return true;
	}
	else if(RETRO_ENVIRONMENT_GET_CAN_DUPE == cmd)
	{
		*(uint8_t*)data = true;
		return true;
	}
	else if(RETRO_ENVIRONMENT_GET_VARIABLE == cmd)
	{
		// TODO
		return false;
	}
	else if(RETRO_ENVIRONMENT_SET_VARIABLES == cmd)
	{
		// HACK
		return false;
	}
	else if(RETRO_ENVIRONMENT_SET_MESSAGE == cmd)
	{
		// TODO
		return false;
	}
	else if(RETRO_ENVIRONMENT_SHUTDOWN == cmd)
	{
		// TODO
		return false;
	}
	else if(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL == cmd)
	{
		// TODO
		return false;
	}
	else if(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY == cmd)
	{
		*(const char**)data = systemDirectory;
		return true;
	}
	else if(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT == cmd)
	{
		unsigned newFormat = *(unsigned*)data;

		if(newFormat < 3 && newFormat != 1)
		{
			VIDEO::pixelFormat = newFormat;
			VIDEO::createTexture();
			return true;
		}

		return false;
	}

	return false;
}

//
#define JNIFUNC(RET, FUNCTION) extern "C" RET Java_org_libretro_LibRetro_ ## FUNCTION
#define JNIARGS JNIEnv* aEnv, jclass aClass

JNIFUNC(jboolean, loadLibrary)(JNIARGS, jstring path, jstring systemDir)
{
    JavaChars libname(aEnv, path);
    
    delete module;
    module = 0;
    
    try
    {
        module = new Library(libname);
        systemDirectory = strdup(JavaChars(aEnv, systemDir));
        chdir(systemDirectory);
        return true;
    }
    catch(...)
    {
        return false;
    }
}

JNIFUNC(void, unloadLibrary)(JNIARGS)
{
    delete module;
    module = 0;

    free(systemDirectory);
    systemDirectory = 0;

    memset(&systemInfo, 0, sizeof(systemInfo));

    rotation = 0;
}

JNIFUNC(void, init)(JNIARGS)
{
    module->set_environment(retro_environment_imp);

    VIDEO::pixelFormat = 0;
	VIDEO::createTexture();

    module->init();

    module->get_system_info(&systemInfo);
}

JNIFUNC(void, deinit)(JNIARGS)
{
    module->deinit();
}

JNIFUNC(jint, apiVersion)(JNIARGS)
{
    return module->api_version();
}

JNIFUNC(void, getSystemInfo)(JNIARGS, jobject aSystemInfo)
{
    aEnv->SetObjectField(aSystemInfo, (*systemInfo_class)["libraryName"], JavaString(aEnv, systemInfo.library_name));
    aEnv->SetObjectField(aSystemInfo, (*systemInfo_class)["libraryVersion"], JavaString(aEnv, systemInfo.library_version));
    aEnv->SetObjectField(aSystemInfo, (*systemInfo_class)["validExtensions"], JavaString(aEnv, systemInfo.valid_extensions));
    aEnv->SetBooleanField(aSystemInfo, (*systemInfo_class)["needFullPath"], systemInfo.need_fullpath);
    aEnv->SetBooleanField(aSystemInfo, (*systemInfo_class)["blockExtract"], systemInfo.block_extract);
}

JNIFUNC(void, getSystemAVInfo)(JNIARGS, jobject aAVInfo)
{
    aEnv->SetIntField(aAVInfo, (*avInfo_class)["baseWidth"], avInfo.geometry.base_width);
    aEnv->SetIntField(aAVInfo, (*avInfo_class)["baseHeight"], avInfo.geometry.base_height);
    aEnv->SetIntField(aAVInfo, (*avInfo_class)["maxWidth"], avInfo.geometry.max_width);
    aEnv->SetIntField(aAVInfo, (*avInfo_class)["maxHeight"], avInfo.geometry.max_height);
    aEnv->SetFloatField(aAVInfo, (*avInfo_class)["aspectRatio"], avInfo.geometry.aspect_ratio);
    aEnv->SetDoubleField(aAVInfo, (*avInfo_class)["fps"], avInfo.timing.fps);
    aEnv->SetDoubleField(aAVInfo, (*avInfo_class)["sampleRate"], avInfo.timing.sample_rate);
}

JNIFUNC(void, setControllerPortDevice)(JNIARGS, jint port, jint device)
{
    module->set_controller_port_device(port, device);
}

JNIFUNC(void, reset)(JNIARGS)
{
    module->reset();
}

JNIFUNC(void, run)(JNIARGS, jobject aVideo)
{
    // TODO
    env = aEnv;
    videoFrame = aVideo;
    
    if(env->GetBooleanField(videoFrame, (*frame_class)["restarted"]))
    {
    	VIDEO::createTexture();
    	env->SetBooleanField(videoFrame, (*frame_class)["restarted"], false);
    }

    const int count = env->GetIntField(videoFrame, (*frame_class)["framesToRun"]);
    const bool rewind = env->GetBooleanField(videoFrame, (*frame_class)["rewind"]);

    for(int i = 0; i < count; i ++)
    {
    	dontProcess = !(i == (count - 1));

        const bool rewound = rewind && rewinder.eatFrame(module);

        if(!rewind || rewound)
        {
            AUDIO::prepareFrame();
        	module->run();
        	AUDIO::endFrame();

        	if(!rewind)
        	{
        		rewinder.stashFrame(module);
        	}
        }
    }
}

JNIFUNC(jint, serializeSize)(JNIARGS)
{
    return module->serialize_size();
}

JNIFUNC(void, cheatReset)(JNIARGS)
{
    module->cheat_reset();
}

JNIFUNC(void, cheatSet)(JNIARGS, jint index, jboolean enabled, jstring code)
{
    JavaChars codeN(aEnv, code);
    module->cheat_set(index, enabled, codeN);
}

JNIFUNC(bool, loadGame)(JNIARGS, jstring path)
{
    JavaChars fileName(aEnv, path);
    
    retro_game_info info = {fileName, 0, 0, 0};
    
    if(!systemInfo.need_fullpath)
    {
        if(ROM.load(fileName))
        {
            info.data = ROM.base();
            info.size = ROM.size();
        }
        else
        {
            return false;
        }
    }

    if(module->load_game(&info))
    {
        module->get_system_av_info(&avInfo);

        module->set_video_refresh(VIDEO::retro_video_refresh_imp);
        module->set_audio_sample(AUDIO::retro_audio_sample_imp);
        module->set_audio_sample_batch(AUDIO::retro_audio_sample_batch_imp);
        module->set_input_poll(INPUT::retro_input_poll_imp);
        module->set_input_state(INPUT::retro_input_state_imp);

        rewinder.gameLoaded(module);
        return true;
    }
    
    return false;
}

JNIFUNC(void, unloadGame)(JNIARGS)
{
    module->unload_game();
    memset(&avInfo, 0, sizeof(avInfo));
    
    ROM.close();
}

JNIFUNC(jint, getRegion)(JNIARGS)
{
    return module->get_region();
}

JNIFUNC(jobject, getMemoryData)(JNIARGS, int aID)
{
    void* const memoryData = module->get_memory_data(aID);
    const size_t memorySize = module->get_memory_size(aID);
    
    return (memoryData && memorySize) ? aEnv->NewDirectByteBuffer(memoryData, memorySize) : 0;
}

JNIFUNC(int, getMemorySize)(JNIARGS, int aID)
{
    return module->get_memory_size(aID);
}

// Extensions: Rewinder
JNIFUNC(void, setupRewinder)(JNIARGS, int aSize)
{
	rewinder.setSize(aSize);
}

// Extensions: Read or write a memory region into a specified file.
JNIFUNC(jboolean, writeMemoryRegion)(JNIARGS, int aID, jstring aFileName)
{
    const size_t size = module->get_memory_size(aID);
    void* const data = module->get_memory_data(aID);
    
    if(size && data)
    {
        DumpFile(JavaChars(aEnv, aFileName), data, size);
    }
    
    return true;
}

JNIFUNC(jboolean, readMemoryRegion)(JNIARGS, int aID, jstring aFileName)
{
    const size_t size = module->get_memory_size(aID);
    void* const data = module->get_memory_data(aID);
    
    if(size && data)
    {
        ReadFile(JavaChars(aEnv, aFileName), data, size);
    }
    
    return true;
}

// Extensions: Serialize/Unserialize using a file
JNIFUNC(jboolean, serializeToFile)(JNIARGS, jstring aPath)
{
    const size_t size = module->serialize_size();

    if(size > 0)
    {        
        uint8_t buffer[size];
        if(module->serialize(buffer, size))
        {
            return DumpFile(JavaChars(aEnv, aPath), buffer, size);
        }
    }
    
    return false;
}

JNIFUNC(jboolean, unserializeFromFile)(JNIARGS, jstring aPath)
{
    const size_t size = module->serialize_size();
    
    if(size > 0)
    {
        uint8_t buffer[size];
        
        if(ReadFile(JavaChars(aEnv, aPath), buffer, size))
        {
            return module->unserialize(buffer, size);
        }
    }

    return false;
}

// Preload native class data
JNIFUNC(jboolean, nativeInit)(JNIARGS)
{
    try
    {
        {
            static const char* const n[] = {"libraryName", "libraryVersion", "validExtensions", "needFullPath", "blockExtract"};
            static const char* const s[] = {"Ljava/lang/String;", "Ljava/lang/String;", "Ljava/lang/String;", "Z", "Z"};
            systemInfo_class.reset(new JavaClass(aEnv, aEnv->FindClass("org/libretro/LibRetro$SystemInfo"), sizeof(n) / sizeof(n[0]), n, s));
        }
    
        {
            static const char* const n[] = {"baseWidth", "baseHeight", "maxWidth", "maxHeight", "aspectRatio", "fps", "sampleRate"};
            static const char* const s[] = {"I", "I", "I", "I", "F", "D", "D"};
            avInfo_class.reset(new JavaClass(aEnv, aEnv->FindClass("org/libretro/LibRetro$AVInfo"), sizeof(n) / sizeof(n[0]), n, s));
        }
        
        {
        	static const char* const n[] = {"restarted", "framesToRun", "rewind", "width", "height", "pixelFormat", "rotation", "aspect", "keyboard", "buttons", "touchX", "touchY", "touched", "audio", "audioSamples"};
        	static const char* const s[] = {"Z", "I", "Z", "I", "I", "I", "I", "F", "[I", "[I", "S", "S", "Z", "[S", "I"};
        	frame_class.reset(new JavaClass(aEnv, aEnv->FindClass("org/libretro/LibRetro$VideoFrame"), sizeof(n) / sizeof(n[0]), n, s));
        }

        return true;
    }
    catch(...)
    {
        return false;
    }
}
