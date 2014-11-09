#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <dlfcn.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <EGL/egl.h>

#include "smaa.h"

static void *libGL = 0;
static void (*_glXSwapBuffers)(Display *dpy, GLXDrawable drawable);

static void *libEGL = 0;
static EGLBoolean (*_eglSwapBuffers)(EGLDisplay display, EGLSurface surface);

static
void shim_load_libGL()
{
    if(!libGL) {
	libGL = dlopen("/usr/lib/libGL.so", RTLD_LAZY);
	if(!libGL) {
	    fputs(dlerror(), stderr);
	    exit(1);
	}

	_glXSwapBuffers = (void (*)(Display*, GLXDrawable))dlsym(libGL, "glXSwapBuffers");

	const char *error;
	if((error = dlerror())) {
	    fputs(error, stderr);
	    exit(1);
	}
    }
}

static
void shim_load_libEGL()
{
    if(!libEGL) {
	libEGL = dlopen("/usr/lib/libEGL.so", RTLD_LAZY);
	if(!libEGL) {
	    fputs(dlerror(), stderr);
	    exit(1);
	}

	_eglSwapBuffers = (EGLBoolean (*)(EGLDisplay, EGLSurface))dlsym(libEGL, "eglSwapBuffers");

	const char *error;
	if((error = dlerror())) {
	    fputs(error, stderr);
	    exit(1);
	}
    }
}

static SMAA global_smaa;

void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
    if(!libGL) {
	shim_load_libGL();

	smaa_init(&global_smaa);
    }

    smaa_update(&global_smaa);

    _glXSwapBuffers(dpy, drawable);
}

EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface)
{
    if(!libEGL) {
	shim_load_libEGL();

	smaa_init(&global_smaa);
    }

    smaa_update(&global_smaa);

    return _eglSwapBuffers(display, surface);
}
