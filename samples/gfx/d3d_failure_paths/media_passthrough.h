/* These fixtures isolate staging/map failures with no viewer chain.
 * viewer_history.py executes the real version selector and publication. */
#include <assert.h>
static D3D11ShaderResourceView d3d11_media_writable(d3d11_video_t *video,
      d3d11_texture_t *texture)
{
   (void)video;
   assert(!texture->media_versions);
   return texture->view;
}
static void d3d11_media_publish(d3d11_texture_t *texture,
      D3D11ShaderResourceView view)
{
   assert(view == texture->view);
}
