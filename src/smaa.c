
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AreaTex.h"
#include "SearchTex.h"

#include "smaa.h"
#include "smaa_shader.h"

static
void smaa_texture_filter_setup()
{
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static
GLuint smaa_compile_shader(const char *source, int source_length, GLenum type)
{
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, &source_length);
    glCompileShader(shader);

    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if(compile_status == GL_FALSE) {
	fprintf(stderr, "smaa_compile_shader error\n");
	fputs(source, stderr);
	fprintf(stderr, "\n\nShader info log:\n");

	GLint info_log_length;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);

	char *info_log = malloc(info_log_length);
	glGetShaderInfoLog(shader, info_log_length, 0, info_log);
	fputs(info_log, stderr);
	free(info_log);

	glDeleteShader(shader);
	return 0;
    }

    return shader;
}

static
const char *strErrorGl(GLenum error)
{
    switch(error) {
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
    default: return "<unknown error code>";
    }
}

#define checkGl() { int line = __LINE__;				\
	GLenum error;							\
	while((error = glGetError()) != GL_NO_ERROR) {			\
	    fprintf(stderr, "%3d: %s\n", line, strErrorGl(error));	\
	}								\
    }


static
void smaa_compile_smaa(GLuint program, GLenum type, const char *defs, const char *main)
{
    // It appears there is a bug regarding passing multiple strings
    // to glShaderSource with MESA. It won't replace #defines made in
    // a string passed after a string #defining it.

    char *fs_vs = "";
    if(type == GL_VERTEX_SHADER) {
	fs_vs = "#define SMAA_INCLUDE_PS 0\n";
    }

    size_t source_size = strlen(defs) + strlen(fs_vs)
	+ strlen(main) + sizeof(SMAA_hlsl) + 1;
    char *source = malloc(source_size);
    source[0] = 0;
    strncat(source, defs, source_size);
    strncat(source, fs_vs, source_size);
    strncat(source, (char*) SMAA_hlsl, source_size);
    strncat(source, main, source_size);

    glAttachShader(program, smaa_compile_shader(source, source_size, type));
}

static
int smaa_link_program(GLuint program)
{ 
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(!status) {
	fprintf(stderr, "shader linking failure\n");

	GLint info_log_length;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);

	char *info_log = malloc(info_log_length);
	glGetProgramInfoLog(program, info_log_length, 0, info_log);
	fputs(info_log, stderr);
	free(info_log);

	return 0;
    }

    return 1;
}

static
int smaa_create_fbo(GLuint *_fbo, GLuint *_tex, int width, int height)
{
    GLuint fbo, tex, rb;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glGenRenderbuffers(1, &rb);

    if(!fbo || !tex || !rb) {
	return 1;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    smaa_texture_filter_setup();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width , height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
    if(status == GL_FRAMEBUFFER_COMPLETE) {
	(*_fbo) = fbo;
	(*_tex) = tex;
	return 1;
    } else {
	fprintf(stderr, "smaa_create_fbo failed, status=%d\n", status);
	return 0;
    }
}

static
void smaa_resize_fbo_texture(GLuint tex, int width, int height)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
}

static
const char *smaa_settings(SMAA *smaa)
{
    if(!smaa->legacy) {
	return
	    "#version 330\n"
	    "#define SMAA_PRESET_ULTRA 1\n"
	    "#define SMAA_RT_METRICS in_rt_metrics\n"
	    "#define SMAA_GLSL_3 1\n"
	    "uniform vec4 in_rt_metrics;\n";
    } else {
	return
	    "#version 130\n"
	    "#define SMAA_PRESET_ULTRA 1\n"
	    "#define SMAA_RT_METRICS in_rt_metrics\n"
	    "#define SMAA_GLSL_3 1\n"
	    "uniform vec4 in_rt_metrics;\n";
    }
}

static
int smaa_init_smaa_program(SMAA *smaa, GLuint *program, const char *vsmain, const char *fsmain)
{
    *program = glCreateProgram();

    smaa_compile_smaa(*program, GL_VERTEX_SHADER, smaa_settings(smaa), vsmain);

    smaa_compile_smaa(*program, GL_FRAGMENT_SHADER, smaa_settings(smaa), fsmain);

    if(smaa->legacy) {
	// All legacy programs have an in_texcoord attribute
	glBindAttribLocation(*program, 0, "in_texcoord");
    }

    GLint attached_shaders;
    glGetProgramiv(*program, GL_ATTACHED_SHADERS, &attached_shaders);
    if(attached_shaders != 2 || !smaa_link_program(*program)) {
	fprintf(stderr, "smaa_init_smaa_program error.!\n");
	return 0;
    }

    return 1;
}

