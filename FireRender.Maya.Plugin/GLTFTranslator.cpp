#include "GLTFTranslator.h"

#include "FireRenderContext.h"

#include <maya/MItDag.h>
#include <maya/MGlobal.h>
#include <maya/MDagPath.h>

#include <thread>

#ifdef OPAQUE
#undef OPAQUE
#include "ProRenderGLTF.h"
#endif


using namespace FireMaya;

GLTFTranslator::GLTFTranslator() = default;


GLTFTranslator::~GLTFTranslator() = default;

void* GLTFTranslator::creator()
{
	return new GLTFTranslator();
}

MStatus	GLTFTranslator::writer(const MFileObject& file,
	const MString& optionsString,
	FileAccessMode mode)
{
#if defined(OSMac_)
    return MStatus::kFailure;
#else
	// Create new context and fill it with scene
	std::unique_ptr<FireRenderContext> m_context = std::make_unique<FireRenderContext>();

	m_context->setCallbackCreationDisabled(true);
	if (!m_context->buildScene(false, false, false, true))
		return MS::kFailure;

	// Some resolution should be set. It's requred to retrieve background image.
	// We set 800x600 here but we can set any other resolution.
	m_context->setResolution(800, 600, false, 0);

	MStatus status;
	
	//Get current active camera from active 3d view
	MDagPath camera;
	M3dView view = M3dView::active3dView(&status);
	if (status != MS::kSuccess) 
		return status;

	status = view.getCamera(camera);
	if (status != MS::kSuccess)
		return status;

	m_context->setCamera(camera);
	m_context->state = FireRenderContext::StateRendering;

	//Populate rpr scene with actual data
	if (!m_context->Freshen(false, [this]() { return false; }))
		return MS::kFailure;

	frw::Scene scene = m_context->GetScene();
	frw::Context context = m_context->GetContext();
	frw::MaterialSystem materialSystem = m_context->GetMaterialSystem();

	if (!scene || !context || !materialSystem)
		return MS::kFailure;

	std::vector<rpr_scene> scenes;
	scenes.push_back(scene.Handle());

	gltf::glTF gltf;
	if (!rpr::ExportToGLTF(gltf, file.expandedFullName().asChar(), context.Handle(), materialSystem.Handle(), materialSystem.GetRprxContext(), scenes))
		return MS::kFailure;

	// Export glTF object to disk.
	if (!gltf::Export(file.expandedFullName().asChar(), gltf))
		return MS::kFailure;
	
	m_context->cleanScene();

	return MS::kSuccess;
#endif
}
