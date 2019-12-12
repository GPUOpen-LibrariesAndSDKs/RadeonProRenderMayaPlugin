#include "FireRenderTexture.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>

namespace FireMaya
{

	namespace
	{
		namespace Attribute
		{
			MObject	uv;
			MObject	uvSize;
			MObject	output;
			MObject	source;
			MObject	mapChannel;
		}
	}

	void* Texture::creator()
	{
		return new Texture();
	}

	MStatus Texture::initialize()
	{
		MFnTypedAttribute tAttr;
		MFnNumericAttribute nAttr;
		MFnEnumAttribute eAttr;

		Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
		MAKE_INPUT(nAttr);
		// Note: we are not using this, but we have to have it to support classification of this node as a texture2D in Maya:
		Attribute::uvSize = nAttr.create("uvFilterSize", "fs", MFnNumericData::k2Float);
		MAKE_INPUT(nAttr);

		Attribute::output = nAttr.createColor("out", "o");
		MAKE_OUTPUT(nAttr);

		Attribute::source = tAttr.create("filename", "src", MFnData::kString);
		tAttr.setUsedAsFilename(true);
		MAKE_INPUT_CONST(tAttr);

		Attribute::mapChannel = eAttr.create("mapChannel", "mc", 0);
		eAttr.addField("0", kTexture_Channel0);
		eAttr.addField("1", kTexture_Channel1);
		MAKE_INPUT_CONST(eAttr);

		CHECK_MSTATUS(addAttribute(Attribute::uv));
		CHECK_MSTATUS(addAttribute(Attribute::uvSize));
		CHECK_MSTATUS(addAttribute(Attribute::output));
		CHECK_MSTATUS(addAttribute(Attribute::source));
		CHECK_MSTATUS(addAttribute(Attribute::mapChannel));

		CHECK_MSTATUS(attributeAffects(Attribute::uv, Attribute::output));
		CHECK_MSTATUS(attributeAffects(Attribute::uvSize, Attribute::output));
		CHECK_MSTATUS(attributeAffects(Attribute::source, Attribute::output));
		CHECK_MSTATUS(attributeAffects(Attribute::mapChannel, Attribute::output));

		return MS::kSuccess;
	}

	frw::Value Texture::GetValue(Scope& scope)
	{
		MFnDependencyNode shaderNode(thisMObject());

		MPlug texturePlug = shaderNode.findPlug(Attribute::source, false);
		MString texturePath = texturePlug.asString();
		MString nodeName = shaderNode.name();
		if (auto image = scope.GetImage(texturePath, "", nodeName))
		{
			frw::ImageNode imageNode(scope.MaterialSystem());
			imageNode.SetMap(image);

			Type mapChannel = kTexture_Channel0;

			MPlug mapChannelPlug = shaderNode.findPlug(Attribute::mapChannel, false);
			if (!mapChannelPlug.isNull())
			{
				int n = 0;
				if (MStatus::kSuccess == mapChannelPlug.getValue(n))
				{
					mapChannel = static_cast<Type>(n);
				}
			}

			imageNode.SetValue("uv", scope.GetConnectedValue(shaderNode.findPlug(Attribute::uv, false)) | scope.MaterialSystem().ValueLookupUV(mapChannel));

			return imageNode;
		}
		return nullptr;
	}

	MString Texture::getMapPath()
	{
		MFnDependencyNode shaderNode(thisMObject());

		MPlug texturePlug = shaderNode.findPlug(Attribute::source, false);
		MString texturePath = texturePlug.asString();

		return texturePath;
	}

}