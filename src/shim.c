#define _GNU_SOURCE 1


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <dlfcn.h>

#include "smaa.h"

#include <GL/glx.h>
#include <EGL/egl.h>

#ifndef __LP64__
#define lib_path(lib) "/usr/lib32/" lib
#else
#define lib_path(lib) "/usr/lib/" lib
#endif

static void *libGL = 0;
static void (*_glXSwapBuffers)(Display *dpy, GLXDrawable drawable);
static void (*(*_glXGetProcAddress)(const GLubyte *procName))();

static void *libEGL = 0;
static EGLBoolean (*_eglSwapBuffers)(EGLDisplay display, EGLSurface surface);

static void * (*real_dlsym)(void *, const char *) = 0;

static
void shim_load_libGL()
{
    if(!libGL) {
	libGL = dlopen(lib_path("libGL.so"), RTLD_LAZY);

	fprintf(stderr, "%s\n", lib_path("libGL.so"));
	
	if(!libGL) {
	    fputs(dlerror(), stderr);
	    exit(1);
	}

	_glXSwapBuffers = (void (*)(Display*, GLXDrawable))real_dlsym(libGL, "glXSwapBuffers");
	_glXGetProcAddress = (void (*(*)(const GLubyte *procName))())real_dlsym(libGL, "glXGetProcAddressARB");

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
	libEGL = dlopen(lib_path("libEGL.so"), RTLD_LAZY);
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

static SMAA *global_smaa = 0;

void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
    GLint vao, program, texture, depth, blending;
    GLfloat clear_color[4];
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &texture);
    glGetIntegerv(GL_DEPTH_TEST, &depth);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
    glGetIntegerv(GL_BLEND, &blending);

    GLint textures[3];
    for(int i = 0; i < 3; i++) {
	glActiveTexture(GL_TEXTURE0 + i);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i]);
    }

    glActiveTexture(GL_TEXTURE0);
    
    if(!libGL) {
	shim_load_libGL();
    }

    if(!global_smaa) {
	global_smaa = smaa_create();
	smaa_init(global_smaa);
    }

    smaa_update(global_smaa);

    glBindVertexArray(vao);
    glUseProgram(program);
    if(depth) {
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }
    glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
    if(blending) {
	glEnable(GL_BLEND);
    } else {
	glDisable(GL_BLEND);
    }
    for(int i = 0; i < 3; i++) {
	glActiveTexture(GL_TEXTURE0 + i);
	glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    glActiveTexture(texture);

    _glXSwapBuffers(dpy, drawable);
}

void (*(glXGetProcAddress)(const GLubyte *procName))()
{
    if(!libGL) {
	shim_load_libGL();
    }

    if(!strcmp((const char*) procName, "glXSwapBuffers")) {
	fprintf(stderr, "with_smaa: glXGetProcAddress[ARB]: redirecting glXSwapBuffers\n");
	return (void (*)()) glXSwapBuffers;
    }

    return _glXGetProcAddress(procName);
}

void (*(glXGetProcAddressARB)(const GLubyte *procName))()
{
    return glXGetProcAddress(procName);
}

EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface)
{
    if(!libEGL) {
	shim_load_libEGL();
    }

    smaa_update(global_smaa);

    return _eglSwapBuffers(display, surface);
}

void *__libc_dlsym (void *, const char *);

void *dlsym(void *handle, const char *name)
{
    if(!real_dlsym) {
	fprintf(stderr, "with_smaa: Getting real dlsym...\n");
	void *libdl = dlopen(lib_path("libdl.so"), RTLD_NOW);
	real_dlsym = __libc_dlsym(libdl, "dlsym");	
	fprintf(stderr, "with_smaa: real_dlsym=%p\n", real_dlsym);
    }
    
    if (!strcmp(name, "dlsym")) {
	return (void*) dlsym;
    } else if(!strcmp(name, "glXGetProcAddressARB")) {
	fprintf(stderr, "with_smaa: dlsym: redirecting glXGetProcAddressARB\n");
	return (void*) glXGetProcAddress;
    } else if(!strcmp(name, "glXGetProcAddress")) {
	fprintf(stderr, "with_smaa: dlsym: redirecting glXGetProcAddress\n");
	return (void*) glXGetProcAddress;
    } else if(!strcmp(name, "glXSwapBuffers")) {
	fprintf(stderr, "with_smaa: dlsym: redirecting glXSwapBuffers\n");
	return (void*) glXSwapBuffers;
    }

    return real_dlsym(handle, name);
}
