//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BONUSMAPSDATABASE_H
#define BONUSMAPSDATABASE_H
#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"
#include "GameUI/IBonusMapsDatabase.h"

struct BonusMapChallenge_t
{
	char szFileName[128];
	char szMapName[32];
	char szChallengeName[32];
	int iBest;
};


class KeyValues;


//-----------------------------------------------------------------------------
// Purpose: Keeps track of bonus maps on disk
//-----------------------------------------------------------------------------
class CBonusMapsDatabase : public IBonusMapsDatabase
{

public:
	CBonusMapsDatabase( void );
	virtual ~CBonusMapsDatabase();

	bool ReadBonusMapSaveData( void );
	bool WriteSaveData( void );

	virtual const char * GetPath( void ) { return m_szCurrentPath; }
	virtual void RootPath( void );
	virtual void AppendPath( const char *pchAppend );
	virtual void BackPath( void );
	void SetPath( const char *pchPath, int iDirDepth );

	virtual void ClearBonusMapsList( void );
	virtual void ScanBonusMaps( void );
	virtual void RefreshMapData( void );

	virtual int BonusCount( void );
	virtual BonusMapDescription_t * GetBonusData( int iIndex ) { return &(m_BonusMaps[ iIndex ]); }
	int InvalidIndex( void ) { return m_BonusMaps.InvalidIndex(); }
	virtual bool IsValidIndex( int iIndex ) { return m_BonusMaps.IsValidIndex( iIndex ); }

	bool GetBlink( void );
	void SetBlink( bool bState );

	bool BonusesUnlocked( void );

	virtual void SetCurrentChallengeNames( const char *pchFileName, const char *pchMapName, const char *pchChallengeName );
	void GetCurrentChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName );
	virtual void SetCurrentChallengeObjectives( int iBronze, int iSilver, int iGold );
	void GetCurrentChallengeObjectives( int &iBronze, int &iSilver, int &iGold );

	bool SetBooleanStatus( const char *pchName, const char *pchFileName, const char *pchMapName, bool bValue );
	bool SetBooleanStatus( const char *pchName, int iIndex, bool bValue );
	bool UpdateChallengeBest( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest );

	virtual float GetCompletionPercentage( void );

	int NumAdvancedComplete( void );
	void NumMedals( int piNumMedals[ 3 ] );

private:

	void AddBonus( const char *pCurrentPath, const char *pDirFileName, bool bIsFolder );
	void BuildSubdirectoryList( const char *pCurrentPath, bool bOutOfRoot );
	void BuildBonusMapsList( const char *pCurrentPath, bool bOutOfRoot );

	void ParseBonusMapData( char const *pszFileName, char const *pszShortName, bool bIsFolder );

private:

	KeyValues	*m_pBonusMapsManifest;

	CUtlVector<BonusMapDescription_t>	m_BonusMaps;

	KeyValues	*m_pBonusMapSavedData;
	bool		m_bSavedDataChanged;

	int		m_iX360BonusesUnlocked;		// Only used on 360
	bool	m_bHasLoadedSaveData;

	int		m_iDirDepth;
	char	m_szCurrentPath[_MAX_PATH];
	float	m_fCurrentCompletion;
	int		m_iCompletableLevels;

	BonusMapChallenge_t		m_CurrentChallengeNames;
	ChallengeDescription_t	m_CurrentChallengeObjectives;
};


void GetChallengeMedals( ChallengeDescription_t *pChallengeDescription, int &iBest, int &iEarnedMedal, int &iNext, int &iNextMedal );
CBonusMapsDatabase *BonusMapsDatabase( void );

extern const char g_pszMedalNames[4][8];


#endif // BONUSMAPSDATABASE_H
