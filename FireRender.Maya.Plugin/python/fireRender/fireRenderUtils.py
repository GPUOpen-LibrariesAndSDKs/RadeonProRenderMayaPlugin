import os
import glob
import maya.cmds

def setShaderCachePathEnvironment(version):
    projectPath = maya.cmds.workspace(q = True, rd = True) + "cache/"
    frCacheVersionPath = "fr_render/" + version + "/"
    frcachepath = projectPath + frCacheVersionPath
    if not os.path.exists(projectPath + frCacheVersionPath):
        os.makedirs(projectPath + frCacheVersionPath)
        frcachepath = projectPath + frCacheVersionPath

    if not os.path.exists(frcachepath):
        if "Temp" in os.environ:
            os.makedirs(os.environ["Temp"].decode("utf8") + "/" + frCacheVersionPath)
            frcachepath = os.environ["Temp"].decode("utf8") + "/" + frCacheVersionPath

    if not os.path.exists(frcachepath):
        if "TEMP" in os.environ:
            os.makedirs(os.environ["TEMP"].decode("utf8") + "/" + frCacheVersionPath)
            frcachepath = os.environ["TEMP"].decode("utf8") + "/" + frCacheVersionPath

    os.environ["FR_SHADER_CACHE_PATH"] = frcachepath.encode("utf8")