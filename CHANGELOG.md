# Version 3.2

## New Features:
- Subsurface Scattering and Volume shaders now work in RPR 2.0.  This allows the rendering of organic materials, such as skin, which absorb light into their interior.  Volume shaders can now also be used for simple fog boxes. 
- Viewport denoising and upscaling improves the interactivity and speed of Viewport rendering.  With the use of the Radeon Image Filter Library, this allows Radeon ProRender to render at half resolution faster, and then upscale to the full size of the Viewport.
- Deformation motion blur gives accurate motion blur to objects that are being deformed, for example, a flag flapping in the wind or a ball squashing.
- The new RPR Toon Shader has been added.  This enables cartoon-style shading for a non-photorealistic look.  Toon shaders can be used in a “simple” mode for just setting a color or a gradient of different colors for shadow vs lit areas of the object.
- Support for Maya 2022 has been added.
- Mesh export has been optimized for better performance.  In general, all scenes should synchronize faster; besides, meshes with per-face material shaders attached have been optimized.

## Issues Fixed:
- The Camera Normal AOV has been enabled for .rpr file export.
- The selected renderer quality mode is exported with a .rpr config file for offline rendering.
- When the denoiser was enabled and an AOV selected to view in IPR mode, an incorrect AOV was displayed ― fixed.
- A crash that could occur if Maya passed a zero-sized mesh has been fixed.
- nHair visibility flags are added for setting the ray visibility on hair.  By default, Maya makes hair invisible to refraction and reflection, which these new settings override.
- An issue that could cause missing geometry in scenes with multiple references to the same file ― fixed.
- UVs and edge creases of smoothed meshes were not being respected correctly ― fixed.
- Maya could hang when the user double-clicked the IPR button ― fixed.
- Overbright edges of objects with Uber shaders in metalness mode ― fixed.
- Shaders with high roughness could have artifacts with reflection or refraction ― fixed.

## Known Issues:
- In RPR 2.0, heterogenous volumes, smoke and fire simulations or VDB files are not yet supported.
- Subsurface scattering and volume shader are currently disabled on macOS due to long compile times.
- Some AOVs may have artifacts on AMD cards with drivers earlier than 21.6.1

	
# Version 3.1

## New Features:
 - Support for AMD Radeon™ RX 6700 XT graphics cards has been added.
 - Significant new features have been implemented in the AOV and compositing workflow for the plug-in:
 - Cryptomatte AOVs make it easy to filter objects by name or material when using compositors that support Cryptomatte.
 - Users can set a Material ID on shader nodes to use it with the MaterialID AOV.
 - The Camera Normal AOV is now available.
 - A 16-bit Machine Learning Denoiser option is now available to provide for less memory usage and greater speed.

## Issues Fixed:
 - Various fixes have been made to MASH instances:
 - Groups of objects can now be used for instancing;
 - Instance motion blur could be incorrect in certain cases — fixed;  
 - Mash Dynamic Nodes are now supported.
 - Denoisers are now supported for RPR 2.0 for the Production Render and IPR. Earlier, denoisers were only supported for RPR 1.0 (now the legacy engine).
 - Contour rendering not updating during IPR rendering — fixed.
 - Fixes and enhancements in .rpr file exporting:
 - Texture cache can now be used to provide for faster export of scenes;
 - GLTF file export includes an option to bake PBR images;
 - The current selected render layer is restored after exporting.
 - Undoing” deletion of an RPR Light would not work — fixed.
 - Motion blur for alembic GPU caches was not supported — fixed.
 - Hardware preference changes were not saved before starting a batch render — fixed.
 - Smoothed meshes could have the wrong geometry or missing textures — fixed.
 - If you animated using MotionPath and batch render with a specified frame range, the first frame from within that range could 
