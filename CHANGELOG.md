
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