Rigs of Rods - v0.4.0.7 retro edition
=====================================

INTRO
-----

This branch is based on v0.4.0.7 source code from year 2013
and updated to use recent components (OGRE 1.11.6, MyGUI 3.4.0).

The purpose is to have a working instance of the original
truck parser/spawner logic, for diagnosing defects
in the current upstream parser.

EXTRAS
------

RoR.cfg setting "diag_actor_dump" (bool, default true) writes 2 files to /logs:
 1. truckname_dump_raw.txt ~ original data from truck file
 2. truckname_dump_recalc.txt ~ after recalc. masses, pressurizing tires and other adjustments
The output is equivalent to the same setting in upstream,
it's intended for comparsion using diff tool.

RoR.cfg setting "stable_physics_tick" (bool, default true) fixates physics tick rate to 2khz,
same as used in upstream repository.
 
BUILDING
--------

Only tested on Windows10 + MSVC2019/MSVC2022, needs a lot of manual adjustments.

1. run cmake; build outside the source tree

2. set ROR_DEPENDENCIES dir; you'll need to manually assemble the includes/libs to the expected layout:
    includes/x64:
        AngelScript\
        moFileReader\
        MyGUI\
        OGRE\
        OIS\
        OpenAL\
        PagedGeometry\
    libs/x64:
        moFileReader\
        MyGUI\ ~ MyGUIEngine.lib + MyGUI.OgrePlatform.lib (from conan), freetype.lib (from latest conan OGREDeps)
        OGRE\
        OIS\
        OpenAL\
        PagedGeometry\
        pthread\ ~ pthreadVC3.lib (from vcpkg)
    
   
3. generate visual studio project

4. select build config: RelWithDebInfo
    
RUNNING
-------
    
To effectivelly compare debug dumps using diff-tool, you must spawn the same vehicle at exactly the same position.

Command line hints:
```
# Git master:
RoR -terrain simple2 -truck semi.truck -enter -truckspawnpos "0 0 0"

# Git retro-0407: 
RoR -terrain simple2 -truck semi.truck -enter -pos "0 0 0"
```

LICENSE
-------

Copyright 2005-2013 Pierre-Michel Ricordel
Copyright 2007-2013 Thomas Fischer

For more information, see http://www.rigsofrods.com/

Rigs of Rods is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3, as 
published by the Free Software Foundation.

Rigs of Rods is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Rigs of Rods.  If not, see <http://www.gnu.org/licenses/>.