be incorrectly rendered — fixed.

 - UDIM textures are now cached, like any other image textures, for performance enhancement.
 - Ray visibility flags for hair geometry are now respected.
 - Converter scripts were not installed correctly on macOS — fixed.
 - Camera callback changes from command line rendering were not respected — fixed.
 - An error which was occurring in some cases when Rectangular Area Lights were changed in size has been fixed.
 - Crashing occurred if swatch rendering was enabled and disabled while the IPR was running — fixed.
 - Gamma correction on single layer EXR files is now correct.
 - An issue when a shader node is connected to multiple blend materials has been fixed.
 - The IPR could randomly stop refreshing an image in certain cases — fixed.
 - Issues when framebuffers could be flipped in some cases have been fixed.
 - Shadow Catchers now work on macOS both in final and IPR renders also with the denoiser enabled.
 - Crashing occurred if denoisers were used with the Render Region option — fixed.
 - When using an IBL Environment Light with tiled rendering and the denoiser, the background would be black — fixed.
 - Area lights were shadowing too aggressively — fixed.
 - Switching to contour rendering could fail on an NVidia GPU — fixed.
 - Possible crashing when rendering AOVs in the IPR view has been fixed.
 - Textures could be too blurry due to incorrect MIP mapping, particularly textures in a plane parallel to the camera direction.
 - Adaptive sampling now works with RPR 2.
 - Spiral artifacts when rendering with both the CPU and GPU have been fixed.
 - The Opacity AOV was not taking the maximum ray depth into account — fixed.
 - Artifacts in the Depth AOV have been fixed.
 - Compiling the BVH geometry has been improved in large scenes.

## Known Issues:
 - RPR 2.0 has some upcoming features. If these are needed, please use the Legacy render mode:
	Heterogenous volumes;
	Adaptive subdivision. 

# Version 3.0

## New Features:
-   The new plug-in version incorporates version 2.0 of our Radeon™ ProRender system and brings about significant changes and enhancements:
    - Hardware-accelerated ray tracing on AMD Radeon™ RX 6000 series GPUs.
    - Better scaling across multiple devices: the use of two or more GPUs gives a better performance improvement with Radeon ProRender 2.0 than with Radeon ProRender 1.0 in most cases.
    - Less noise for a given number of render samples.  Using the same number of samples as with Radeon ProRender 1.0 may be slower in some scenes, but noise will be significantly lower.  
    - RPR 2.0 is the default render mode called “Full”.  Users who wish to use RPR 1.0 can set the render quality mode to “Legacy”.
    - A new setting for the texture cache has been added.  The specified folder will cache textures for rendering, and can be cleaned up by the user if it becomes too large.
    - Support for disk and sphere light types in the Physical Light has been added.
-   A script to convert V-Ray scenes to RPR has been added.
-   A setting has been added to allow “Motion Blur only in the Velocity AOV” in the Motion Blur settings.  Enabling this setting means that all AOVs will not have motion blur, but the Velocity AOV will contain motion blur information to allow compositing of post-process motion blur.
-   Support of OpenColorIO color space management via Maya’s preferences has been added.

## Issues Fixed:
-   Using the Uber shader with the Metalness reflection mode now matches the Disney shader PBR standard more closely.
-   File paths for textures can now include environment variables.
-   Changes in the Thumbnail iteration Count parameter did not cause any changes without a restart of Maya — fixed.
-   The setting to “Save all AOVs” was not being saved with the scene — fixed.
-   Any AOV can now be viewed in the IPR viewer.
-   Errors of the type “Unable to create mesh” now contain more info.
-   Some alembic shapes were receiving incorrect UV coordinates — fixed.
-   A crash which could occur when the user edited a ramp node in the IPR mode has been fixed.
-   Better logging during batch rendering has been enabled.
-   nHair node attributes can now be used to render the hair color.
-   The issues fixed relating to .rpr file export:
    - The config.json file now has the same name as the exported .rpr file.
    - Support of non-ASCII characters in the file path when exporting an .rpr file has been added.
    - Export of .rpr config.json files now reflects the selected render quality mode.
    - Animated .rpr files can now be exported.
    - The same number of config.json files and .rpr files are now exported.
-   Motion blur was previously scaled using “frames per second”.  Now it simply uses frame time.  That is, the camera “exposure” setting is specified in fractions of the length of a frame.  This corresponds to the camera’s shutter being open for some fraction of the frame time.  Motion Blur, particularly for rotation, is now more correct.
-   Some fixes were done to the Arnold converter to support the aiFacingRatio and aiRange nodes, as well as fixes for SSS in aiStandardSurface conversion.

