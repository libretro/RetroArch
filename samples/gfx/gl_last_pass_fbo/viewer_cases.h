/* The shipping viewer chain against the same GL/backend script as the
 * core chain. No core framebuffer or history may become a viewer input. */
static unsigned viewer_pass, viewer_dims, viewer_calls, viewer_colour_calls;
static unsigned viewer_history_slots = 2;
static unsigned viewer_frame = 42, viewer_source_dims = VIDEO_SCALE_PACK(320, 240);

static bool viewer_colour(void *data, const float *rows)
{
   (void)data;
   viewer_colour_calls += rows ? 1 : 2;
   return true;
}
static GLuint viewer_feedback, viewer_previous;
static GLuint viewer_expected[2];
static bool viewer_check_history;
static gl_media_shader_t *viewer_pixel_state;
static GLuint viewer_bound_fbo;
static unsigned viewer_bound_draws, viewer_version = 1;
static unsigned viewer_pixels[1024], viewer_expected_pixels[2];

static void viewer_bind_framebuffer(GLenum target, GLuint id)
{
   unsigned i;
   if (viewer_pixel_state && gl11_draws != viewer_bound_draws)
      for (i = 0; i < viewer_pixel_state->history_count; i++)
         if (viewer_pixel_state->history[i].fbo == viewer_bound_fbo)
         {
            GLuint tex = viewer_pixel_state->history[i].texture;
            CHECK(tex < 1024, "scripted history pixel storage is bounded");
            if (tex < 1024) viewer_pixels[tex] = viewer_version;
         }
   viewer_bound_fbo = id;
   viewer_bound_draws = gl11_draws;
   fake_bind_framebuffer(target, id);
}

static void viewer_use(void *driver, void *data, unsigned pass, bool active)
{
   (void)driver; (void)data; (void)active;
   viewer_pass = pass;
}

static void viewer_source(void *data, unsigned dims)
{
   (void)data;
   viewer_dims = dims;
}

static unsigned viewer_history(void *data)
{
   (void)data;
   return viewer_history_slots;
}

static bool viewer_coords(void *data, const struct video_coords *coords)
{
   (void)data;
   return coords->vertices == 4;
}

static bool viewer_mvp(void *data, const void *matrix)
{
   (void)data; (void)matrix;
   return true;
}

static void viewer_params(void *data, void *shader_data)
{
   video_shader_ctx_params_t *p = (video_shader_ctx_params_t*)data;
   const struct video_tex_info *original = (const struct video_tex_info*)p->info;
   const struct video_tex_info *feedback = (const struct video_tex_info*)p->feedback_info;
   const struct video_tex_info *previous = (const struct video_tex_info*)p->prev_info;
   (void)shader_data;
   CHECK(original->tex == 999, "viewer borrows its image, never the core texture");
   CHECK(viewer_dims == viewer_source_dims, "viewer source semantics enabled");
   CHECK(p->fbo_info_cnt == viewer_pass - 1, "only completed passes are visible");
   CHECK(p->frame_counter == viewer_frame, "frame count travels with the video frame");
   if (feedback_pass >= 0)
      CHECK(feedback->tex != 0, "last-pass feedback available to the first pass");
   CHECK(previous[0].tex && (viewer_history_slots == 1 || previous[1].tex),
         "viewer owns its history");
   if (viewer_check_history)
   {
      CHECK(previous[0].tex == viewer_expected[0], "Prev matches the prior presentation");
      CHECK(previous[1].tex == viewer_expected[1], "Prev1 preserves the older presentation");
      CHECK(viewer_pixels[previous[0].tex] == viewer_expected_pixels[0],
            "Prev pixels survive subsequent uploads");
      CHECK(viewer_pixels[previous[1].tex] == viewer_expected_pixels[1],
            "older Prev pixels remain immutable while referenced");
   }
   if (viewer_pass == 1)
   {
      viewer_feedback = feedback->tex;
      viewer_previous = previous[0].tex;
   }
   viewer_calls++;
}

static void viewer_active_texture(GLenum unit) { (void)unit; }