static
int smaa_init_smaa_core(SMAA *smaa)
{
    int r = 
	smaa_init_smaa_program
	(smaa, &smaa->edge_shader,
	 "layout(location = 0) in vec2 in_texcoord;\n"
	 "out vec2 texcoord;\n"
	 "out vec4 offset[3];\n"
	 
	 "void main() {\n"
	 "    SMAAEdgeDetectionVS(in_texcoord, offset);\n"
	 "    texcoord = in_texcoord;\n"
	 "    gl_Position = vec4(in_texcoord * 2.0f + vec2(-1.0f, -1.0f), 0.0f, 1.0f);\n"
	 "}",
	 
	 "uniform sampler2D in_tex;\n"
	 "in vec2 texcoord;\n"
	 "in vec4 offset[3];\n"
	 "layout(location = 0) out vec4 out_color;\n"

	 "void main() {\n"
	 "    out_color = vec4(SMAALumaEdgeDetectionPS(texcoord, offset, in_tex), 0.0f, 1.0f);\n"
	 "}");

    if(!r) {
	return r;
    }

    r = 
	smaa_init_smaa_program
	(smaa, &smaa->blend_shader,
	 "layout(location = 0) in vec2 in_texcoord;\n"
	 "out vec2 texcoord;\n"
	 "out vec2 pixcoord;\n"
	 "out vec4 offset[3];\n"
	 
	 "void main() {\n"
	 "    SMAABlendingWeightCalculationVS(in_texcoord, pixcoord, offset);\n"
	 "    texcoord = in_texcoord;\n"
	 "    gl_Position = vec4(in_texcoord * 2.0f + vec2(-1.0f, -1.0f), 0.0f, 1.0f);\n"
	 "}",
	 
	 "uniform sampler2D in_tex;\n"
	 "uniform sampler2D in_area_tex;\n"
	 "uniform sampler2D in_search_tex;\n"
	 "in vec2 texcoord;\n"
	 "in vec2 pixcoord;\n"
	 "in vec4 offset[3];\n"
	 "layout(location = 0) out vec4 out_color;\n"
	 
	 "void main() {\n"
	 "    out_color = SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset,\n"
	 "        in_tex, in_area_tex, in_search_tex, vec4(0.0f));\n"
	 "}");

    if(!r) {
	return r;
    }

    r = smaa_init_smaa_program
	(smaa, &smaa->neighbor_shader,
	 "layout(location = 0) in vec2 in_texcoord;\n"
	 "out vec2 texcoord;\n"
	 "out vec4 offset;\n"
	 
	 "void main() {\n"
	 "    SMAANeighborhoodBlendingVS(in_texcoord, offset);\n"
	 "    texcoord = in_texcoord;\n"
	 "    gl_Position = vec4(in_texcoord * 2.0f + vec2(-1.0f, -1.0f), 0.0f, 1.0f);\n"
	 "}",
	 
	 "uniform sampler2D in_tex;\n"
	 "uniform sampler2D in_blend_tex;\n"
	 "in vec2 texcoord;\n"
	 "in vec4 offset;\n"
	 "layout(location = 0) out vec4 out_color;\n"

	 
	 "void main() {\n"
	 "    out_color = SMAANeighborhoodBlendingPS(texcoord, offset, in_tex, in_blend_tex);\n"
	 "}");

    if(!r) {
	return r;
    }

    return 1;
}

