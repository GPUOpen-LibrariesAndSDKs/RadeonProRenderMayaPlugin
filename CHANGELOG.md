# Version 2.9.42

## New features
-   **Beta support for RPR 2.0 and the capability of using it for Maya Interactive Photorealistic Render (IPR) has been added.**
-   The Arnold to RPR conversion script has been updated.
-   Support for Maya nHair has been added.  
-   A field has been added to the Transform nodes to set RPR Object ID (used with the Object ID AOV).
-   Support for UDIM (Mari) textures in the File node has been added.
-   Sync and Render time are now displayed in the Maya console.
-   A config.json file for rendering via the command line or cloud is exported with an .rpr file.
-   The ML Denoising filter speed has been improved.
-   The capability to assign a “Light Group ID” attribute to lights has been added. With the corresponding AOV, this allows artists to separate lighting via groups of lights.  
-   Support for Image texture sequences for the Image File node has been added.
-   Support for Maya GPU cache objects (Alembic files) has been added.
-   For users interested in testing the latest developments in the Radeon ProRender for Maya plugin, a weekly “Development Build” will be posted on future Fridays.  See https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRenderMayaPlugin/releases or follow the repository on github to get weekly updates.

## Fixed issues
-   The SetRange, RemapHSV and RemapValue nodes now process faster.
-   A crash can no longer occur if the Material nodes are connected to a Transform node.
-   In some instances when the render settings were imported to a scene, some render layers failed to export — fixed.
-   Instances of lights were not exported correctly — fixed.
-   .rpr Export did not always use the correct camera — fixed.
-   The Anti-Aliasing filter could not be set to < 1.0 — the minimum value now is 0.0.
-   Physical Light normalization was not taking transform scaling into account — fixed.
-   Various Conversion scripts no longer can result in failures.
-   RPR lights were showing up in the viewport as Locators. They are now Lights.
-   The camera size in GLTF files is now correct.
-   Various issues have been fixed with render layers using shader overrides and with updating them for IPR.
-   The plugin can now start on macOS with the Japanese language set.
-   Various bugs have been fixed in the OpenVDB node UI.
-   Lights created from the Hypershade no longer have incorrect names in the outliner.
-   Some settings in the “System” Tab have been moved to other tabs. In particular, the Render Engine and Ray Depth settings have been moved to the Quality tab and are now saved with the scene, not in the Maya preferences.
-   Animated lights are exported correctly with GLTF

## Known issues
-   RPR 2.0 known issues
    - Shadow and reflection catchers are not yet enabled
    - Adaptive sampling is disabled
    - Adaptive subdivision is disabled
    - On macOS, currently RPR 2.0 uses OpenCL and not metal.


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
