import os
import ycm_core
# Defaults, if no database can be found.
defaultsc = [
        'gcc',
        '-Wno-long-long',
        '-Wno-variadic-macros',
        '-pthread'
        '-std=c99',
        ]
defaultscpp = [
        'c'
        '-Wno-long-long',
        '-Wno-variadic-macros',
        '-pthread'
        '-std=c99',
        ]

# Things that must be included.
entered_flags = ['-fdelayed-template-parsing']

#
def DirectoryOfThisScript():
    return os.path.dirname( os.path.abspath( __file__ ) )

# Find all compilation databases in the build directory and load them.

def MakeRelativePathsInFlagsAbsolute( flags, working_directory ):
  if not working_directory:
    return list( flags )
  new_flags = []
  make_next_absolute = False
  path_flags = [ '-isystem', '-I', '-iquote', '--sysroot=' ]
  for flag in flags:
    new_flag = flag

    if make_next_absolute:
      make_next_absolute = False
      if not flag.startswith( '/' ):
        new_flag = os.path.join( working_directory, flag )

    for path_flag in path_flags:
      if flag == path_flag:
        make_next_absolute = True
        break

      if flag.startswith( path_flag ):
        path = flag[ len( path_flag ): ]
        new_flag = path_flag + os.path.join( working_directory, path )
        break

    if new_flag:
      new_flags.append( new_flag )
  return new_flags


def FlagsForFile( filename ):
    # Search through all parent directories for a directory named 'build'
    databases = []
    path_focus = os.path.dirname(filename)
    while len(path_focus) > 1:
        for f in os.listdir(path_focus):
            compilation_database_folder = path_focus + "/" + f
            for r,d,f in os.walk(compilation_database_folder):
                for files in f:
                    if files == 'compile_commands.json':
                        databases += [ycm_core.CompilationDatabase( r )]
        path_focus = os.path.dirname(os.path.dirname(path_focus))

    # Use a header's source file database for completion.
    filetype_flags = []
    if filename.endswith(".h"):
        for f in os.listdir(os.path.dirname(filename)):
            if filename.replace(".h",".cpp").find(f) != -1:
                filename = filename.replace(".h",".cpp")
                break
            if filename.replace(".h",".c").find(f) != -1:
                filename = filename.replace(".h",".c")
                break
    elif filename.endswith(".hpp"):
        for f in os.listdir(os.path.dirname(filename)):
            if filename.replace(".hpp",".cpp").find(f) != -1:
                filename = filename.replace(".hpp",".cpp")
                break

    # Get the compile commands
    final_flags = []
    # If possible, from the database.
    if len(databases) > 0:
        for database in databases:
            compilation_info = database.GetCompilationInfoForFile( filename )
            fromfile_flags = MakeRelativePathsInFlagsAbsolute(
            compilation_info.compiler_flags_,
            compilation_info.compiler_working_dir_ )
            final_flags += fromfile_flags

    # If not, set some sane defaults.
    else:
        relative_to = DirectoryOfThisScript()
        final_flags += MakeRelativePathsInFlagsAbsolute( flags, relative_to )
    if not final_flags:
        if filename.endswith(".c"):
            final_flags = defaultsc
        elif filename.endswith(".cpp"):
            final_flags = defaultscpp

    # This allows header files to be parsed according to their parent source
    final_flags = filetype_flags + final_flags

    # For things that must be included regardless:
    final_flags += entered_flags
    return {
        'flags': final_flags,
        'do_cache': True
    }
