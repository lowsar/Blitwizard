
/* blitwizard game engine - source code file

  Copyright (C) 2011-2014 Jonas Thiem

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

*/

#ifndef BLITWIZARD_GRAPHICSTEXTUREMANAGER_H_
#define BLITWIZARD_GRAPHICSTEXTUREMANAGER_H_

#include "config.h"
#include "os.h"

// This is the texture manager.
// You can request textures and it will provide them,
// however it can also revoke your texture access again.
// YOU ONLY GET TEMPORARY COPIES OF TEXTURES THAT CAN BE REVOKED ANYTIME.
// Carefully implement the callbacks that revoke your texture access
// and give you e.g. smaller versions!
//
// The main purpose of this texture manager is to keep the full
// resolution of frequently used textures in memory, while automatically
// swapping to scaled down versions of textures which are rather
// infrequently used. The raw image data of high resolution versions
// might get dumped to disk or memory (in raw decoded form) where it
// can be retrieved very fast.
//
// As long as you adhere to all callbacks providing you with higher, lower,
// otherwise different textures, it will automagically work.
//
// TECHNICAL DETAILS: uses graphicstexturelist/graphicstexture for
// texture GPU uploading, uses diskcache for disk caching.

#ifdef USE_GRAPHICS

#include <stdint.h>

#include "graphics.h"
#include "graphicstexture.h"

struct texturerequesthandle;

// texturemanager_RequestTexture allows you to request a texture.
// You will get a request handle that identifies your request.
//
// You provide two callbacks: the textureDimensionInfo callback,
// and the textureSwitch callback.
//
// The texture manager will call the first one as soon as the
// texture dimensions are known. This is usually quite early.
//
// The texture manager will call the second one with an actual
// texture as soon as it is available. IT MIGHT BE OF DIFFERENT
// DIMENSIONS THEN THE ONE YOU GOT. Also, the callback can be
// called again any time, at which point it will provide a new
// texture and THE OLD ONE WILL GET INVALID with the next call
// of texturemanager_tick().
// (Exception: when texturemanager_deviceLost() is called, it
// becomes invalid right with the callback. This can currently
// not be solved differently, sorry)
//
// The textureSwitch texture passed to you can be NULL aswell!
// (you must stop using the old one and not draw anything until
// you get a proper texture with the next textureSwitch call)
//
// This means the texture manager can essentially provide you
// with any chain of different texture versions and you always
// must use the newest one and stop using the older ones.
//
// The texture manager will ensure the old ones are properly
// free'd from memory, don't attempt to do that yourself.
//
// If you no longer wish to use a texture or all the future
// versions of it you might end up with, use
// texturemanager_DestroyRequest.
//
// Multiple requests of the same texture might or might not
// give you the same texture. (The texture manager certainly
// will try to avoid duplication)
//
// NOTE: Report back usage of your texture through
// texturemanager_UsingRequest. Do it even if your current
// copy provided through textureSwitch is a NULL copy and you
// cannot actually draw things. The texture manager will
// give your texture higher priority if usage is high.
//
// BEWARE OF THE CALLBACKS: Inside the callbacks, don't
// call the texture manager api ever. This will break things.
// (Because of deadlocks/threading issues)
struct texturerequesthandle* texturemanager_requestTexture(
    const char* path,
    void (*textureDimensionInfo)(struct texturerequesthandle* request,
        size_t width, size_t height, void* userdata),
    void (*textureSwitch)(struct texturerequesthandle* request,
        struct graphicstexture* texture, void* userdata),
        void (*textureHandlingDone)(struct texturerequesthandle* request,
        void* userdata),
    void* userdata
);

// Use texturemanager_UsingRequest to report back how much
// you use a texture, and at which visibility (so the texture
// manager can decide whether to scale it up or down to save
// memory for the textures used most).
//
// !! LOCK WITH texturemanager_lockForTextureAccess BEFORE CALL !!
//
// Levels of visibility explained:
//
// Please make sure you actually use the diverse levels of
// visibility provided. It is not so important you
// use the INVISIBLE option when things get out of visibility,
// but you should use DETAIL/NORMAL/DISTANT as accurately as
// possible to allow the scaling down of far away textures.
//
// DETAIL should only be used for very close up (a few meters
// at best) textures which are rendered at larger than
// 1:1 pixel size.
//
// NORMAL should be used for anything rendered very close to 1:1.
//
// DISTANT should be used for anything notably below 1:1 size,
// e.g. 50% in screen pixels compared to its actual texture size
// or smaller.
//
// If you do actually have visibility algorithms, use
// INVISIBLE for things that aren't rendered at all. However,
// avoid this for any objects that would be considered DETAIL
// judging from their distance but which are just temporarily obscured,
// because they will otherwise pop up with ugly low res textures
// when suddenly getting visible again in close range :-)
void texturemanager_usingRequest(
    struct texturerequesthandle* request, int visibility);
#define USING_AT_VISIBILITY_DETAIL 0
#define USING_AT_VISIBILITY_NORMAL 1
#define USING_AT_VISIBILITY_DISTANT 2
#define USING_AT_VISIBILITY_INVISIBLE 3
#define USING_AT_COUNT 4

