//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========
//
// Purpose: Objective Handler for Project Heist, each objective is an induvidial entity - this might
// be an issue later on, if we have massive branching paths but, oh well - we cross that bridge
// when we get to it, hopefully never
// 
//=============================================================================

#include "cbase.h"

// this is maybe stupid for networking, but it should be fine
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
	void nextObj(inputdata_t& inputData); // goes to to next objective

private:

	bool	m_nisOptional;	// Stores Objective Type
	int		m_nPrimaryObjectiveNumber; // is this objective next?

	COutputEvent	m_OnPrimaryObjectiveComplete; // Output event for primary objective completion
	COutputEvent	m_OnOptionalObjectiveComplete; // Output event for optional objective completion (not needed to finish)
};

LINK_ENTITY_TO_CLASS(logic_objective, CHeistObjectiveHandler);

// // i hate valve boilerplate // // i hate valve boilerplate // // i hate valve boilerplate // // i hate valve boilerplate // // i hate valve boilerplate

// Start of our data description for the class
BEGIN_DATADESC(CHeistObjectiveHandler)

// Links our member variable to our keyvalue from Hammer
DEFINE_KEYFIELD(m_nisOptional, FIELD_INTEGER, "Optional"),
DEFINE_KEYFIELD(m_nPrimaryObjectiveNumber, FIELD_INTEGER, "objectiveNumber"),


// Links our input name from Hammer to our input member function
DEFINE_INPUTFUNC(FIELD_VOID, "NextObjective", nextObj),

// Links our output member to the output name used by Hammer
DEFINE_OUTPUT(m_OnPrimaryObjectiveComplete, "OnPrimaryObjectiveComplete"),
DEFINE_OUTPUT(m_OnOptionalObjectiveComplete, "OnOptionalObjectiveComplete")

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Handles Next Objective when condition reached
//-----------------------------------------------------------------------------
void CHeistObjectiveHandler::nextObj(inputdata_t& inputData)
{
	// Check if this is a primary objective
	if (m_nisOptional == false && m_nPrimaryObjectiveNumber == m_nObjectiveCount) // assuming type 1 is primary objective, it HAS to be the next primary objective - or else it wont trigger
	{
		// Fire the primary objective completion event
		m_OnPrimaryObjectiveComplete.FireOutput(inputData.pActivator, this);
		ConColorMsg(LOG_COLOR_PURPLE, "primary objective %d", m_nObjectiveCount);

		// Starts Next Primary Objective
		m_nObjectiveCount++;
	}
	if (m_nisOptional == true) { 
		// we dont care what order you do optionals, just that they arent locked out because of the mission status
		// but we currently dont have a loud flag, so TODO: that shit

		// oh and because we arent storing a number we can kill this entity at any point, and it wont error.
		// we just need to make the UI element reload, so i might rewrite the "kill" to not.... kill.
		m_OnOptionalObjectiveComplete.FireOutput(inputData.pActivator, this);
		ConColorMsg(LOG_COLOR_MAGENTA, "secondary objective");

	}

}