static
int smaa_init_smaa_legacy(SMAA *smaa)
{
    int r = 
	smaa_init_smaa_program
	(smaa, &smaa->edge_shader,
	 "attribute vec2 in_texcoord;\n"
	 "varying vec2 texcoord;\n"
	 "varying vec4 offset[3];\n"
	 
	 "void main() {\n"
	 "    SMAAEdgeDetectionVS(in_texcoord, offset);\n"
	 "    texcoord = in_texcoord;\n"
	 "    gl_Position = vec4(in_texcoord * 2.0f + vec2(-1.0f, -1.0f), 0.0f, 1.0f);\n"
	 "}",
	 
	 "uniform sampler2D in_tex;\n"
	 "varying vec2 texcoord;\n"
	 "varying vec4 offset[3];\n"

	 "void main() {\n"
	 "    gl_FragColor = vec4(SMAALumaEdgeDetectionPS(texcoord, offset, in_tex), 0.0f, 1.0f);\n"
	 "}");

    if(!r) {
	return r;
    }

    r = 
	smaa_init_smaa_program
	(smaa, &smaa->blend_shader,
	 "attribute vec2 in_texcoord;\n"
	 "varying vec2 texcoord;\n"
	 "varying vec2 pixcoord;\n"
	 "varying vec4 offset[3];\n"
	 
	 "void main() {\n"
	 "    SMAABlendingWeightCalculationVS(in_texcoord, pixcoord, offset);\n"
	 "    texcoord = in_texcoord;\n"
	 "    gl_Position = vec4(in_texcoord * 2.0f + vec2(-1.0f, -1.0f), 0.0f, 1.0f);\n"
	 "}",
	 
	 "uniform sampler2D in_tex;\n"
	 "uniform sampler2D in_area_tex;\n"
	 "uniform sampler2D in_search_tex;\n"
	 "varying vec2 texcoord;\n"
	 "varying vec2 pixcoord;\n"
	 "varying vec4 offset[3];\n"
	 
	 "void main() {\n"
	 "    gl_FragColor = SMAABlendingWeightCalculationPS(texcoord, pixcoord, offset,\n"
	 "        in_tex, in_area_tex, in_search_tex, vec4(0.0f));\n"
	 "}");

    if(!r) {
	return r;
    }

    r = smaa_init_smaa_program
	(smaa, &smaa->neighbor_shader,
	 "attribute vec2 in_texcoord;\n"
	 "varying vec2 texcoord;\n"
	 "varying vec4 offset;\n"
	 
	 "void main() {\n"
	 "    SMAANeighborhoodBlendingVS(in_texcoord, offset);\n"
	 "    texcoord = in_texcoord;\n"
	 "    gl_Position = vec4(in_texcoord * 2.0f + vec2(-1.0f, -1.0f), 0.0f, 1.0f);\n"
	 "}",
	 
	 "uniform sampler2D in_tex;\n"
	 "uniform sampler2D in_blend_tex;\n"
	 "varying vec2 texcoord;\n"
	 "varying vec4 offset;\n"
	 
	 "void main() {\n"
	 "    gl_FragColor = SMAANeighborhoodBlendingPS(texcoord, offset, in_tex, in_blend_tex);\n"
	 "}");

    if(!r) {
	return r;
    }

    return 1;
}

static
int smaa_init_smaa(SMAA *smaa)
{
    if(smaa->legacy) {
	return smaa_init_smaa_legacy(smaa);
    } else {
	return smaa_init_smaa_core(smaa);
    }
}

internal
SMAA *smaa_create()
{
    SMAA *smaa = malloc(sizeof(SMAA));
    smaa->initialized = 0;
    return smaa;
}

