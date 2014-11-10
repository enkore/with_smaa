
// This is a Linux-only utility anyway so we don't bother with
// getprocaddress for everything.
#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glext.h>

#define public __attribute__ ((visibility ("default")))

// As a shim library we should only export symbols we want to override
#define internal  __attribute__ ((visibility ("hidden")))

typedef struct SMAAState {
    GLint vao, program, texture, depth, blending, srgb;
    GLfloat clear_color[4];
    GLint textures[3];
} SMAAState;

typedef struct SMAA {
    int initialized;
    int incompatible;
    int legacy;

    GLuint area_tex;
    GLuint search_tex;

    // Contains a copy of the original color buffer
    GLuint color_tex;
    // Target of the edge pass
    GLuint edge_tex;
    // Target of the blend pass
    GLuint blend_tex;

    GLuint edge_fbo;
    GLuint blend_fbo;

    GLuint edge_shader;
    GLuint blend_shader;
    GLuint neighbor_shader;

    GLuint vao;
    GLuint vbo;

    int old_width;
    int old_height;

    SMAAState state;
} SMAA;

internal SMAA *smaa_create();

internal void smaa_update(SMAA *smaa);

