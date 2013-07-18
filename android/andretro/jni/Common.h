#ifndef MJNI_COMMON_H
#define MJNI_COMMON_H

#include <string>
#include <map>
#include <vector>
#include <jni.h>
#include <android/log.h>
#include <fstream>
#include <exception>

#define Log(...) __android_log_print(ANDROID_LOG_INFO, "andretro", __VA_ARGS__);

class FileReader
{
    private:
        std::vector<uint8_t> data;

    public:
        FileReader()
        {
        
        }
    
        FileReader(const char* aPath)
        {
            load(aPath);
        }
        
        bool load(const char* aPath)
        {
            close();
            
            // Open new file
            if(aPath)
            {
                FILE* file = fopen(aPath, "rb");
                
                if(file)
                {
                    fseek(file, 0, SEEK_END);
                    data.resize(ftell(file));
                    fseek(file, 0, SEEK_SET);
                    
                    fread(&data[0], data.size(), 1, file);
                    
                    fclose(file);
                    return true;
                }
            }
            
            return false;
        }
        
        void close()
        {
            data.clear();
        }
        
        bool isOpen() const
        {
            return 0 < data.size();
        }
        
        const uint8_t* base() const
        {
            return &data[0];
        }
        
        size_t size() const
        {
            return data.size();
        }
};

// Makes no guarantees about aData if read fails!
bool ReadFile(const char* aPath, void* aData, size_t aLength)
{
	FILE* file = fopen(aPath, "rb");

	if(file)
	{
		bool result = 1 == fread(aData, aLength, 1, file);
		fclose(file);
		return result;
	}

	return false;
}

bool DumpFile(const char* aPath, const void* aData, size_t aLength)
{
	FILE* file = fopen(aPath, "wb");

	if(file)
	{
		bool result = 1 == fwrite(aData, aLength, 1, file);
		fclose(file);
		return result;
	}

	return false;
}

struct JavaClass
{
	std::map<std::string, jfieldID> fields;

	JavaClass(JNIEnv* aEnv, jclass aClass, int aFieldCount, const char* const* aNames, const char* const* aSigs)
	{
		if(!aEnv || !aClass || !aNames || !aSigs || !aFieldCount)
		{
			throw std::exception();
		}

		for(int i = 0; i != aFieldCount; i ++)
		{
			fields[aNames[i]] = aEnv->GetFieldID(aClass, aNames[i], aSigs[i]);

			if(fields[aNames[i]] == 0)
			{
				throw std::exception();
			}
		}
	}

	jfieldID operator[](const char* aName)
	{
		return fields[aName];
	}
};

// jstring wrapper
struct JavaChars
{
	JNIEnv* env;
	jstring string;
	const char* chars;

	JavaChars(JNIEnv* aEnv, jstring aString)
	{
		env = aEnv;
		string = aString;
		chars = env->GetStringUTFChars(string, 0);
	}

	~JavaChars()
	{
	    env->ReleaseStringUTFChars(string, chars);
	}

	operator const char*() const
	{
		return chars;
	}

	// No copy
private:
	JavaChars(const JavaChars&);
	JavaChars& operator= (const JavaChars&);

};

// Build a string with checking (later anyway)
inline jstring JavaString(JNIEnv* aEnv, const char* aString)
{
	jstring string = aEnv->NewStringUTF(aString ? aString : "(NULL)");

	if(string)
	{
		return string;
	}

	throw std::exception();
}

#endif