internal
void smaa_init(SMAA *smaa)
{
    fprintf(stderr, "with_smaa: GLSL_VERSION: %s\n",
	    (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    fprintf(stderr, "with_smaa: GL_VERSION: %s\n",
	    (const char*)glGetString(GL_VERSION));

    int major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    if((major == 3 && minor >= 2) || major > 3) {
	smaa->legacy = 0;
    } else if(major == 3) {
	fprintf(stderr, "with_smaa: detected legacy GL\n");
	smaa->legacy = 1;
    } else {
	fprintf(stderr, "with_smaa: No OpenGL 3 context found\n");
	smaa->incompatible = 1;
	return;
    }

    smaa->incompatible = 0;

    glGenTextures(1, &smaa->area_tex);
    glGenTextures(1, &smaa->search_tex);
    glGenTextures(1, &smaa->color_tex);

    glBindTexture(GL_TEXTURE_2D, smaa->area_tex);
    smaa_texture_filter_setup();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, AREATEX_WIDTH, AREATEX_HEIGHT, 0, GL_RG, GL_UNSIGNED_BYTE, areaTexBytes);

    glBindTexture(GL_TEXTURE_2D, smaa->search_tex);
    smaa_texture_filter_setup();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, searchTexBytes);


    glBindTexture(GL_TEXTURE_2D, smaa->color_tex);
    smaa_texture_filter_setup();

    glBindTexture(GL_TEXTURE_2D, 0);

    checkGl();

    if(!smaa_init_smaa(smaa)) {
	fprintf(stderr, "smaa_init_smaa error.\n");
	return;
    }

    checkGl();

    GLint size[4];
    glGetIntegerv(GL_VIEWPORT, size);
    int width = size[2], height = size[3];

    smaa->old_width = width;
    smaa->old_height = height;

    if(!smaa_create_fbo(&smaa->edge_fbo, &smaa->edge_tex, width, height)) {
	fprintf(stderr, "smaa_create_fbo(edge_fbo) failed.\n");
	return;
    }

    if(!smaa_create_fbo(&smaa->blend_fbo, &smaa->blend_tex, width, height)) {
	fprintf(stderr, "smaa_create_fbo(blend_fbo) failed.\n");
	return;
    }

    checkGl();

    glGenVertexArrays(1, &smaa->vao);
    glBindVertexArray(smaa->vao);

    glGenBuffers(1, &smaa->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, smaa->vbo);
    float vertices[] = {
	0, 0,
	0, 1,
	1, 1,
	0, 0,
	1, 1,
	1, 0
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    checkGl();

    fprintf(stderr, "with_smaa: smaa_init() success.\n");
    fprintf(stderr, "with_smaa: initial size: %dx%d\n", width, height);

    smaa->initialized = 1;
}

internal
void smaa_update(SMAA *smaa)
{
    if(smaa->incompatible) {
	return;
    }

    //fprintf(stdout, "smaa_update\n");

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0, 0, 0, 0);


    GLint size[4];
    glGetIntegerv(GL_VIEWPORT, size);
    int width = size[2], height = size[3];

    GLfloat rt_metrics[4] = {
	1.0f / width, 1.0f / height, width, height
    };

    if(width != smaa->old_width || height != smaa->old_height) {
	fprintf(stderr, "with_smaa: resizing %dx%d -> %dx%d\n", smaa->old_width, smaa->old_height,
		width, height);
	smaa_resize_fbo_texture(smaa->edge_tex, width, height);
	smaa_resize_fbo_texture(smaa->blend_tex, width, height);
	smaa->old_width = width;
	smaa->old_height = height;
	checkGl();
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, smaa->color_tex);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, width, height, 0);

    // I don't really have any better idea on how to get this right.
    // Except the neighborhood blending pass no pass should use sRGB reads/writes.
    // So I just copy it again below, just with sRGB flag set.

    // SMAA edge detection pass
    // Reads rendered image from smaa->color_tex and renders into smaa->edge_fbo+tex.
    glUseProgram(smaa->edge_shader);
    glBindVertexArray(smaa->vao);

    glUniform1i(glGetUniformLocation(smaa->edge_shader, "in_tex"), 0);
    glUniform4fv(glGetUniformLocation(smaa->edge_shader, "in_rt_metrics"), 1, rt_metrics);

    glBindFramebuffer(GL_FRAMEBUFFER, smaa->edge_fbo);
    glClear(GL_COLOR_BUFFER_BIT);

    GLenum db = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &db);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // SMAA blending weight calculation pass
    // Reads edges from smaa->edge_tex and renders into smaa->blend_fbo+tex.
    glBindFramebuffer(GL_FRAMEBUFFER, smaa->blend_fbo);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, smaa->edge_tex);
    glUseProgram(smaa->blend_shader);
    glUniform1i(glGetUniformLocation(smaa->blend_shader, "in_tex"), 0);
    glUniform1i(glGetUniformLocation(smaa->blend_shader, "in_area_tex"), 1);
    glUniform1i(glGetUniformLocation(smaa->blend_shader, "in_search_tex"), 2);
    glUniform4fv(glGetUniformLocation(smaa->blend_shader, "in_rt_metrics"), 1, rt_metrics);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, smaa->edge_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, smaa->area_tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, smaa->search_tex);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    /*
    // To see if edge detection works corretcly
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, smaa->blend_fbo);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return;
    */

    // SMAA neighborhood blending pass
    // Reads blending weights from smaa->blend_tex, rendered image from smaa->color_tex
    // and renders into the standard framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(smaa->neighbor_shader);

    glUniform1i(glGetUniformLocation(smaa->neighbor_shader, "in_tex"), 0);
    glUniform1i(glGetUniformLocation(smaa->neighbor_shader, "in_blend_tex"), 1);
    glUniform4fv(glGetUniformLocation(smaa->neighbor_shader, "in_rt_metrics"), 1, rt_metrics);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, smaa->color_tex);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 0, 0, width, height, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, smaa->blend_tex);

    db = GL_BACK_LEFT;
    glDrawBuffers(1, &db);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
