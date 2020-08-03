/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#pragma once

#include <map>
#include "FireRenderAOV.h"
#include <maya/MFnDependencyNode.h>
#include <functional>

using namespace OIIO;

// Forward declarations.
class FireRenderContext;
class RenderRegion;


/**
 * The set of available AOVs (arbitrary output variables) and
 * methods for registering Maya attributes, reading active
 * state from scene globals and applying to the render context.
 */
class FireRenderAOVs
{
public:

	// Life Cycle
	// -----------------------------------------------------------------------------
	FireRenderAOVs();
	virtual ~FireRenderAOVs();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Get an AOV for the specified ID. */
	FireRenderAOV* getAOV(unsigned int id);

	void ForEachActiveAOV(std::function<void(FireRenderAOV& aov)> actionFunc);

	/** Get the AOV to display in the Maya render view. */
	FireRenderAOV& getRenderViewAOV();

	/** Get the number of active AOVs. */
	unsigned int getActiveAOVCount();

	/** Register attributes with Maya. */
	void registerAttributes();

	/** Read AOV state from scene RPR globals. */
	void readFromGlobals(const MFnDependencyNode& globals);

	/** Apply the current AOV state to the render context. */
	void applyToContext(FireRenderContext& context);

	/** Apply the current render context state to the AOVs state. */
	void setFromContext(FireRenderContext& context);

	/** Set the size of the AOVs, including the full frame buffer size. */
	void setRegion(const RenderRegion& region,
		unsigned int frameWidth, unsigned int frameHeight);

	/** Allocate pixels for active AOVs. */
	void allocatePixels();

	/** Free pixels for active AOVs. */
	void freePixels();

	/** Read the frame buffer pixels for all active AOVs. */
	void readFrameBuffers(FireRenderContext& context, bool flip);

	/** Write the active AOVs to file. */
	void writeToFile(const MString& filePath, unsigned int imageFormat, FireRenderAOV::FileWrittenCallback fileWrittenCallback = nullptr);

	/** Setup render stamp */
	void setRenderStamp(const MString& renderStamp);

	/** Gets number of AOVs */
	int getNumberOfAOVs();

	MString GetEXRCompressionType() const;
	TypeDesc::BASETYPE GetChannelFormat() const;

private:
	void AddAOV(unsigned int id, const MString& attribute, const MString& name,
									const MString& folder, AOVDescription description);

	void InitEXRCompressionMap();

private:
	// Members
	// -----------------------------------------------------------------------------

	/** AOV region. */
	RenderRegion m_region;

	/** The set of AOV data. */
	std::map<unsigned int, std::shared_ptr<FireRenderAOV> > m_aovs;

	/** The ID of the AOV to display in the Maya render view. */
	short m_renderViewAOVId;

	std::map<int, MString> m_exrCompressionTypesMap;
	MString m_exrCompressionType;
	TypeDesc::BASETYPE m_channelFormat;
};
