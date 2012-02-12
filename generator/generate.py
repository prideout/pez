#!/usr/bin/python

import re
import os

def Substitute(lines, mapping):
    out = []
    for line in lines:
        for key in mapping.keys():
            line = re.sub(key, mapping[key], line)
        out.append(line)
    return out

def Generate(
        PlatformPrefix, 
        MainLoopPrefix,
        EnumPrefix,
        StructPrefix,
        OutputFolder,
        MainLoop = True,
        DropHandler = True,
        MouseHandler = True):

    GlswPrefix = PlatformPrefix
    GlboPrefix = PlatformPrefix
    GeneratedSource = PlatformPrefix + ".c"
    GeneratedHeader = PlatformPrefix + ".h"
    
    sourcedir = os.path.dirname(__file__)
    def qopen(X): return open(os.path.join(sourcedir, X), 'r')

    bstrlibc = qopen('bstrlib.c').readlines()
    bstrlibh = qopen('bstrlib.h').readlines()
    glswh = qopen('glsw.h').readlines()
    glswc = qopen('glsw.c').readlines()
    glboh = qopen('glbo.h').readlines()
    glboc = qopen('glbo.c').readlines()
    glewh = qopen('glew.h').readlines()
    glewc = qopen('glew.c').readlines()
    glxewh = qopen('glxew.h').readlines()
    wglewh = qopen('wglew.h').readlines()
    lzfxh = qopen('lzfx.h').readlines()
    lzfxc = qopen('lzfx.c').readlines()
    platformh = qopen('platform.h').readlines()
    platformwinc = qopen('platform.win.c').readlines()
    platformx11c = qopen('platform.x11.c').readlines()

    pdict = dict(ZepConfig = StructPrefix + 'Config')
    platformwinc = Substitute(platformwinc, pdict)
    platformx11c = Substitute(platformx11c, pdict)
    platformh = Substitute(platformh, pdict)

    pdict = dict(
        Zep = MainLoopPrefix,
        zep = PlatformPrefix,
        ZEP = EnumPrefix)

    platformwinc = Substitute(platformwinc, pdict)
    platformx11c = Substitute(platformx11c, pdict)
    platformh = Substitute(platformh, pdict)
    glswc = Substitute(glswc, dict(glsw = GlswPrefix))
    glswh = Substitute(glswh, dict(glsw = GlswPrefix))
    glboc = Substitute(glboc, dict(glbo = GlboPrefix, Glbo = StructPrefix))
    glboh = Substitute(glboh, dict(glbo = GlboPrefix, Glbo = StructPrefix))

    pezh = open(OutputFolder + GeneratedHeader, 'w')
    pezh.write('#pragma once\n')
    pezh.write('\n')
    pezh.write('#ifdef __cplusplus\n')
    pezh.write('extern "C" {\n')
    pezh.write('#endif\n')
    if MainLoop: pezh.write('\n#define ' + EnumPrefix + '_MAINLOOP 1\n')
    if MouseHandler: pezh.write('\n#define ' + EnumPrefix + '_MOUSE_HANDLER 1\n')
    if DropHandler: pezh.write('\n#define ' + EnumPrefix + '_DROP_HANDLER 1\n')
    pezh.writelines(platformh)
    pezh.writelines(glswh)
    pezh.writelines(glboh)
    pezh.write('#ifdef __cplusplus\n')
    pezh.write('}\n')
    pezh.write('#endif\n')

    pezc = open(OutputFolder + GeneratedSource, 'w')
    pezc.write('// This is an autogenerated Pez library.\n')
    pezc.write('#include "' + GeneratedHeader + '"\n')
    pezc.write("#ifdef WIN32\n")
    pezc.write("#define _WIN32_WINNT 0x0500\n")
    pezc.write("#define WINVER 0x0500\n")
    pezc.write("#include <windows.h>\n")
    pezc.write("#endif\n")
    pezc.writelines(bstrlibh)
    pezc.writelines(bstrlibc)
    pezc.writelines(glswc)
    pezc.writelines(lzfxh)
    pezc.writelines(lzfxc)
    pezc.writelines(glboc)
    pezc.write("#ifdef WIN32\n")
    pezc.writelines(platformwinc)
    pezc.write("#else\n")
    pezc.writelines(platformx11c)
    pezc.write("#endif\n")

    glewcfile = open(OutputFolder + "glew.c", 'w')
    glewcfile.writelines(glewc)

    glewhfile = open(OutputFolder + "glew.h", 'w')
    glewhfile.write('#define GLEW_STATIC 1\n')
    glewhfile.writelines(glewh)
    glewhfile.write("#ifdef WIN32\n")
    glewhfile.write("#define _WIN32_WINNT 0x0500\n")
    glewhfile.write("#define WINVER 0x0500\n")
    glewhfile.write("#include <windows.h>\n")
    glewhfile.writelines(wglewh)
    glewhfile.write("#else\n")
    glewhfile.writelines(glxewh)
    glewhfile.write("#endif\n")

if __name__ == "__main__":
    args = dict(
        PlatformPrefix = "pez",
        MainLoopPrefix = "Pez",
        EnumPrefix = "PEZ",
        StructPrefix = "Pez",
        OutputFolder = "../")
    Generate(**args)
