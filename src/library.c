
/* blitwizard 2d engine - source code file

  Copyright (C) 2011 Jonas Thiem

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

#include "os.h"
#ifdef WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "library.h"
#include "filelist.h"
#include "file.h"

char fileext[10] = "";

const char* library_GetFileExtension() {
    if (strlen(fileext) > 0) {
        return fileext;
    }

    #ifdef WINDOWS
    strcpy(fileext, ".dll");
    #else
    #ifdef MAC
    strcpy(fileext, ".dylib");
    #else
    strcpy(fileext, ".so");
    #endif
    #endif

    return fileext;
}

void* library_Load(const char* libname) {
    //ignore .<number> endings for the extension check on linux/bsd
    unsigned int ignoreextbytes = 0;
#ifndef WINDOWS
#ifndef MAC
    //identify the .so.NUMBER.NUMBER stuff linux has,
    //and store the length of that NUMBER mess we don't care about
    //in "ignoreextbytes"
    int dot = -1;
    unsigned int k = 0;
    //we want to locate the last dot preceded by non-numeric things:
    while (k < strlen(libname)) {
        if (k < strlen(libname) - 1 && libname[k] == '.') {
            //find a dot
            if (dot < 0 || dot == ((int)k)-1) {
                dot = k;
            }
        }else{
            //if a dot is found, remember it if we just come
            //across NUMBER.NUMBER.... trash:
            if (libname[k] < '0' || libname[k] > '9') {
                //if not numeric, this is no NUMBER ending
                //-> forget about this dot
                dot = -1;
            }
        }
        k++;
    }
    if (dot >= 0) {
        //the difference between full length and dot is what we want to ignore
        ignoreextbytes = strlen(libname) - (unsigned int)dot;
    }
#endif
#endif

    //Check if name ends with library extension
    const char* ext = library_GetFileExtension();
    unsigned int i = strlen(libname)-strlen(ext)-ignoreextbytes;
    unsigned int istart = i;
    int matches = 1;
    while (i < strlen(libname)-ignoreextbytes) {
        if (libname[i] != ext[i-istart]) {
            matches = 0;
            break;
        }
        i++;
    }

    //Append extension if we don't have it
    char* freename = NULL;
    if (!matches) {
        //Compose new name
        freename = malloc(strlen(libname) + strlen(ext) + 1);
        if (!freename) {
            return NULL;
        }
        memcpy(freename, libname, strlen(libname));
        memcpy(freename + strlen(libname), ext, strlen(ext));
        freename[strlen(libname) + strlen(ext)] = 0;

        //Set it as name
        libname = (const char*)freename;
    }

    //Load library
    #ifdef WINDOWS
    void* ptr = (void*)LoadLibrary(libname);
    #else
    void* ptr = dlopen(libname, RTLD_LAZY|RTLD_LOCAL);
    #endif

    //Clean up and return pointer
    if (freename) {
        free(freename);
    }
    return ptr;
}

static void library_SearchDir(const char* dir, const char* name, void** ptr) {
    if (*ptr) {
        //library has already been found successfully
        return;
    }

    struct filelistcontext* fctx = filelist_Create(dir);
    if (!fctx) {
        return;
    }

    char namebuf[600];
    int isdir;
    while (filelist_GetNextFile(fctx, namebuf, sizeof(namebuf), &isdir) == 1) {
        if (!isdir) {
            //it is a file. does it start with our name + .so?
            if (strlen(namebuf) >= strlen(name) + 3 && memcmp(namebuf, name, strlen(name)) == 0) {
                if (memcmp(namebuf + strlen(name), ".so", 3) == 0) {
                    //does it end after .so or continue with a dot?
                    if (strlen(namebuf) == strlen(name) + 3 || namebuf[strlen(name) + 3] == '.') {
                        //yes! so let's attempt to load this
                        char* p = file_AddComponentToPath(dir, namebuf);
                        if (p) {
                            *ptr = library_Load(p);
                            free(p);
                            if (*ptr) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    filelist_Free(fctx);
}

void* library_LoadSearch(const char* name) {
    void* ptr = library_Load(name);
    if (ptr) {
        return ptr;
    }

    //try a more interesting search:
#ifndef WINDOWS
#ifndef MAC
    library_SearchDir("/usr/lib", name, &ptr);
    library_SearchDir("/lib", name, &ptr);
    library_SearchDir("/usr/local/lib", name, &ptr);
#ifdef _64BIT
    library_SearchDir("/usr/lib64", name, &ptr);
    library_SearchDir("/usr/local/lib64", name, &ptr);
#else
    library_SearchDir("/usr/lib32", name, &ptr);
    library_SearchDIr("/usr/local/lib32", &name, &ptr);
#endif
    if (ptr) {
        return ptr;
    }
#else
    //Apple:
    if (strcasecmp(name, "ffmpegsumo") == 0) {
        //Reference: /Applications/Google Chrome.app/Contents/Versions/20.0.1132.11/Google Chrome Framework.framework/Libraries/ffmpegsumo.so
        const char* chromepath = Mac_getPathForApplication("Google Chrome");
        if (chromepath && strlen(chromepath) > 0) {
            //Find out the version folder:
            struct filelistcontext* fctx = filelist_Create(chromepath);
            char versionfolder[512];
            int isdir = 0;
            while (filelist_GetNextFile(fctx, versionfolder, sizeof(versionfolder), &isdir) == 1) {
                if (isdir) {break;}
            }
            if (!isdir) {
                //No fitting sub folder found
#ifdef FFMPEGLOCATEDEBUG
                printwarning("Warning: [FFmpeg-locate] Failed to find version sub folder of Google Chrome");
#endif
            }else{
                //Compose final path:
                char pathbuf[512];
                char sep[2] = "/";
                char sep2[2] = "/";
                if (chromepath[strlen(chromepath)-1] == '/') {
                    strcpy(sep, "");
                }
                if (strlen(versionfolder) > 0 && versionfolder[strlen(versionfolder)-1] == '/') {
                    strcpy(sep2, "");
                }
                snprintf(pathbuf, sizeof(pathbuf), "%s%sContents/Versions/%s/Google Chrome Framework.framework/Libraries/ffmpegsumo.so", pathbuf, sep, versionfolder, sep2);

                //Attempt to load the lib now:
                ptr = library_Load(pathbuf);
                if (ptr) {return ptr;}
#ifdef FFMPEGLOCATEDEBUG
                printinfo("[FFmpeg-locate] Failed to load FFmpeg from Google Chrome (Path: \"%s\")", pathbuf);
#endif
            }
        }else{
#ifdef FFMPEGLOCATEDEBUG
            printwarning("Warning: [FFmpeg-locate] Google chrome not found");
#endif
        }

        //Reference: /Applications/Steam.app/Contents/MacOS/osx32/
        const char* steampath = Mac_getPathForApplication("Steam");
        if (steampath && strlen(steampath) > 0) {
            //Compose final path:
            char pathbuf[512];
            char sep[2] = "/";
            if (steampath[strlen(steampath)-1] == '/') {
                strcpy(sep, "");
            }
            snprintf(pathbuf, sizeof(pathbuf), "%s%sContents/MacOS/osx32/ffmpegsumo.so", steampath, sep);

            //Attempt to load the lib now:
            ptr = library_Load(pathbuf);
            if (ptr) {return ptr;}
#ifdef FFMPEGLOCATEDEBUG
            printinfo("[FFmpeg-locate] Failed to load FFmpeg from Valve Steam (Path: \"%s\")", pathbuf);
#endif 
        }else{
#ifdef FFMPEGLOCATEDEBUG
            printwarning("Warning: [FFmpeg-locate] Valve Steam not found");
#endif
        }
    }
#endif
#endif
    return NULL;
}

void* library_GetSymbol(void* ptr, const char* name) {
    #ifdef WINDOWS
    return (void*)GetProcAddress((HMODULE)ptr, name);
    #else
    return dlsym(ptr, name);
    #endif
}

void library_Close(void* ptr) {
    #ifdef WINDOWS
    FreeLibrary((HMODULE)ptr);
    #else
    dlclose(ptr);
    #endif
}
