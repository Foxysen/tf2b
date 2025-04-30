#!/usr/bin/env python3

# Clangd is a great Language Server parser to use with Kate for C++
# But it's hard to configure. We could use compile_commands but it doesn't include headers and it fails to approximate
# So we must include a configuration file at src root
# BUT the .clangd config file does not support having command be relative to the .clangd file
# So we have to use this script to generate absolute paths on user machine

# Ignores folders that start with '.' and '_'

# Fun fact: output order is non-determenistic due to usage of set

# Make sure that no folder in "src" has any link that goes outside the project
# Otherwise they will all be added to .clangd file, making it slower to use

import os
from pathlib import Path

EXTRA_OPTIONS = "-DCLIENT_DLL, -DGAME_DLL, -DVPC, -DSOURCESDK, -DSOURCE_HAS_FREETYPE, -DNDEBUG, -DGNUC, -DPOSIX, -DCOMPILER_GCC, -D_DLL_EXT=.so, -D_LINUX, -DLINUX, -DPLATFORM_64BITS, -DPOSIX, -D_POSIX, -DDLLNAME=server, -DBINK_VIDEO, -DGL_GLEXT_PROTOTYPES, -DDX_TO_GL_ABSTRACTION, -DUSE_SDL, -DDEV_BUILD, -DFRAME_POINTER_OMISSION_DISABLED, -DREPLAY_ENABLED, -DVECTOR, -DVERSION_SAFE_STEAM_API_INTERFACES, -DPROTECTED_THINGS_ENABLE, -Dsprintf=use_Q_snprintf_instead_of_sprintf, -Dstrncpy=use_Q_strncpy_instead, -D_snprintf=use_Q_snprintf_instead, -DINCLUDED_STEAM2_USERID_STRUCTS, -DSWDS, -DUSES_ECON_ITEMS, -DUSE_NAV_MESH, -DTF_DLL, -DENABLE_GC_MATCHMAKING, -DGLOWS_ENABLE, -DUSE_DYNAMIC_ASSET_LOADING, -DNEXT_BOT, -DDISABLE_GC_CONNECTION, -DDISABLE_IN_SOURCESDK, -DINVENTORY_VIA_WEBAPI, -D_DLL_EXT=.so, -D_DLL_PREFIX=lib, -D_EXTERNAL_DLL_EXT=.so, -D_LINUX=1, -D_LINUXSTEAMRT64=1, -D_POSIX=1, -DLINUX=1, -DLINUX64=1, -DLINUXSTEAMRT64=1, -DPOSIX=1, -DPROJECTDIR=/my_mod/src/game/server, -DSYSTEM_TRIPLE=x86_64-linux-gnu, -DVPCGAME=tf, -DVPCGAMECAPS=TF"


# optimization: 'game' folders must be stored separately to go first in the list because most of dev time will be spent in game layer.
m_Paths_game = set()
m_Paths_src = set()

# assume this is src path
SRC_PATH = Path(__file__).resolve().parent
GAME_PATH = SRC_PATH.joinpath("game")


# pretty much a filter function
def is_Path_valid(P):
    #if P == SRC_PATH:
    #    return False
    if P in m_Paths_game or P in m_Paths_src:
        return False
    name = str(P.name)
    if len(name) >= 1:
        if name[0] == '.' or name[0] == '_':
            return False
    return True


def walk_from_Path(P, into_set):
    for root, dirs, UNUSED in os.walk(P):
        P_root = Path(root)
        if is_Path_valid(P_root):
            into_set.add(P_root)

        for d in dirs:
            if not is_Path_valid(Path(d)):
                dirs.remove(d)

walk_from_Path(GAME_PATH, m_Paths_game)
walk_from_Path(SRC_PATH, m_Paths_src) # Since we also ignore dir that are already put into game paths set, this should not tread on m_Paths_game


try:
    with open(SRC_PATH.joinpath(".clangd"), 'w') as f:
        IMPRINT_TEXT = "# Version 1\n\nCompileFlags:\n\tAdd: [{0}, {1}]"
        I_PREFIX = "-I"
        str_includes = ""
        for P in m_Paths_game:
            str_includes += I_PREFIX + str(P) + ", ";
        for P in m_Paths_src:
            str_includes += I_PREFIX + str(P) + ", ";

        if len(str_includes) >= 2:
            str_includes = str_includes[0:-2] # remove the last ", "
        f.write(IMPRINT_TEXT.format(EXTRA_OPTIONS, str_includes))


except OSError:
    print("Cannot write .clangd file for some reason.", file=sys.stderr)


# Write Version 1 in comment of .clangd
