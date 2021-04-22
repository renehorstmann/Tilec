#include "mathc/float.h"
#include "r/render.h"
#include "r/program.h"
#include "r/ro_batchrefract.h"


static const vec4 VIEW_AABB_FULLSCREEN = {{0.5, 0.5, 0.5, 0.5}};


static void init_rects(rRect_s *instances, int num) {
    for (int i = 0; i < num; i++) {
        rRect_s *r = &instances[i];
        r->pose = mat4_eye();
        r->uv = mat4_eye();
        r->color = vec4_set(1);
    }
}

static int clamp_range(int i, int begin, int end) {
    if (i < begin)
        i = begin;
    if (i >= end)
        i = end - 1;
    return i;
}

RoBatchRefract ro_batchrefract_new_a(int num,
        const float *vp, const float *scale_ptr,
        rTexture tex_main_sink, rTexture tex_refraction_sink,
        Allocator_s alloc) {
    r_render_error_check("ro_batchrefract_newBEGIN");
    RoBatchRefract self;
    self.allocator = alloc;
    
    assume(num>0, "batch needs atleast 1 rect");
    self.rects = alloc.malloc(alloc, num * sizeof(rRect_s));
    assume(self.rects, "allocation failed");
    for(int i=0; i<num; i++) {
        self.rects[i] = r_rect_new();
    }

    self.num = num;
    self.vp = vp;
    self.scale = scale_ptr;
    self.view_aabb = &VIEW_AABB_FULLSCREEN.v0;

    self.program = r_program_new_file("res/r/batchrefract.glsl");
    const int loc_pose = 0;
    const int loc_uv = 4;
    const int loc_color = 8;
    const int loc_sprite = 9;

    self.tex_main = tex_main_sink;
    self.tex_refraction = tex_refraction_sink;
    self.owns_tex_main = true;
    self.owns_tex_refraction = true;
    
    self.tex_framebuffer_ptr = &r_render.framebuffer_tex;


    // vao scope
    {
        glGenVertexArrays(1, &self.vao);
        glBindVertexArray(self.vao);

        // vbo
        {
            glGenBuffers(1, &self.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, self.vbo);
            glBufferData(GL_ARRAY_BUFFER,
                         num * sizeof(rRect_s),
                         self.rects,
                         GL_STREAM_DRAW);

            glBindVertexArray(self.vao);

            // pose
            for (int c = 0; c < 4; c++) {
                int loc = loc_pose + c;
                glEnableVertexAttribArray(loc);
                glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                                      sizeof(rRect_s), (void *) (c * sizeof(vec4)));
                glVertexAttribDivisor(loc, 1);
            }

            // uv
            for (int c = 0; c < 4; c++) {
                int loc = loc_uv + c;
                glEnableVertexAttribArray(loc);
                glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                                      sizeof(rRect_s), (void *) (offsetof(rRect_s, uv) + c * sizeof(vec4)));
                glVertexAttribDivisor(loc, 1);
            }

            // color
            glEnableVertexAttribArray(loc_color);
            glVertexAttribPointer(loc_color, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(rRect_s),
                                  (void *) offsetof(rRect_s, color));
            glVertexAttribDivisor(loc_color, 1);
            
            // sprite
            glEnableVertexAttribArray(loc_sprite);
            glVertexAttribPointer(loc_sprite, 2, GL_FLOAT, GL_FALSE,
                                  sizeof(rRect_s),
                                  (void *) offsetof(rRect_s, sprite));
            glVertexAttribDivisor(loc_sprite, 1);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        glBindVertexArray(0);
    }
    
    r_render_error_check("ro_batchrefract_new");
    return self;
}


void ro_batchrefract_kill(RoBatchRefract *self) {
    self->allocator.free(self->allocator, self->rects);
    glDeleteProgram(self->program);
    glDeleteVertexArrays(1, &self->vao);
    glDeleteBuffers(1, &self->vbo);
    if (self->owns_tex_main)
        r_texture_kill(&self->tex_main);
    if (self->owns_tex_refraction)
        r_texture_kill(&self->tex_refraction);
    *self = (RoBatchRefract) {0};
}

void ro_batchrefract_update_sub(RoBatchRefract *self, int offset, int size) {
    r_render_error_check("ro_batchrefract_updateBEGIN");
    glBindBuffer(GL_ARRAY_BUFFER, self->vbo);

    offset = clamp_range(offset, 0, self->num);
    size = clamp_range(size, 1, self->num + 1);

    if (offset + size > self->num) {
        int to_end = self->num - offset;
        int from_start = size - to_end;
        glBufferSubData(GL_ARRAY_BUFFER,
                        offset * sizeof(rRect_s),
                        to_end * sizeof(rRect_s),
                        self->rects + offset);

        glBufferSubData(GL_ARRAY_BUFFER,
                        0,
                        from_start * sizeof(rRect_s),
                        self->rects);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER,
                        offset * sizeof(rRect_s),
                        size * sizeof(rRect_s),
                        self->rects + offset);

    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    r_render_error_check("ro_batchrefract_update");
}


void ro_batchrefract_render_sub(RoBatchRefract *self, int num) {
    r_render_error_check("ro_batchrefract_renderBEGIN");
    glUseProgram(self->program);

    // base
    glUniformMatrix4fv(glGetUniformLocation(self->program, "vp"),
                       1, GL_FALSE, self->vp);
                       
    vec2 sprites = vec2_cast_from_int(&self->tex_main.sprites.v0);
    glUniform2fv(glGetUniformLocation(self->program, "sprites"), 1, &sprites.v0);

    // fragment shader
    glUniform1f(glGetUniformLocation(self->program, "scale"), *self->scale);
    
    glUniform4fv(glGetUniformLocation(self->program, "view_aabb"), 1, self->view_aabb);

    glUniform1i(glGetUniformLocation(self->program, "tex_main"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, self->tex_main.tex);
    
    glUniform1i(glGetUniformLocation(self->program, "tex_refraction"), 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, self->tex_refraction.tex);
    
    glUniform1i(glGetUniformLocation(self->program, "tex_framebuffer"), 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, self->tex_framebuffer_ptr->tex);

    {
        glBindVertexArray(self->vao);
        // r_program_validate(self->program); // debug test
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, num);
        glBindVertexArray(0);
    }

    glUseProgram(0);
    r_render_error_check("ro_batchrefract_render");
}

void ro_batchrefract_set_texture_main(RoBatchRefract *self, rTexture tex_main_sink) {
    if (self->owns_tex_main)
        r_texture_kill(&self->tex_main);
    self->tex_main = tex_main_sink;
}

void ro_batchrefract_set_texture_refraction(RoBatchRefract *self, rTexture tex_refraction_sink){
    if (self->owns_tex_refraction)
        r_texture_kill(&self->tex_refraction);
    self->tex_refraction = tex_refraction_sink;
}