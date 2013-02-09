//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

static NSDictionary* boolean_setting(NSString* name, NSString* label, NSString* value)
{
   return [[NSDictionary alloc] initWithObjectsAndKeys:
            @"B", @"TYPE",
            name, @"NAME",
            label, @"LABEL",
            value, @"VALUE",
            nil];
}

@implementation settings_list
{
   NSArray* settings;
};

- (id)init
{
   self = [super initWithStyle:UITableViewStyleGrouped];

   settings = [NSArray arrayWithObjects:
      [NSArray arrayWithObjects:@"Video",
         boolean_setting(@"video_smooth", @"Smooth Video", @"true"),
         boolean_setting(@"video_crop_overscan", @"Crop Overscan", @"false"),
         nil],

      [NSArray arrayWithObjects:@"Audio",
         boolean_setting(@"audio_enable", @"Enable Output", @"true"),
         boolean_setting(@"audio_sync", @"Sync on Audio Stream", @"true"),
         boolean_setting(@"audio_rate_control", @"Adjust for Better Sync", @"true"),
         nil],
         
      [NSArray arrayWithObjects:@"Save States",
         boolean_setting(@"rewind_enable", @"Enable Rewinding", @"false"),
         boolean_setting(@"block_sram_overwrite", @"Disable SRAM on Load", @"false"),
         boolean_setting(@"savestate_auto_save", @"Auto Save on Exit", @"false"),
         boolean_setting(@"savestate_auto_load", @"Auto Load on Startup", @"true"),
         nil],
      nil
   ];
  
   [self setTitle:@"RetroArch Settings"];
   return self;
}

- (void)write_to_file
{
   const char* const sd = [RetroArch_iOS get].system_directory;
   char config_path[PATH_MAX];
   snprintf(config_path, PATH_MAX, "%s/retroarch.cfg", sd);
   config_path[PATH_MAX - 1] = 0;

   FILE* output = fopen(config_path, "w");

   for (int i = 0; i != [settings count]; i ++)
   {
      NSArray* group = [settings objectAtIndex:i];
   
      for (int j = 1; j < [group count]; j ++)
      {
         NSDictionary* setting = [group objectAtIndex:j];
         
         fprintf(output, "%s = %s\n", [[setting objectForKey:@"NAME"] UTF8String], [[setting objectForKey:@"VALUE"] UTF8String]);
      }
   }
   
   fclose(output);
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
   return [settings count];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return [[settings objectAtIndex:section] count] -1;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
   return [[settings objectAtIndex:section] objectAtIndex:0];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSDictionary* setting = [[settings objectAtIndex:indexPath.section] objectAtIndex:indexPath.row + 1];
   UITableViewCell* cell = nil;
   
   if ([[setting valueForKey:@"TYPE"] isEqualToString:@"B"])
   {
      cell = [self.tableView dequeueReusableCellWithIdentifier:@"boolean"];
   
      if (cell == nil)
      {
         cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"boolean"];
         cell.accessoryView = [[UISwitch alloc] init];
         [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
      }
      
      UISwitch* swt = (UISwitch*)cell.accessoryView;
      swt.on = [[setting valueForKey:@"VALUE"] isEqualToString:@"true"];
   }

   cell.textLabel.text = [setting valueForKey:@"LABEL"];

   return cell;
}

@end
