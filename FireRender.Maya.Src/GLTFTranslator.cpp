#include "GLTFTranslator.h"

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
	std::unique_ptr<FireRenderContext> fireRenderContext = std::make_unique<FireRenderContext>();
	ContextAutoCleaner contextAutoCleaner(fireRenderContext.get());

	fireRenderContext->setCallbackCreationDisabled(true);
	if (!fireRenderContext->buildScene(false, false, false, true))
		return MS::kFailure;

	// Some resolution should be set. It's requred to retrieve background image.
	// We set 800x600 here but we can set any other resolution.
	fireRenderContext->setResolution(800, 600, false, 0);

	MStatus status;
	
	//Get current active camera from active 3d view
	MDagPath camera;
	M3dView view = M3dView::active3dView(&status);
	if (status != MS::kSuccess) 
		return status;

	status = view.getCamera(camera);
	if (status != MS::kSuccess)
		return status;

	fireRenderContext->setCamera(camera);
	fireRenderContext->state = FireRenderContext::StateRendering;

	//Populate rpr scene with actual data
	if (!fireRenderContext->Freshen(false, [this]() { return false; }))
		return MS::kFailure;

	frw::Scene scene = fireRenderContext->GetScene();
	frw::Context context = fireRenderContext->GetContext();
	frw::MaterialSystem materialSystem = fireRenderContext->GetMaterialSystem();

	if (!scene || !context || !materialSystem)
		return MS::kFailure;

	std::vector<rpr_scene> scenes;
	scenes.push_back(scene.Handle());

#if (RPR_API_VERSION >= 0x010032500)
	int err = rprExportToGLTF(file.expandedFullName().asChar(), context.Handle(), materialSystem.Handle(), materialSystem.GetRprxContext(), scenes.data(), scenes.size(), 0);
#else
	int err = rprExportToGLTF(file.expandedFullName().asChar(), context.Handle(), materialSystem.Handle(), materialSystem.GetRprxContext(), scenes.data(), scenes.size());
#endif

	if (err != GLTF_SUCCESS)
	{
		return MS::kFailure;
	}

	return MS::kSuccess;
#endif
}
