#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <dlfcn.h>

#include "smaa.h"

#include <GL/glx.h>
#include <EGL/egl.h>


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
    GLint vao, program, texture_2d, texture, depth;
    GLfloat clear_color[4];
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_2d);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &texture);
    glGetIntegerv(GL_DEPTH_TEST, &depth);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
    
    if(!libGL) {
	shim_load_libGL();

	smaa_init(&global_smaa);
    }

//    glFinish();
    smaa_update(&global_smaa);

    glBindVertexArray(vao);
    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D, texture_2d);
    glActiveTexture(texture);
    if(depth) {
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);

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
