   if (!list)
      return;

   list->menu->need_texture_reload = true;
   for (i = 0; i < list->size; i++)
   {
      menu_list_entry_t *entry = &list->list[i];