// will be used for normal use:
#define TEXSIZE_HIGH 1024
// will be used for previously normal, now distant use:
#define TEXSIZE_MEDIUM 512
// will be used for longer distant use (SCALEDOWNSECONDSLONG):
#define TEXSIZE_LOW 128
// will be used when texture not used for a longer time:
#define TEXSIZE_TINY 32

// How fast to scale down things:
// Going down to TINY roughly after this time:
#define SCALEDOWNSECONDSVERYVERYLONG (120*2)
// Going down to LOW roughly after this time:
#define SCALEDOWNSECONDSVERYLONG (120*1)
// Going down to MEDIUM roughly after this time:
#define SCALEDOWNSECONDSLONG 60
// Going down to HIGH roughly after this time:
#define SCALEDOWNSECONDS 30

// helps with debugging downscaling:
#ifdef UNITTEST
#define ULTRAFASTDOWNSCALE
#endif

#ifdef ULTRAFASTDOWNSCALE
#warning "-------------------------------------"
#warning "debugging with ultra fast down scale!"
#warning "         !!! !!! !!! !!! !!!         "
#warning "    THIS SHOULD NEVER BE ENABLED     "
#warning "          IN A REGULAR BUILD         "
#warning "         !!! !!! !!! !!! !!!         "
#warning "-------------------------------------"
#undef SCALEDOWNSECONDS
#define SCALEDOWNSECONDS 2
#undef SCALEDOWNSECONDSLONG
#define SCALEDOWNSECONDSLONG 3
#undef SCALEDOWNSECONDSVERYLONG
#define SCALEDOWNSECONDSVERYLONG 4
#undef SCALEDOWNSECONDSVERYVERYLONG
#define SCALEDOWNSECONDSVERYVERYLONG 5
#endif

// How often to check all textures for down- and upscaling:
#define ADAPTINTERVAL 1


// Destroy a texture request. You will still get a textureSwitch
// callback setting your provided texture back to NULL if
// it's not already, then no further callbacks.
//
// When this function returns, you will be guaranteed to no
// longer receive any callbacks related to this texture request.
void texturemanager_destroyRequest(
    struct texturerequesthandle* request);

// Call this as often as possible (preferrably right before rendering)
// to allow the graphics texture manager to delete old textures
// from memory.
//
// At this point, all graphicstexture* handles that were exchanged
// through textureSwitch callbacks with new ones (or NULL) become
// invalid.
//
// Hence, call this at a point where your drawing operations are
// complete and it won't crash your drawing code when textures
// become unavailable and new ones need to be used.
void texturemanager_tick(void);

// Use those if you need to access a graphicstexturemanaged directly
// and you need to be sure the texture manager is not messing around
// with it while you're doing that:
void texturemanager_lockForTextureAccess(void);
void texturemanager_releaseFromTextureAccess(void);

// Report that the device was lost and that all GPU textures are now
// declared garbage:
void texturemanager_deviceLost(void);
// THIS FUNCTION MIGHT HANG!! (worst case: 1-5 seconds)
// It needs to wait for some concurrent processes to finish in some
// rare cases. Expect it to hang a short bit.

// Report that device was restored and GPU textures may be reuploaded:
void texturemanager_deviceRestored(void);

// Force a specific texture to be reloaded:
void texturemanager_wipeTexture(const char* tex);
// THIS FUNCTION MIGHT HANG!! (worst case: 1-5 seconds)
// Similar to texturemanager_deviceLost(), this function might be
// blocked by concurrent processes for which it needs to wait to finish!
// It is less likely to hang than texturemanager_deviceLost(),
// but don't be surprised if it does!

// INFO FUNCTIONS (mainly useful for debugging) //

// Query the current GPU memory use by the texture manager in bytes:
uint64_t texturemanager_getGpuMemoryUse(void);

// Query the most notable use level of the given texture.
// Returns USING_AT_* info.
int texturemanager_getTextureUsageInfo(const char* texture);

// Query the highest texture size loaded onto the GPU for the
// given texture.
// Returns 4 (high) to 1 (tiny), or 0 for full original size.
// -1 means the texture isn't currently on the GPU.
int texturemanager_getTextureGpuSizeInfo(const char* texture); 

// Query the highest texture size info loaded in system memory
// for the given texture.
// Returns 4 (high) to 1 (tiny) or 0 for full original size.
// -1 means the texture isn't currently held in system memory.
int texturemanager_getTextureRamSizeInfo(const char* texture);

// Query the amount of texture requests waiting to be served this
// particular texture (no matter if it's currently loaded or not).
int texturemanager_getWaitingTextureRequests(const char* texture);

// Query the amount of texture requests which are served any size
// of this particular texture right now.
int texturemanager_getServedTextureRequests(const char* texture);

// Check if the initial loading of the given texture has been
// completed. (This doesn't mean it is currently available,
// it may already have been unloaded again)
int texturemanager_isInitialTextureLoadDone(const char* texture);

// Get the total amount of texture requests.
size_t texturemanager_getRequestCount(void);

#endif  // USE_GRAPHICS

#endif  // BLITWIZARD_GRAPHICSTEXTUREMANAGER_H_

