   if (!texture)
      return;

   texture->menu->need_texture_reload = true;
   texture_image_free(&texture->image);
}

   if (!texture)
      return;

   if (texture->menu->need_texture_reload)
   {
      menu_xmb_clear_textures(texture->menu);
      texture->menu->need_texture_reload = false;
   }

   texture_image_load(&texture->image, path);
}