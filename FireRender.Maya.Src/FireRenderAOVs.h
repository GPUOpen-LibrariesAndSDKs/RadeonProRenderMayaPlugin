#pragma once

#include <map>
#include "FireRenderAOV.h"
#include <maya/MFnDependencyNode.h>


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
	FireRenderAOV& getAOV(unsigned int id);

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

private:
	void AddAOV(unsigned int id, const MString& attribute, const MString& name,
									const MString& folder, AOVDescription description);

private:
	// Members
	// -----------------------------------------------------------------------------

	/** AOV region. */
	RenderRegion m_region;

	/** The set of AOV data. */
	std::map<unsigned int, std::shared_ptr<FireRenderAOV> > m_aovs;

	/** The ID of the AOV to display in the Maya render view. */
	short m_renderViewAOVId;
};