## Known Issues:
-   RPR 2.0 has some forthcoming features.  If these are needed, please use the “Legacy” render mode:
    - Heterogenous volumes;
    - Adaptive sampling;
    - Adaptive subdivision.
-   The first render on macOS® with RPR 2.0 can take a few minutes to start, while the kernels are being compiled.
-   macOS® Mojave users should use Legacy mode if seeing crashes with Full mode.
-   Pixelated textures or color artifacts in textures can sometimes happen in Full mode.  


# Version 2.9.4

## New Features

-   **Hair improvements**:
    Support for Esphere's Ornatrix Hair plug-in for Maya has been added;
    Support has been added for tapered width curves/hair for Xgen.
    

-   Maya 2020 support has been added.
    
-   macOS Plug-in now supports ML Denoising.
    
-   The RPR 2.0 “experimental” render mode has been added. Currently this is Windows only and only recommended for final rendering. This is a prototype of our next generation renderer. Performance and memory usage should be improved especially for complex scenes. Multi-GPU and CPU + GPU performance particularly when rendering with an AMD CPU + AMD GPU is dramatically improved. For complex scenes that are larger than video memory size, out-of-core textures and geometry are automatic.
    
-   Lighting improvements:
    - RPR Light parameters now show in the Light Editor and Hypershade.
    - A color option has been added to the IBL node instead of just a map.
  
  - Render settings UI changes:
    -   Tiled Rendering has been moved to the Sampling tab;
	-   Adaptive sampling settings are now separate for final and viewport Rendering;
    -   "Completion Criteria" settings have been renamed to "Sampling Settings";
    -   Default for Max Time is now 0 (i.e. there is no Max Time, Max Samples is used by default);
    -   Full Spectrum Rendering has been disabled in the UI for macOS.
    

-   Flags for command line rendering have been updated:
    -   -stmp ― to enable or disable render stamp;
    -   -devc ― to choose the render device. Format is "gpu1;cpu";
    -   Example: render.exe -r FireRender -of "jpg" -im "test3" -stmp 1 -devc "gpu1;cpu" arealight_test1.mb will enable render stamp and enable the first GPU and disable the CPU.
    

-   Improvements to file export:
    -   RPR file export now respects Maya frame padding options;
    -   Optimization and feedback of progress with GLTF export have been implemented;
    -   An option to select camera with .rpr export has been added ;
    -   GLTF export now takes into account frame start and end time;
    -   RPR export now works correctly for the file name if a single frame is used.
    

-   Shading nodes updates:
    -   The lookup types have been added to the RPR Lookup node:
	    -   Outgoing Vector;
	    -   Object position;
	    -   Vertex Color;
	    -   Random (per object) color.
    -   Support for separate U and V coordinates has been added to the Texture Node input;
    -   The File Texture node now supports ”Alpha” and "Alpha is luminance" for outputs;
    -   The Maya Blend node order has been corrected (was in reverse);
    -   Support for the Maya Clamp node has been added;
    -   The Place2DTexture node Coverage (U and V) parameters are now respected;
    -   Support for Maya Bump2D node;
    -   Support for the Maya Gamma Correct node has been added;
    -   Support for the Blend 2 Attr Node has been added;
    -   Support for the Texture Projection node has been added for texture inputs. Note: U, V Angle parameters do not yet work;
    -   Ramp node now works with the Arithmetic node inputs;
    -   The Sheen parameter has been added to Uber material;
    -   The Arithmetic nodes plugged into color ramps now work correctly.
    

-   Camera Motion Blur is now supported.
    
-   Support has been added to load OpenVDB volumes with a new node “RPR Volume”.
    

## Bug Fixes

-   Gamma is no longer set if tone mapping is used.
    
-   Pressing “Escape” while rendering now cancels the render faster.
    
-   Excessive error messages in Full Spectrum Rendering modes have been quelled.
    
-   Adaptive sampling has been improved.
    
-   Issues with setting Color, Weight, Transparency with a Shadow Catcher material has been fixed.
    
-   Error messages if min > max in the Displacement node settings have been fixed.
    

  
  

## Known Issues:

-   ML Denoiser on macOS (with certain Vega cards) can produce black pixels.
    
-   Viewport rendering with RPR 2.0 is not recommended.
    
-   Invisible area lights can cause firefly artifacts in RPR 2.0.
