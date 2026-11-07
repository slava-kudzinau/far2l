// One small export that answers the look of a list of Colorer region names
// under the active scheme, for the F3 Markdown view (story
// markdown-colorer-look, ADR-009).  The core reaches the plugin through this
// symbol and ColorerTokenizeV1.  It gives the scheme's foreground and style
// word — plain data; the roles stay in far2l/src/markdown.

#include "pcolorer.h"
#include "FarEditorSet.h"

#include "colorer/Exception.h"
#include "colorer/HrcLibrary.h"
#include "colorer/ParserFactory.h"
#include "colorer/Region.h"
#include "colorer/strings/legacy/UnicodeString.h"
#include "colorer/handlers/RegionMapper.h"
#include "colorer/handlers/StyledHRDMapper.h"
#include "colorer/handlers/StyledRegion.h"

extern FarEditorSet* editorSet;

// Return codes (spec.md, Export): 0 the look is filled, 1 no FarEditorSet
// yet (the export does not create it, so the first call shows no
// "Reloading" message), 2 the plugin is disabled (Enabled=0, or SyntaxDraw=0
// left the factory and the mapper null), 3 a Colorer Exception, 4 a bad abi
// or capacity.  A count-0 call answers with abi checked, no rows, and the
// generation: the bridge probes it before fetching.
SHAREDSYMBOL int ColorerRegionLookV1(int abi, const wchar_t* const* names, int count,
	unsigned int* look, int capacity, unsigned int* generation)
{
	if (abi != 1 || capacity < 2 * count) {
		return 4;
	}
	if (editorSet == nullptr) {
		return 1;
	}
	if (!editorSet->GetPluginStatus() || !editorSet->GetParserFactory()
		|| !editorSet->GetRegionMapper()) {
		return 2;
	}

	try {
		HrcLibrary& library = editorSet->GetParserFactory()->getHrcLibrary();
		StyledHRDMapper* mapper = editorSet->GetRegionMapper();
		for (int i = 0; i < count; ++i) {
			unsigned int fore = 0;
			unsigned int style = 0;
			const Region* region = nullptr;
			if (names != nullptr && names[i] != nullptr) {
				const UnicodeString name(names[i]);
				region = library.getRegion(&name);
			}
			if (region != nullptr) {
				// The Region* overload climbs the HRC `parent=` chain;
				// the name overload does not (story Notes, research).
				const StyledRegion* rd = StyledRegion::cast(mapper->getRegionDefine(region));
				if (rd != nullptr && rd->isForeSet) {
					fore = rd->fore;
					style = rd->style;
				}
			}
			look[2 * i] = fore;
			look[2 * i + 1] = style;
		}
		if (generation != nullptr) {
			*generation = editorSet->GetBaseGeneration();
		}
		return 0;
	} catch (Exception& ex) {
		COLORER_LOG_ERROR("%", ex.what());   // announce, then the core falls back
		return 3;
	}
}
