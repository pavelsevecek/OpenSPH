set(GUI_SOURCES
    Controller.cpp 
    Factory.cpp 
    MainLoop.cpp 
    Project.cpp 
    Settings.cpp 
    Utils.cpp 
    objects/Bitmap.cpp 
    objects/Camera.cpp 
    objects/CameraJobs.cpp
    objects/Colorizer.cpp 
    objects/Filmic.cpp 
    objects/Movie.cpp 
    objects/Palette.cpp 
    objects/RenderContext.cpp 
    objects/RenderJobs.cpp 
    renderers/ContourRenderer.cpp 
    renderers/IRenderer.cpp 
    renderers/MeshRenderer.cpp 
    renderers/ParticleRenderer.cpp 
    renderers/VolumeRenderer.cpp 
    renderers/RayTracer.cpp 
    renderers/Spectrum.cpp 
    windows/BatchDialog.cpp 
    windows/RunSelectDialog.cpp 
    windows/SessionDialog.cpp 
    windows/CurveDialog.cpp 
    windows/GridPage.cpp 
    windows/OrthoPane.cpp 
    windows/PaletteDialog.cpp 
    windows/ParticleProbe.cpp 
    windows/PlotView.cpp 
    windows/RunPage.cpp 
    windows/NodePage.cpp 
    windows/MainWindow.cpp 
    windows/TimeLine.cpp 
    windows/Widgets.cpp)

set(GUI_HEADERS
    ArcBall.h 
    Controller.h 
    Factory.h 
    MainLoop.h 
    Project.h 
    Settings.h 
    Utils.h 
    objects/Bitmap.h 
    objects/Camera.h 
    objects/CameraJobs.h
    objects/Color.h 
    objects/Colorizer.h 
    objects/DelayedCallback.h 
    objects/Filmic.h 
    objects/GraphicsContext.h 
    objects/Movie.h 
    objects/Palette.h 
    objects/Point.h 
    objects/RenderContext.h 
    objects/RenderJobs.h 
    objects/SvgContext.h 
    renderers/Brdf.h 
    renderers/ContourRenderer.h 
    renderers/FrameBuffer.h 
    renderers/IRenderer.h 
    renderers/MeshRenderer.h 
    renderers/ParticleRenderer.h 
    renderers/VolumeRenderer.h 
    renderers/RayTracer.h 
    renderers/Spectrum.h 
    windows/BatchDialog.h 
    windows/RunSelectDialog.h 
    windows/SessionDialog.h 
    windows/CurveDialog.h 
    windows/GridPage.h 
    windows/IGraphicsPane.h 
    windows/Icons.data.h 
    windows/OrthoPane.h 
    windows/PaletteDialog.h 
    windows/ParticleProbe.h 
    windows/PlotView.h 
    objects/Texture.h 
    windows/RunPage.h 
    windows/NodePage.h 
    windows/MainWindow.h 
    windows/TimeLine.h 
    windows/ProgressPanel.h 
    windows/Widgets.h)

add_library(gui SHARED ${GUI_HEADERS} ${GUI_SOURCES})
target_include_directories(gui PUBLIC ../core ..)

execute_process(COMMAND wx-config --cxxflags
    OUTPUT_VARIABLE WX_FLAGS_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND wx-config --libs std propgrid aui
    OUTPUT_VARIABLE WX_LIBRARIES
    OUTPUT_STRIP_TRAILING_WHITESPACE)

separate_arguments(WX_CXX_FLAGS NATIVE_COMMAND ${WX_FLAGS_OUTPUT})
target_compile_options(gui PUBLIC ${WX_CXX_FLAGS} -Wno-deprecated-copy -Wno-array-bounds -Wno-float-conversion)

set(GUI_LIBRARIES core ${WX_LIBRARIES} ${TBB_LIBRARIES})
target_link_libraries(gui PUBLIC ${GUI_LIBRARIES})
