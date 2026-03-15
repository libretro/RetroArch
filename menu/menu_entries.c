   if (!entries)
      return;

   entries->menu->need_texture_reload = true;
   for (i = 0; i < entries->size; i++)
   {
      menu_entry_t *entry = &entries->list[i];