
/* blitwizard game engine - source code file

  Copyright (C) 2013 Jonas Thiem

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

///
// @author Jonas Thiem  (jonas.thiem@gmail.com)
// @copyright 2011-2013
// @license zlib
// @module blitwizard.graphics

#if (defined(USE_GRAPHICS))

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "luaheader.h"

#include "os.h"
#include "logging.h"
#include "luaerror.h"
#include "luastate.h"
#include "blitwizardobject.h"
#include "physics.h"
#include "objectphysicsdata.h"
#include "luafuncs_object.h"
#include "luafuncs_physics.h"
#include "main.h"
#include "graphics.h"

// camera handling:

struct luacameralistentry {
    // This entry is referenced to by garbage collected lua
    // idrefs, and will be deleted once all lua idrefs are gone.
    // It identifies a camera by camera slot id (see graphics.h).

    int cameraslot;  // if -1, the camera at that slot was deleted,
    // and this is a stale entry
    int refcount;  // if 0, we will delete this entry

    struct luacameralistentry* prev, *next;
};
struct luacameralistentry* luacameralist = NULL;


static int garbagecollect_cameraobjref(lua_State* l) {
    // get id reference to object
    struct luaidref* idref = lua_touserdata(l, -1);

    if (!idref || idref->magic != IDREF_MAGIC
    || idref->type != IDREF_CAMERA) {
        // either wrong magic (-> not a luaidref) or not a camera object
        lua_pushstring(l, "internal error: invalid camera object ref");
        lua_error(l);
        return 0;
    }

    // it is a valid camera object, decrease ref count:
    struct luacameralistentry* e = idref->ref.camera;
    e->refcount--;

    if (e->refcount <= 0) {
        // remove entry from list:
        if (e->prev) {
            e->prev->next = e->next;
        } else {
            luacameralist = e->next;
        }
        if (e->next) {
            e->next->prev = e->prev;
        }
        // delete entry:
        free(e);
    }
    return 0;
}

void luacfuncs_pushcameraidref(lua_State* l, struct luacameralistentry* c) {
    // create luaidref userdata struct which points to the blitwizard object
    struct luaidref* ref = lua_newuserdata(l, sizeof(*ref));
    memset(ref, 0, sizeof(*ref));
    ref->magic = IDREF_MAGIC;
    ref->type = IDREF_CAMERA;
    ref->ref.camera = c;

    // set garbage collect callback:
    luastate_SetGCCallback(l, -1, (int (*)(void*))&garbagecollect_cameraobjref);

    // set metatable __index to blitwizard.graphics.camera table
    lua_getmetatable(l, -1);
    lua_pushstring(l, "__index");
    lua_getglobal(l, "blitwizard");
    if (lua_type(l, -1) == LUA_TTABLE) {
        // extract blitwizard.graphics:
        lua_pushstring(l, "graphics");
        lua_gettable(l, -2);
        lua_insert(l, -2);
        lua_pop(l, 1);  // pop blitwizard table

        if (lua_type(l, -1) != LUA_TTABLE) {
            // error: blitwizard.graphics isn't a table as it should be
            lua_pop(l, 3); // blitwizard.graphics, "__index", metatable
        } else {
            // extract blitwizard.graphics.camera:
            lua_pushstring(l, "camera");
            lua_gettable(l, -2);
            lua_insert(l, -2);
            lua_pop(l, 1);  // pop blitwizard.graphics table

            if (lua_type(l, -1) != LUA_TTABLE) {
                // error: blitwizard.graphics.camera isn't a table as it should be
                lua_pop(l, 3); // blitwizard.graphics.camera, "__index", metatable
            } else {
                lua_rawset(l, -3); // removes blitwizard.graphics.camera, "__index"
                lua_setmetatable(l, -2); // setting remaining metatable
                // stack is now back empty, apart from new userdata!
            }
        }
    } else {
        // error: blitwizard namespace is broken. nothing we could do
        lua_pop(l, 3);  // blitwizard, "__index", metatable
    }

    c->refcount++;
}

struct luacameralistentry* toluacameralistentry(lua_State* l,
int index, int arg, const char* func) {
    if (lua_type(l, index) != LUA_TUSERDATA) {
        haveluaerror(l, badargument1, arg, func, "camera", lua_strtype(l, index));
    }
    if (lua_rawlen(l, index) != sizeof(struct luaidref)) {
        haveluaerror(l, badargument2, arg, func, "not a valid camera");
    }
    struct luaidref* idref = lua_touserdata(l, index);
    if (!idref || idref->magic != IDREF_MAGIC
    || idref->type != IDREF_CAMERA) {
        haveluaerror(l, badargument2, arg, func, "not a valid camera");
    }
    struct luacameralistentry* c = idref->ref.camera;
    if (c->cameraslot) {
        haveluaerror(l, badargument2, arg, func, "this camera was deleted");
    }
    return c;
}

/// Get a table array with all currently active cameras.
// (per default, this is only one)
// @function getCameras
// @treturn table a table list array containing @{blitwizard.graphics.camera|camera} items
int luafuncs_getCameras(lua_State* l) {
    int c = graphics_GetCameraCount();
    int i = 0;
    while (i < c) {
        struct luacameralistentry* e = malloc(sizeof(*e));
        if (!e) {
            return 0;
        }
        memset(e, 0, sizeof(*e));
        e->cameraslot = i;
        e->prev = NULL;
        e->next = luacameralist;
        if (e->next) {
            e->next->prev = e;
        }
        luacameralist = e;

        luacfuncs_pushcameraidref(l, e);
        i++;
    }
    return c;
}

/// Blitwizard camera object which represents a render camera.
//
// A render camera has a rectangle on the screen which is a sort of
// "window" that shows a view into the 2d/3d world of your
// blitwizard game.
//
// Per default, you have one camera that fills up the whole
// screen with one single large view. But if you want to split
// the screen up e.g. for split screen multiplayer games, and show
// different world views side by side, you would want multiple
// cameras.
//
// IMPORTANT: If you want to show a different part of the game world,
// this is where you want to be! Grab the first camera from
// @{blitwizard.graphics.getCameras} and let's go:
//
// Use @{blitwizard.graphics.camera:set2dCenter} to modify the 2d world
// position shown, and @{blitwizard.graphics.camera:set3dCenter} to modify
// the 3d world position shown if you use any 3d objects.
// @type camera

struct luacameralist {
    // This entry is referenced to by garbage collected lua
    // idrefs, and will be deleted once all lua idrefs are gone.
    // It identifies a camera by camera slot id (see graphics.h).

    int cameraslot;  // if -1, the camera at that slot was deleted,
    // and this is a stale entry
    int refcount;  // if 0, we will delete this entry
};

/// Destroy the given camera.
//
// It will be removed from the @{blitwizard.graphics.getCameras} list
// and all references you still have to it will become invalid.
// @function destroy
int luafuncs_camera_destroy(lua_State* l) {
#ifdef USE_SDL_GRAPHICS
    return haveluaerror(l, "the SDL renderer backend doesn't support destroying or adding cameras");
#else
#error "unimplemented code path"
    // ...
#endif
}

/// Add a new camera.
// Returns the new @{blitwizard.graphics.camera|camera}, and
// it will be automatically part of the list of all followup
// @{blitwizard.graphics.getCameras} calls aswell.
//
// If you want to get rid of it again, check out
// @{blitwizard.graphics.camera:destroy}.
// @function new
int luafuncs_camera_new(lua_State* l) {
#ifdef USE_SDL_GRAPHICS
    return haveluaerror(l, "the SDL renderer doesn't support multiple cameras");
#else
#error "unimplemented code path"
    // ... FIXME !!!
#endif
}

/// Get the extend in pixels a game unit in the 2d world
// has at the default camera zoom level of 1.
//
// For 3d, a game unit should be roughly one meter and there
// is no generic way to tell how this ends up in pixels due
// to the very dynamic way objects look depending on your
// position in the world etc.
//
// @function gameUnitsPerPixel
int luafuncs_camera_gameUnitsPerPixel(lua_State* l) {
    struct luacameralistentry* e = toluacameralistentry(
    l, 1, 0, "blitwizard.graphics.camera:gameUnitsPerPixel");
    return UNIT_TO_PIXELS;
}

#endif  // USE_GRAPHICS
