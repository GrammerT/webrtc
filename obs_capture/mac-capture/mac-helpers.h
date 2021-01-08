#pragma once

#include <util/dstr.h>

static inline bool mac_success(OSStatus stat, const char *action)
{
	if (stat != noErr) {
		blog(LOG_WARNING, "%s failed: %d", action, (int)stat);
		return false;
	}

	return true;
}

static inline bool cf_to_cstr(CFStringRef ref, char *buf, size_t size)
{
	if (!ref) return false;
	return (bool)CFStringGetCString(ref, buf, size, kCFStringEncodingUTF8);
}

static inline bool cf_to_dstr(CFStringRef ref, struct dstr *str)
{
    size_t size;
    if (!ref) return false;
    
    
    CFIndex length = CFStringGetLength(ref);
    size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    
    //原来获取的大小方式
    //size = (size_t)CFStringGetLength(ref);
    if (!size)
        return false;
    
    
    dstr_resize(str, size);
    
    
    return (bool)CFStringGetCString(ref, str->array, size+1,
                                    kCFStringEncodingUTF8);
	//size_t size;
	//if (!ref) return false;

	//size = (size_t)CFStringGetLength(ref);
	//if (!size)
		//return false;

	//dstr_resize(str, size);

	//return (bool)CFStringGetCString(ref, str->array, size+1,
		//	kCFStringEncodingUTF8);
}
