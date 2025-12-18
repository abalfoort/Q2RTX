cmake_minimum_required (VERSION 3.15)
cmake_policy(SET CMP0069 NEW)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/MP) # parallel build (use all cores, or as many as configured in VS)

    # ignore some compiler warnings
    add_compile_options(/wd4244 /wd4305) # possible loss of data/truncation (double to float etc; ignore)
    add_compile_options(/wd4018) # signed/unsigned mismatch
    add_compile_options(/wd4996) # 'function': was declared deprecated (like all that secure CRT stuff)
    # don't show me warnings for system headers, why the fuck isn't this default
    add_compile_options(/experimental:external /external:W0)
else() # GCC/clang/mingw
    # Enforce compiler flags:
    #  -fno-strict-aliasing -> Quake 2 is far away from strict aliasing
    #  -fwrapv              -> Make signed integer overflows defined
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing -fwrapv")
endif() # MSVC'S else-case

SET(SRC_CTF
    menu/menu.c
    monster/move.c
    player/client.c
    player/hud.c
    player/trail.c
    player/view.c
    player/weapon.c
    shared/shared.c
    g_ai.c
    g_chase.c
    g_cmds.c
    g_combat.c
    g_ctf.c
    g_func.c
    g_items.c
    g_main.c
    g_misc.c
    g_monster.c
    g_phys.c
    g_save.c
    g_spawn.c
    g_svcmds.c
    g_target.c
    g_trigger.c
    g_utils.c
    g_weapon.c
)

SET(HEADERS_CTF
    header/ctf.h
    header/game.h
    header/local.h
    header/menu.h
    header/shared.h
    monster/player.h
)

ADD_LIBRARY(ctf SHARED ${SRC_CTF} ${HEADERS_CTF})

SOURCE_GROUP("sources" FILES ${SRC_CTF})
SOURCE_GROUP("headers" FILES ${HEADERS_CTF})

IF (WIN32)
    TARGET_INCLUDE_DIRECTORIES(ctf PRIVATE ../VC/inc)
    TARGET_COMPILE_OPTIONS(ctf PRIVATE /wd4005 /wd4996)
ENDIF()

TARGET_INCLUDE_DIRECTORIES(ctf PRIVATE ../inc)

SET_TARGET_PROPERTIES(ctf
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/ctf"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/ctf"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/ctf"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/ctf"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/ctf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/ctf"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/ctf"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/ctf"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/ctf"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/ctf"
    PREFIX ""
    DEBUG_POSTFIX ""
)

IF(IS_64_BIT)
    IF(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        SET_TARGET_PROPERTIES(ctf
            PROPERTIES
            LIBRARY_OUTPUT_NAME "gameaarch64"
            RUNTIME_OUTPUT_NAME "gameaarch64"
        )
    ELSEIF(CMAKE_SYSTEM_PROCESSOR STREQUAL "ppc64le")
        SET_TARGET_PROPERTIES(ctf
            PROPERTIES
            LIBRARY_OUTPUT_NAME "gameppc64le"
            RUNTIME_OUTPUT_NAME "gameppc64le"
        )
    ELSE()
        SET_TARGET_PROPERTIES(ctf
            PROPERTIES
            LIBRARY_OUTPUT_NAME "gamex86_64"
            RUNTIME_OUTPUT_NAME "gamex86_64"
        )
    ENDIF()
ELSE()
    SET_TARGET_PROPERTIES(ctf
        PROPERTIES
        LIBRARY_OUTPUT_NAME "gamex86"
        RUNTIME_OUTPUT_NAME "gamex86"
    )
ENDIF()
