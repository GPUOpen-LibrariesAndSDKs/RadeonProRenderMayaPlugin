#include "FireRenderBlendOverride.h"
#include <maya/MStringArray.h>
#include <maya/MFragmentManager.h>
#include <maya/MGlobal.h>
#include <maya/MShaderManager.h>
#include <maya/MDrawContext.h>
#include <fstream>
#include "FireRenderMaterial.h"
#include "FireRenderUtils.h"

FireRenderBlendOverride::FireRenderBlendOverride(const MObject& shader) : MHWRender::MPxShaderOverride(shader), m_shaderBound(false)
{
	MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
	const MHWRender::MShaderManager* shaderMgr = renderer->getShaderManager();
	m_shader = shader;
	if (!renderer->getFragmentManager()->hasFragment(m_fragmentName))
		m_fragmentName = renderer->getFragmentManager()->addShadeFragmentFromFile("RPRBlend.xml", true);
	mShaderInstance = shaderMgr->getFragmentShader(m_fragmentName, "", true);
}

FireRenderBlendOverride::~FireRenderBlendOverride()
{
	if (mShaderInstance)
	{
		MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
		if (renderer)
		{
			const MHWRender::MShaderManager* shaderMgr = renderer->getShaderManager();
			if (shaderMgr)
			{
				shaderMgr->releaseShader(mShaderInstance);
				mShaderInstance = NULL;
			}
		}
	}
}

MHWRender::MPxShaderOverride* FireRenderBlendOverride::creator(const MObject& shader)
{
	return new FireRenderBlendOverride(shader);
}

MString FireRenderBlendOverride::initialize(const MInitContext& initContext, MInitFeedback& initFeedback)
{
	if (mShaderInstance)
	{
		addShaderSignature(*mShaderInstance);
		setGeometryRequirements(*mShaderInstance);
	}
	return "RPRBlendMaterialOverride";
}

#include <vector>
#include <string>

void FireRenderBlendOverride::updateDevice()
{
	if (mShaderInstance)
	{
		MFnDependencyNode shaderNode(m_shader);
		MPlug cplug1 = shaderNode.findPlug("color0");
		MPlug cplug2 = shaderNode.findPlug("color1");
		MPlug wplug = shaderNode.findPlug("weight");
		if (!cplug1.isNull() && !cplug2.isNull() && !wplug.isNull())
		{
			float c1[3];
			for (unsigned int i = 0; i < 3; i++)
			{
				const auto& child = cplug1.child(i);
				c1[i] = child.asFloat();
			}

			float c2[3];
			for (unsigned int i = 0; i < 3; i++)
			{
				const auto& child = cplug2.child(i);
				c2[i] = child.asFloat();
			}

			float w;
			w = wplug.asFloat();

			float col1[] = { c1[0], c1[1], c1[2], 1.f };
			float col2[] = { c2[0], c2[1], c2[2], 1.f };

			MStatus result = mShaderInstance->setParameter("color1", col1);
			result = mShaderInstance->setParameter("color2", col2);
			result = mShaderInstance->setParameter("weight", w);
		}
	}
}

void FireRenderBlendOverride::endUpdate()
{

}

void FireRenderBlendOverride::updateDG(MObject object)
{
}

bool FireRenderBlendOverride::handlesDraw(MHWRender::MDrawContext& context)
{
	const MHWRender::MPassContext& passCtx = context.getPassContext();
	const MStringArray& passSem = passCtx.passSemantics();

	// For color passes, only handle if there isn't already
	// a global override. This is the same as the default
	// logic for this method in MPxShaderOverride
	bool handlePass = false;
	for (auto pass : passSem)
	{
		if (pass == MHWRender::MPassContext::kColorPassSemantic)
		{
			bool hasOverrideShader = passCtx.hasShaderOverride();
			if (!hasOverrideShader)
				handlePass = true;
		}

		// If these semantics are specified then they override
		// the color pass semantic handling.
		else if (pass == MHWRender::MPassContext::kDepthPassSemantic ||
			pass == MHWRender::MPassContext::kNormalDepthPassSemantic)
		{
			handlePass = false;
		}
	}
	return handlePass;
}

void FireRenderBlendOverride::activateKey(MHWRender::MDrawContext& context, const MString& /*key*/)
{
	if (mShaderInstance)
	{
		mShaderInstance->bind(context);	// make sure our shader is the active shader
		m_shaderBound = true;
	}
}

bool FireRenderBlendOverride::draw(MHWRender::MDrawContext& context, const MHWRender::MRenderItemList& renderItemList) const
{
	if (!m_shaderBound)
		return false;

	// Draw for color pass
	if (mShaderInstance)
	{
		unsigned int passCount = mShaderInstance->getPassCount(context);
		if (passCount)
		{
			for (unsigned int i = 0; i<passCount; i++)
			{
				mShaderInstance->activatePass(context, i);
				MHWRender::MPxShaderOverride::drawGeometry(context);
			}
		}

		return true;
	}

	return false;
}

void FireRenderBlendOverride::terminateKey(MHWRender::MDrawContext& context, const MString& /*key*/)
{
	if (m_shaderBound)
	{
		if (mShaderInstance)
		{
			mShaderInstance->unbind(context);
		}
	}

	m_shaderBound = false;
}

MHWRender::DrawAPI FireRenderBlendOverride::supportedDrawAPIs() const
{
	return MHWRender::DrawAPI::kAllDevices;
}

bool FireRenderBlendOverride::isTransparent()
{
	return false;
}

bool FireRenderBlendOverride::overridesDrawState()
{
	return false;
}

#ifndef MAYA2015
bool FireRenderBlendOverride::overridesNonMaterialItems() const
{
	return true;
}

MHWRender::MShaderInstance* FireRenderBlendOverride::nonTexturedShaderInstance(bool &monitorNode) const
{
	return mShaderInstance;
}
#endif

MHWRender::MShaderInstance* FireRenderBlendOverride::shaderInstance() const
{
	return mShaderInstance;
}
