//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IBONUSMAPSDATABASE_H
#define IBONUSMAPSDATABASE_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

struct ChallengeDescription_t
{
	char szName[32];
	char szComment[256];

	int iType;

	int iBronze;
	int iSilver;
	int iGold;

	int iBest;
};

struct BonusMapDescription_t
{
	bool bIsFolder;

	char szShortName[64];
	char szFileName[128];

	char szMapFileName[128];
	char szChapterName[128];
	char szImageName[128];

	char szMapName[64];
	char szComment[256];

	bool bLocked;
	bool bComplete;

	CUtlVector<ChallengeDescription_t>	*m_pChallenges;

	BonusMapDescription_t( void )
	{
		bIsFolder = false;

		szShortName[ 0 ] = '\0';
		szFileName[ 0 ] = '\0';

		szMapFileName[ 0 ] = '\0';
		szChapterName[ 0 ] = '\0';
		szImageName[ 0 ] = '\0';

		szMapName[ 0 ] = '\0';
		szComment[ 0 ] = '\0';

		bLocked = false;
		bComplete = false;

		m_pChallenges = NULL;
	}
};

class IBonusMapsDatabase
{
public:
	virtual void RootPath( void ) = 0;
	virtual void AppendPath( const char *pchAppend ) = 0;
	virtual void BackPath( void ) = 0;
	virtual const char * GetPath( void ) = 0;

	virtual void ClearBonusMapsList( void ) = 0;
	virtual void ScanBonusMaps( void ) = 0;
	virtual void RefreshMapData( void ) = 0;

	virtual int BonusCount( void ) = 0;
	virtual BonusMapDescription_t * GetBonusData( int iIndex ) = 0;
	virtual bool IsValidIndex( int iIndex ) = 0;

	virtual void SetCurrentChallengeNames( const char *pchFileName, const char *pchMapName, const char *pchChallengeName ) = 0;
	virtual void SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold ) = 0;

	virtual float GetCompletionPercentage( void ) = 0;
};

#endif // IBONUSMAPSDATABASE_H