static int viewer_cases(void)
{
   shader_backend_t backend;
   gl_media_shader_t *state = NULL;
   GLuint feedback, previous, first;
   gl_fakes_install();
   __rglgen_glBindFramebuffer = viewer_bind_framebuffer;
   __rglgen_glActiveTexture = viewer_active_texture;
   backend_init(&backend);
   backend.use = viewer_use;
   backend.set_params = viewer_params;
   backend.set_coords = viewer_coords;
   backend.set_mvp = viewer_mvp;
   backend.set_media_source = viewer_source;
   backend.get_prev_textures = viewer_history;
   num_passes = 2;
   feedback_pass = 1;
   scale_source(&scales[0], 2.0f);
   scale_viewport(&scales[1], 0.5f, 1.0f);
   CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
         VIDEO_SCALE_PACK(320, 240), 0, VIDEO_SCALE_PACK(800, 600), 42, NULL, false),
         "multipass viewer rendered");
   CHECK(viewer_calls == 2 && !viewer_dims, "core semantics restored");
   CHECK(state->pass[0].dims == VIDEO_SCALE_PACK(640, 480), "input scaling");
   CHECK(state->feedback.dims == VIDEO_SCALE_PACK(400, 600), "viewport scaling");
   first = state->pass[0].texture;
   feedback = viewer_feedback;
   previous = viewer_previous;
   CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
         VIDEO_SCALE_PACK(320, 240), 0, VIDEO_SCALE_PACK(800, 600), 42, NULL, false),
         "second viewer frame rendered");
   CHECK(state->pass[0].texture == first, "stable frame reuses resources");
   CHECK(feedback != viewer_feedback, "feedback advances independently");
   CHECK(previous != viewer_previous, "history advances independently");
   feedback_pass = -1;
   scale_viewport(&scales[1], 1.0f, 1.0f);
   CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
         VIDEO_SCALE_PACK(320, 240), 0, VIDEO_SCALE_PACK(800, 600), 42, NULL, false),
         "unscaled final pass rendered directly");
   CHECK(!state->pass[1].texture && !state->pass[1].fbo,
         "direct final pass needs no texture copy or framebuffer");
   {
      static const float rows[12] = {1, 0, 0, 1, 0, 1, 0, 0.4166667f, 0, 0, 1, 0};
      backend.set_media_colour = viewer_colour;
      CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
            VIDEO_SCALE_PACK(320, 240), 0, VIDEO_SCALE_PACK(800, 600), 42, rows, false),
            "HDR viewer borrows the same source");
      CHECK(viewer_colour_calls == 3, "private colour programs restored after the draw");
      CHECK((state->history[0].flags & GL_MEDIA_LINEAR_HDR)
            && (state->pass[0].flags & GL_MEDIA_LINEAR_HDR),
            "existing HDR history and pass targets keep signed dynamic range");
      CHECK(!state->pass[1].texture && !state->pass[1].fbo,
            "HDR normalization adds no final target or colour conversion draw");
      backend.set_media_colour = NULL;
      CHECK(!VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
            VIDEO_SCALE_PACK(320, 240), 0, VIDEO_SCALE_PACK(800, 600), 42, rows, false),
            "unsupported colour variants leave the plain HDR viewer available");
   }
   {
      unsigned i, draws, calls, names;
      backend.set_media_colour = viewer_colour;
      for (i = 0; i < 5; i++)
      {
         draws = gl11_draws;
         calls = viewer_calls;
         names = gl11_next_name;
         viewer_frame++;
         CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
               viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600), viewer_frame, NULL, true),
               "unchanged managed image rendered");
         CHECK(gl11_draws - draws == (i == 0 ? 3u : 2u),
               "one immutable snapshot serves repeated presentations");
         CHECK(viewer_calls - calls == 2, "shader animation continues without history copies");
         if (i)
            CHECK(gl11_next_name == names, "sharing history allocates no additional GPU resources");
      }
      viewer_uploaded(state);
      CHECK(!state->history_copies, "in-place upload invalidates the cached source");
      draws = gl11_draws;
      CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
            viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600), viewer_frame, NULL, true),
            "updated image rendered");
      CHECK(gl11_draws - draws == 3, "changed content takes a new history snapshot");
      {
         unsigned version, repeat, older_pixels;
         GLuint snapshot, older;
         viewer_expected[0] = state->history[state->history_order[0]].texture;
         viewer_expected[1] = state->history[state->history_order[1]].texture;
         viewer_pixel_state = state;
         viewer_expected_pixels[0] = viewer_expected_pixels[1] = viewer_version;
         viewer_pixels[viewer_expected[0]] = viewer_pixels[viewer_expected[1]] = viewer_version;
         viewer_check_history = true;
         for (version = 0; version < 16; version++)
         {
            viewer_version++;
            viewer_uploaded(state);
            for (repeat = 0; repeat < (version % 4) + 1; repeat++)
            {
               draws = gl11_draws;
               older = viewer_expected[0];
               older_pixels = viewer_expected_pixels[0];
               viewer_frame++;
               CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
                     viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600),
                     viewer_frame, NULL, true), "moving frame and repeated presentations rendered");
               snapshot = state->history[state->history_order[0]].texture;
               CHECK(gl11_draws - draws == (repeat ? 2u : 3u),
                     "each upload takes exactly one snapshot regardless of display cadence");
               if (!repeat)
                  CHECK(snapshot != older, "new pixels cannot overwrite the newest retained frame");
               else
                  CHECK(snapshot == older, "repeated pixels share their immutable snapshot");
               viewer_expected_pixels[1] = older_pixels;
               viewer_expected_pixels[0] = viewer_version;
               viewer_expected[1] = older;
               viewer_expected[0] = snapshot;
            }
         }
         viewer_check_history = false;
         viewer_pixel_state = NULL;
      }
      viewer_unloaded(state);
      CHECK(!state->history_copies, "deletion invalidates reused GL names");
      for (i = 0; i < 4; i++)
      {
         draws = gl11_draws;
         CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
               viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600), viewer_frame, NULL, false),
               "external source rendered");
         CHECK(gl11_draws - draws == 3, "unknown owners always snapshot their texture");
      }
      feedback_pass = 1;
      for (i = 0; i < 4; i++)
      {
         static const float rows[12] = {1, 0, 0, 1, 0, 1, 0, 0.4166667f, 0, 0, 1, 0};
         draws = gl11_draws;
         previous = viewer_previous;
         feedback = viewer_feedback;
         CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
               viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600), viewer_frame, rows, true),
               "unchanged HDR image rendered with feedback");
         CHECK(gl11_draws - draws == (i == 0 ? 4u : 3u), "HDR history also stops copying");
         CHECK(feedback != viewer_feedback, "shader feedback continues");
         if (i > 1)
            CHECK(previous == viewer_previous, "repeated Prev entries share their snapshot");
      }
      {
         static const float changed_rows[12] = {1, 0, 0, 2, 0, 1, 0, 0.4166667f, 0, 0, 1, 0};
         draws = gl11_draws;
         CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
               viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600), viewer_frame,
               changed_rows, true), "HDR colour settings changed without an upload");
         CHECK(gl11_draws - draws == 4, "colour changes restart history snapshots");
      }
      viewer_source_dims = VIDEO_SCALE_PACK(160, 120);
      draws = gl11_draws;
      CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
            viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600), viewer_frame, NULL, true),
            "resized image rendered");
      CHECK(gl11_draws - draws == 4, "resize or colour-format changes restart snapshots");
   }
   for (viewer_history_slots = 1; viewer_history_slots < GFX_MAX_TEXTURES;
         viewer_history_slots++)
   {
      unsigned draw, repeat, names;
      for (repeat = 0; repeat < viewer_history_slots + 3; repeat++)
      {
         draw = gl11_draws;
         names = gl11_next_name;
         CHECK(VIEWER_DRAW(&state, NULL, &backend, &backend, 999,
               viewer_source_dims, 0, VIDEO_SCALE_PACK(800, 600), viewer_frame,
               NULL, true), "changed history depth rendered");
         CHECK(gl11_draws - draw == (repeat ? 3u : 4u),
               "every supported history depth shares repeated snapshots");
         if (repeat)
            CHECK(gl11_next_name == names, "steady history needs no new resources");
      }
   }
   VIEWER_FREE(&state);
   CHECK(!state, "preset replacement releases viewer resources");
   printf("viewer multipass/history/feedback checks passed\n");
   return fails != 0;
}
