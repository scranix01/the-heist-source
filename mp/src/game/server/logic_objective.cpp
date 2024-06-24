//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========
//
// Purpose: Objective Handling for Project Heist
//
//=============================================================================

#include "cbase.h"

static int m_nPrimaryObjectiveCount = 0;

class CHeistObjectiveHandler : public CLogicalEntity
{
public:
	DECLARE_CLASS(CHeistObjectiveHandler, CLogicalEntity);
	DECLARE_DATADESC();

	// Constructor
	CHeistObjectiveHandler()
	{
		m_nPrimaryObjectiveCount;
	}

	// Input function
	void nextObj(inputdata_t& inputData); // move to next obj
	void DisableObjective();

private:

	int	m_nType;	// Stores Objective Type
	int m_nPrimaryObjectiveNumber; // where this primary objective is in the list
	int m_nSecondaryObjectiveNumber; // where this primary objective is in the list

	COutputEvent	m_OnPrimaryObjectiveComplete; // Output event for primary objective completion
	COutputEvent	m_OnSecondaryObjectiveComplete; // Output event for primary objective completion

};

LINK_ENTITY_TO_CLASS(logic_objective, CHeistObjectiveHandler);

// Start of our data description for the class
BEGIN_DATADESC(CHeistObjectiveHandler)

// Links our member variable to our keyvalue from Hammer
DEFINE_KEYFIELD(m_nType, FIELD_INTEGER, "type"),
DEFINE_KEYFIELD(m_nPrimaryObjectiveNumber, FIELD_INTEGER, "primObjectiveNumber"),
DEFINE_KEYFIELD(m_nSecondaryObjectiveNumber, FIELD_INTEGER, "secObjectiveNumber"),


// Links our input name from Hammer to our input member function
DEFINE_INPUTFUNC(FIELD_VOID, "NextObjective", nextObj),

// Links our output member to the output name used by Hammer
DEFINE_OUTPUT(m_OnPrimaryObjectiveComplete, "OnPrimaryObjectiveComplete"),
DEFINE_OUTPUT(m_OnSecondaryObjectiveComplete, "OnSecondaryObjectiveComplete"),


END_DATADESC()

void CHeistObjectiveHandler::DisableObjective()
{
	// TODO: BETTER DISABLING, NOT.. THIS
	UTIL_Remove(this);
}

// handles next objective and deletes other shit
void CHeistObjectiveHandler::nextObj(inputdata_t& inputData)
{
    // Check if this is a primary objective
    if (m_nType == 1 && m_nPrimaryObjectiveNumber == m_nPrimaryObjectiveCount) // assuming type 1 is primary objective
    {
        // Fire the primary objective completion event
        m_OnPrimaryObjectiveComplete.FireOutput(inputData.pActivator, this);
        ConColorMsg(LOG_COLOR_B_GREEN, "primary output objective \n");

        // Starts Next Primary Objective
        m_nPrimaryObjectiveCount++;

        // Disable other primary objectives with the same number
        CBaseEntity* pEntity = nullptr;
        while ((pEntity = gEntList.FindEntityByClassname(pEntity, "logic_objective")) != nullptr)
        {
            CHeistObjectiveHandler* pObjective = dynamic_cast<CHeistObjectiveHandler*>(pEntity);
            if (pObjective && pObjective != this && pObjective->m_nType == 1 && pObjective->m_nPrimaryObjectiveNumber == m_nPrimaryObjectiveNumber)
            {
                pObjective->DisableObjective();
            }
        }
    }
    else if (m_nType == 2 && m_nPrimaryObjectiveNumber == m_nPrimaryObjectiveCount) {
        // fire secondary objective
        m_OnSecondaryObjectiveComplete.FireOutput(inputData.pActivator, this);

        // Log that bih
        ConColorMsg(LOG_COLOR_ORANGE, "secondary output objective \n");

        // Disable other secondary objectives with the same number
        CBaseEntity* pEntity = nullptr;
        while ((pEntity = gEntList.FindEntityByClassname(pEntity, "logic_objective")) != nullptr)
        {
            CHeistObjectiveHandler* pObjective = dynamic_cast<CHeistObjectiveHandler*>(pEntity);
            if (pObjective && pObjective != this && pObjective->m_nType == 2 && pObjective->m_nPrimaryObjectiveNumber == m_nPrimaryObjectiveNumber)
            {
                pObjective->DisableObjective();
            }
        }
    }
}
