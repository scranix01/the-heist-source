//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========
//
// Purpose: Objective Handling for Project Heist
//
//=============================================================================

#include "cbase.h"

static int m_nObjectiveCount = 0;

class CHeistObjectiveHandler : public CLogicalEntity
{
public:
	DECLARE_CLASS(CHeistObjectiveHandler, CLogicalEntity);
	DECLARE_DATADESC();

	// Constructor
	CHeistObjectiveHandler()
	{
		m_nObjectiveCount;
	}

	// Input function
	void nextObj(inputdata_t& inputData); // our tick function

private:

	int	m_nType;	// Stores Objective Type
	int m_nPrimaryObjectiveNumber; // where this primary objective is in the list

	COutputEvent	m_OnPrimaryObjectiveComplete; // Output event for primary objective completion
};

LINK_ENTITY_TO_CLASS(logic_objective, CHeistObjectiveHandler);

// Start of our data description for the class
BEGIN_DATADESC(CHeistObjectiveHandler)

// Links our member variable to our keyvalue from Hammer
DEFINE_KEYFIELD(m_nType, FIELD_INTEGER, "type"),
DEFINE_KEYFIELD(m_nPrimaryObjectiveNumber, FIELD_INTEGER, "objectiveNumber"),


// Links our input name from Hammer to our input member function
DEFINE_INPUTFUNC(FIELD_VOID, "NextObjective", nextObj),

// Links our output member to the output name used by Hammer
DEFINE_OUTPUT(m_OnPrimaryObjectiveComplete, "OnPrimaryObjectiveComplete"),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Handles Next Objective when condition reached
//-----------------------------------------------------------------------------
void CHeistObjectiveHandler::nextObj(inputdata_t& inputData)
{
	// Check if this is a primary objective
	if (m_nType == 1 && m_nPrimaryObjectiveNumber == m_nObjectiveCount) // assuming type 1 is primary objective
	{
		// Fire the primary objective completion event
		m_OnPrimaryObjectiveComplete.FireOutput(inputData.pActivator, this);
		ConColorMsg(COLOR_GREEN, "output objective");

		// Starts Next Primary Objective
		m_nObjectiveCount++;
	}
}