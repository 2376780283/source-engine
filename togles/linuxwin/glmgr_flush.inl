// BE VERY VERY CAREFUL what you do in these function. They are extremely hot, and calling the wrong GL API's in here will crush perf. (especially on NVidia threaded drivers).

#include "togles/linuxwin/glmgr.h"

FORCEINLINE uint32 bitmix32(uint32 a)
{
	a -= (a<<6);
	//a ^= (a>>17);
	//a -= (a<<9);
	a ^= (a<<4);
	//a -= (a<<3);
	//a ^= (a<<10);
	a ^= (a>>15);
	return a;
}

FORCEINLINE void GLMExpandProgramParamRevisionRange(
	const uint *pCurrentRevisions,
	const uint *pUploadedRevisions,
	uint nLogicalStart,
	uint nPhysicalStart,
	uint nSlotCount,
	int &nFirstDirtyPhysicalSlot,
	int &nDirtyPhysicalSlotHighWater )
{
	for ( uint i = 0; i < nSlotCount; ++i )
	{
		if ( pCurrentRevisions[nLogicalStart + i] != pUploadedRevisions[nLogicalStart + i] )
		{
			const int nPhysicalSlot = (int)( nPhysicalStart + i );
			nFirstDirtyPhysicalSlot = MIN( nFirstDirtyPhysicalSlot, nPhysicalSlot );
			nDirtyPhysicalSlotHighWater = MAX( nDirtyPhysicalSlotHighWater, nPhysicalSlot + 1 );
		}
	}
}

FORCEINLINE void GLMCopyProgramParamRevisions(
	uint *pUploadedRevisions,
	const uint *pCurrentRevisions,
	uint nLogicalStart,
	uint nSlotCount )
{
	if ( nSlotCount )
	{
		memcpy(
			pUploadedRevisions + nLogicalStart,
			pCurrentRevisions + nLogicalStart,
			nSlotCount * sizeof(uint) );
	}
}

#ifndef OSX

FORCEINLINE GLuint GLMContext::FindSamplerObject( const GLMTexSamplingParams &desiredParams )
{
	uint32 lodBits;
	memcpy( &lodBits, &desiredParams.m_lodBias, sizeof(lodBits) );
	const uint nHashKey = desiredParams.m_bits + desiredParams.m_borderColor + bitmix32(lodBits);

	for ( ;; )
	{
		const uint nHashSize = m_nSamplerObjectHashSize;
		const uint nHashMask = nHashSize - 1;
		uint h = bitmix32( nHashKey ) & nHashMask;
		uint nProbes = 0;
		uint nFirstOpen = nHashSize;	// first insertable (tombstone) slot seen; nHashSize == none

		// Linear probe.  Tombstone slots are skipped (the key may live past
		// them) but remembered as an insertion point; the probe stops only at
		// a live match, a genuinely empty slot, or a full wrap.
		for ( ;; )
		{
			const GLMTexSamplingParams &slotParams = m_samplerObjectHash[h].m_params;

			if ( slotParams.m_packed.m_isValid )
			{
				if ( ( slotParams.m_bits == desiredParams.m_bits ) &&
					 ( slotParams.m_borderColor == desiredParams.m_borderColor ) &&
					 ( slotParams.m_lodBias == desiredParams.m_lodBias ) )
				{
					return m_samplerObjectHash[h].m_samplerObject;	// live match
				}
			}
			else if ( !slotParams.m_packed.m_tombstone )
			{
				// Genuinely empty: probes never pass an empty slot, so the key
				// is absent and this is the insertion point.
				nFirstOpen = h;
				break;
			}
			else if ( nFirstOpen == nHashSize )
			{
				nFirstOpen = h;	// remember the first tombstone, keep probing
			}

			if ( ++nProbes >= nHashSize )
			{
				// Wrapped the whole table without finding an empty slot.
				break;
			}
			if ( ++h > nHashMask )
				h = 0;
		}

		if ( nFirstOpen == nHashSize )
		{
			// No empty slot and no tombstone in the chain: the table is full
			// of live entries and this key is absent.  Make room and re-probe;
			// the old fixed-size table would spin forever here.
			if ( m_nSamplerObjectHashSize >= cMaxSamplerObjectHashSize )
				EvictSamplerObjectHashEntry();
			else
				GrowSamplerObjectHash();
			continue;
		}

		// Insert at the remembered slot (empty, or the first tombstone in the
		// probe chain).  Copying desiredParams clears the tombstone bit, so the
		// slot becomes a live entry again.
		if ( !m_samplerObjectHash[nFirstOpen].m_samplerObject )
		{
			gGL->glGenSamplers( 1, &m_samplerObjectHash[nFirstOpen].m_samplerObject );
		}
		GLMTexSamplingParams &hashParams = m_samplerObjectHash[nFirstOpen].m_params;
		hashParams = desiredParams;
		hashParams.SetToSamplerObject( m_samplerObjectHash[nFirstOpen].m_samplerObject );
		m_nSamplerObjectHashNumEntries++;
		return m_samplerObjectHash[nFirstOpen].m_samplerObject;
	}
}

#endif // !OSX

// BE VERY CAREFUL WHAT YOU DO IN HERE. This is called on every batch, even seemingly simple changes can kill perf.
FORCEINLINE void GLMContext::FlushDrawStates( uint nStartIndex, uint nEndIndex, uint nBaseVertex )	// shadersOn = true for draw calls, false for clear calls
{
	VPROF_BUDGET( "ToGL_FlushDrawStates", "ToGL_FlushDrawStates" );
	Assert( m_drawingLang == kGLMGLSL ); // no support for ARB shaders right now (and NVidia reports that they aren't worth targeting under Windows/Linux for various reasons anyway)
	Assert( ( m_drawingFBO == m_boundDrawFBO ) && ( m_drawingFBO == m_boundReadFBO ) ); // this check MUST succeed
	Assert( m_pDevice->m_pVertDecl );

	// D3D state setters only update the desired state. Commit the final delta
	// once, immediately before the draw, like DXVK's PrepareDraw path.
	FlushRenderStates();

	// Flush any pending persistent-buffer write ranges before this draw.
	// GetHandle() normally does this on the attrib-enumeration path, but
	// NOOVERWRITE appends into a live ring slice do not bump the buffer
	// revision, so the enumeration (and thus GetHandle) can be skipped while
	// a flush is still pending - the GPU would read stale bytes.  Check the
	// four bound streams directly: four loads per draw, zero GL work when
	// nothing is pending.
	for ( uint nStream = 0; nStream < 4; ++nStream )
	{
		CGLMBuffer *pBuf = m_pDevice->m_vtx_buffers[ nStream ];
		if ( pBuf->m_bPendingPersistentFlush )
		{
			pBuf->FlushPendingPersistentRange();
		}
	}

#if GLMDEBUG
	GLM_FUNC;
#endif

	GL_BATCH_PERF( m_FlushStats.m_nTotalBatchFlushes++; )

#if GLMDEBUG
	bool tex0_srgb = (m_boundDrawFBO[0].m_attach[0].m_tex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0;

	// you can only actually use the sRGB FB state on some systems.. check caps
	if (m_caps.m_hasGammaWrites)
	{
		GLBlendEnableSRGB_t	writeSRGBState;
		m_BlendEnableSRGB.Read( &writeSRGBState, 0 );	// the client set value, not the API-written value yet..
		bool draw_srgb = writeSRGBState.enable != 0;

		if (draw_srgb)
		{
			if (tex0_srgb)
			{
				// good - draw mode and color tex agree
			}
			else
			{
				// bad

				// Client has asked to write sRGB into a texture that can't do it.
				// there is no way to satisfy this unless we change the RT tex and we avoid doing that.
				// (although we might consider a ** ONE TIME ** promotion.
				// this shouldn't be a big deal if the tex format is one where it doesn't matter like 32F.

				GLMPRINTF(("-Z- srgb-enabled FBO conflict: attached tex %08x [%s] is not SRGB", m_boundDrawFBO[0].m_attach[0].m_tex, m_boundDrawFBO[0].m_attach[0].m_tex->m_layout->m_layoutSummary ));

				// do we shoot down the srgb-write state for this batch?
				// I think the runtime will just ignore it.
			}
		}
		else
		{
			if (tex0_srgb)
			{
				// odd - client is not writing sRGB into a texture which *can* do it.
				//GLMPRINTF(( "-Z- srgb-disabled FBO conflict: attached tex %08x [%s] is SRGB", m_boundFBO[0].m_attach[0].m_tex, m_boundFBO[0].m_attach[0].m_tex->m_layout->m_layoutSummary ));
				//writeSRGBState.enable = true;
				//m_BlendEnableSRGB.Write( &writeSRGBState );
			}
			else
			{
				// good - draw mode and color tex agree
			}
		}
	}
#endif

	Assert( m_drawingProgram[ kGLMVertexProgram ] );
	Assert( m_drawingProgram[ kGLMFragmentProgram ] );

	Assert( ( m_drawingProgram[kGLMVertexProgram]->m_type == kGLMVertexProgram ) && ( m_drawingProgram[kGLMFragmentProgram]->m_type == kGLMFragmentProgram ) );
	Assert( m_drawingProgram[ kGLMVertexProgram ]->m_bTranslatedProgram && m_drawingProgram[ kGLMFragmentProgram ]->m_bTranslatedProgram );
	
#if GLMDEBUG
	// Depth compare mode check
	uint nCurMask = 1, nShaderSamplerMask = m_drawingProgram[kGLMFragmentProgram]->m_samplerMask;
	for ( int nSamplerIndex = 0; nSamplerIndex < GLM_SAMPLER_COUNT; ++nSamplerIndex, nCurMask <<= 1 )
	{
		if ( !m_samplers[nSamplerIndex].m_pBoundTex )
			continue;

		if ( m_samplers[nSamplerIndex].m_pBoundTex->m_layout->m_mipCount == 1 )
		{
			if ( m_samplers[nSamplerIndex].m_samp.m_packed.m_mipFilter == D3DTEXF_LINEAR )
			{
				GLMDebugPrintf( "Sampler %u has mipmap filtering enabled on a texture without mipmaps! (texture name: %s, pixel shader: %s)!\n",
					nSamplerIndex, 
					m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel ? m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel : "?", 
					m_drawingProgram[kGLMFragmentProgram]->m_shaderName );
			}
		}

		if ( ( nShaderSamplerMask & nCurMask ) == 0 )
			continue;

		if ( m_samplers[nSamplerIndex].m_pBoundTex->m_layout->m_mipCount == 1 )
		{
			if ( m_samplers[nSamplerIndex].m_samp.m_packed.m_mipFilter == D3DTEXF_LINEAR )
			{
				// Note this is not always an error - shadow buffer debug visualization shaders purposely want to read shadow depths (and not do the comparison)
				GLMDebugPrintf( "Sampler %u has mipmap filtering enabled on a texture without mipmaps! (texture name: %s, pixel shader: %s)!\n",
					nSamplerIndex, 
					m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel ? m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel : "?", 
					m_drawingProgram[kGLMFragmentProgram]->m_shaderName );
			}
		}
				
		bool bSamplerIsDepth = ( m_samplers[nSamplerIndex].m_pBoundTex->m_layout->m_key.m_texFlags & kGLMTexIsDepth ) != 0;
		bool bSamplerShadow = m_samplers[nSamplerIndex].m_samp.m_packed.m_compareMode != 0; 

		bool bShaderShadow = ( m_drawingProgram[kGLMFragmentProgram]->m_nShadowDepthSamplerMask & nCurMask ) != 0;
		
		if ( bShaderShadow )
		{
			// Shader expects shadow depth sampling at this sampler index
			// Must have a depth texture and compare mode must be enabled
			if ( !bSamplerIsDepth || !bSamplerShadow )
			{
				// FIXME: This occasionally occurs in L4D2 when CShaderAPIDx8::ExecuteCommandBuffer() sets the TEXTURE_WHITE texture in the flashlight depth texture slot.
				GLMDebugPrintf( "Sampler %u's compare mode (%u) or format (depth=%u) is not consistent with pixel shader's compare mode (%u) (texture name: %s, pixel shader: %s)!\n",
					nSamplerIndex, bSamplerShadow, bSamplerIsDepth, bShaderShadow,
					m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel ? m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel : "?", 
					m_drawingProgram[kGLMFragmentProgram]->m_shaderName );
			}
		}
		else 
		{
			// Shader does not expect shadow depth sampling as this sampler index
			// We don't care if comparemode is enabled, but we can't have a depth texture in this sampler
			if ( bSamplerIsDepth )
			{
				GLMDebugPrintf( "Sampler %u is a depth texture but the pixel shader's shadow depth sampler mask does not expect depth here (texture name: %s, pixel shader: %s)!\n",
					nSamplerIndex, 
					m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel ? m_samplers[nSamplerIndex].m_pBoundTex->m_debugLabel : "?", 
					m_drawingProgram[kGLMFragmentProgram]->m_shaderName );
			}
		}
	}
#endif

	bool bProgramPairChanged = false;
	bool bProgramParamEpochMismatch = false;
	bool bForceFullProgramParamUpload = false;
	if ( m_bDirtyPrograms )
	{
		m_bDirtyPrograms = false;

		CGLMShaderPair *pNewPair = m_pairCache->SelectShaderPair( m_drawingProgram[ kGLMVertexProgram ], m_drawingProgram[ kGLMFragmentProgram ], 0 );

		if ( pNewPair != m_pBoundPair )
		{
			bProgramPairChanged = true;
#if GL_BATCH_TELEMETRY_ZONES
			tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "NewProgram" );
#endif

			if ( !pNewPair->m_valid )
			{
				if ( !pNewPair->ValidateProgramPair() )
				{
					goto flush_error_exit;
				}
			}

			gGL->glUseProgram( (GLuint)pNewPair->m_program );
			
			m_nGpuFrameProgramChanges++;

			GL_BATCH_PERF( m_FlushStats.m_nTotalProgramPairChanges++; )

			if ( !m_pBoundPair )
			{
				GL_BATCH_PERF( m_FlushStats.m_nNewPS++; )
				GL_BATCH_PERF( m_FlushStats.m_nNewVS++; )
			}
			else 
			{
				GL_BATCH_PERF( if ( pNewPair->m_fragmentProg != m_pBoundPair->m_fragmentProg ) m_FlushStats.m_nNewPS++; )
				GL_BATCH_PERF( if ( pNewPair->m_vertexProg != m_pBoundPair->m_vertexProg ) m_FlushStats.m_nNewVS++; )
			}

#if GL_BATCH_PERF_ANALYSIS
			tmMessage( TELEMETRY_LEVEL2, TMMF_ICON_NOTE, "V:%s (V Regs:%u V Bone Regs:%u) F:%s (F Regs:%u)", 
				m_drawingProgram[ kGLMVertexProgram ]->m_shaderName,
				m_drawingProgram[ kGLMVertexProgram ]->m_descs[kGLMGLSL].m_highWater, 
				m_drawingProgram[ kGLMVertexProgram ]->m_descs[kGLMGLSL].m_VSHighWaterBone, 
				m_drawingProgram[ kGLMFragmentProgram ]->m_shaderName, 
				m_drawingProgram[ kGLMFragmentProgram ]->m_descs[kGLMGLSL].m_highWater );
#endif

			m_pBoundPair = pNewPair;

			// check fragment buffers used (MRT)
			if( pNewPair->m_fragmentProg->m_fragDataMask != m_fragDataMask )
			{
				gGL->glDrawBuffers( pNewPair->m_fragmentProg->m_numDrawBuffers, pNewPair->m_fragmentProg->m_drawBuffers );
				m_fragDataMask = pNewPair->m_fragmentProg->m_fragDataMask;
			}
		}
	}

	// Uniform storage belongs to the linked GL program.  On a pair switch, only
	// re-upload logical slots whose desired revision differs from what this exact
	// pair last received.  A new revision epoch (only possible at serial wrap or
	// after link/relink) deliberately falls back to a complete active-range push.
	bProgramParamEpochMismatch =
		( m_pBoundPair->m_nProgramParamRevisionEpoch != m_nProgramParamRevisionEpoch );
	bForceFullProgramParamUpload =
		bProgramParamEpochMismatch || ( bProgramPairChanged && !m_bUseProgramParamRevisionCache );
	if ( bProgramPairChanged || bProgramParamEpochMismatch )
	{
		const int nVertexNonBoneHighWater =
			MIN( (int)m_drawingProgram[kGLMVertexProgram]->m_descs[kGLMGLSL].m_highWater,
				kGLMProgramParamFloat4Limit );
		const int nVertexBoneHighWater =
			( m_bUseBoneUniformBuffers && ( m_nMaxUsedVertexProgramConstantsHint > DXABSTRACT_VS_FIRST_BONE_SLOT ) ) ?
			MIN( (int)m_drawingProgram[kGLMVertexProgram]->m_descs[kGLMGLSL].m_VSHighWaterBone,
				MIN( m_nMaxUsedVertexProgramConstantsHint, DXABSTRACT_VS_LAST_BONE_SLOT + 1 ) - DXABSTRACT_VS_FIRST_BONE_SLOT ) : 0;
		const int nFragmentHighWater =
			MIN( (int)m_drawingProgram[kGLMFragmentProgram]->m_descs[kGLMGLSL].m_highWater,
				kGLMProgramParamFloat4Limit );

		if ( bForceFullProgramParamUpload )
		{
			if ( nVertexNonBoneHighWater > 0 )
			{
				m_programParamsF[kGLMVertexProgram].m_firstDirtySlotNonBone = 0;
				m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone =
					MAX( m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone, nVertexNonBoneHighWater );
			}
			m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterBone =
				MAX( m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterBone, nVertexBoneHighWater );

			if ( nFragmentHighWater > 0 )
			{
				m_programParamsF[kGLMFragmentProgram].m_firstDirtySlotNonBone = 0;
				m_programParamsF[kGLMFragmentProgram].m_dirtySlotHighWaterNonBone =
					MAX( m_programParamsF[kGLMFragmentProgram].m_dirtySlotHighWaterNonBone, nFragmentHighWater );
			}
		}
		else
		{
			int nFirstDirty = kGLMProgramParamFloat4Limit;
			int nDirtyHighWater = 0;

			if ( m_bUseBoneUniformBuffers )
			{
				const int nPreBoneCount = MIN( nVertexNonBoneHighWater, DXABSTRACT_VS_FIRST_BONE_SLOT );
				GLMExpandProgramParamRevisionRange(
					m_programParamRevisionF[kGLMVertexProgram],
					m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
					0, 0, nPreBoneCount, nFirstDirty, nDirtyHighWater );

				if ( nVertexNonBoneHighWater > DXABSTRACT_VS_FIRST_BONE_SLOT )
				{
					const int nPostBoneCount = MIN(
						nVertexNonBoneHighWater - DXABSTRACT_VS_FIRST_BONE_SLOT,
						kGLMProgramParamFloat4Limit - (DXABSTRACT_VS_LAST_BONE_SLOT + 1) );
					GLMExpandProgramParamRevisionRange(
						m_programParamRevisionF[kGLMVertexProgram],
						m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
						DXABSTRACT_VS_LAST_BONE_SLOT + 1,
						DXABSTRACT_VS_FIRST_BONE_SLOT,
						nPostBoneCount, nFirstDirty, nDirtyHighWater );
				}
			}
			else
			{
				GLMExpandProgramParamRevisionRange(
					m_programParamRevisionF[kGLMVertexProgram],
					m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
					0, 0, nVertexNonBoneHighWater, nFirstDirty, nDirtyHighWater );
			}

			if ( nDirtyHighWater > nFirstDirty )
			{
				m_programParamsF[kGLMVertexProgram].m_firstDirtySlotNonBone =
					MIN( m_programParamsF[kGLMVertexProgram].m_firstDirtySlotNonBone, nFirstDirty );
				m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone =
					MAX( m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone, nDirtyHighWater );
			}

			nFirstDirty = 0;
			nDirtyHighWater = 0;
			GLMExpandProgramParamRevisionRange(
				m_programParamRevisionF[kGLMVertexProgram],
				m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
				DXABSTRACT_VS_FIRST_BONE_SLOT, 0, nVertexBoneHighWater,
				nFirstDirty, nDirtyHighWater );
			m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterBone =
				MAX( m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterBone, nDirtyHighWater );

			nFirstDirty = kGLMProgramParamFloat4Limit;
			nDirtyHighWater = 0;
			GLMExpandProgramParamRevisionRange(
				m_programParamRevisionF[kGLMFragmentProgram],
				m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMFragmentProgram],
				0, 0, nFragmentHighWater, nFirstDirty, nDirtyHighWater );
			if ( nDirtyHighWater > nFirstDirty )
			{
				m_programParamsF[kGLMFragmentProgram].m_firstDirtySlotNonBone =
					MIN( m_programParamsF[kGLMFragmentProgram].m_firstDirtySlotNonBone, nFirstDirty );
				m_programParamsF[kGLMFragmentProgram].m_dirtySlotHighWaterNonBone =
					MAX( m_programParamsF[kGLMFragmentProgram].m_dirtySlotHighWaterNonBone, nDirtyHighWater );
			}
		}

		// Bool/int banks are tiny, so a single revision per stage is sufficient.
		if ( bForceFullProgramParamUpload ||
			( m_pBoundPair->m_uploadedProgramParamRevisionB[kGLMVertexProgram] != m_programParamRevisionB[kGLMVertexProgram] ) )
			m_programParamsB[kGLMVertexProgram].m_dirtySlotCount = kGLMProgramParamBoolLimit;
		if ( bForceFullProgramParamUpload ||
			( m_pBoundPair->m_uploadedProgramParamRevisionB[kGLMFragmentProgram] != m_programParamRevisionB[kGLMFragmentProgram] ) )
			m_programParamsB[kGLMFragmentProgram].m_dirtySlotCount = kGLMProgramParamBoolLimit;
		if ( bForceFullProgramParamUpload ||
			( m_pBoundPair->m_uploadedProgramParamRevisionI[kGLMVertexProgram] != m_programParamRevisionI[kGLMVertexProgram] ) )
			m_programParamsI[kGLMVertexProgram].m_dirtySlotCount = kGLMProgramParamInt4Limit;
	}

	Assert( m_ViewportBox.GetData().width == (int)( m_ViewportBox.GetData().widthheight & 0xFFFF ) );
	Assert( m_ViewportBox.GetData().height == (int)( m_ViewportBox.GetData().widthheight >> 16 ) );

	m_pBoundPair->UpdateScreenUniform( m_ViewportBox.GetData().widthheight );

	if ( m_pBoundPair->m_nClipPlaneStateRevision != m_nClipPlaneStateRevision )
	{
		UpdateClipPlaneUniforms();
		m_pBoundPair->m_nClipPlaneStateRevision = m_nClipPlaneStateRevision;
	}
	
	// DXVK only materializes sampler state for slots used by the active
	// shaders. Preserve dirties for inactive slots and commit them if a later
	// shader actually references the slot.
	{
	const uint nUsedSamplerMask =
		m_drawingProgram[kGLMVertexProgram]->m_samplerMask |
		m_drawingProgram[kGLMFragmentProgram]->m_samplerMask;
	uint8 dirtySamplersToFlush[GLM_SAMPLER_COUNT];
	uint nNumDirtySamplersToFlush = 0;
	uint nNumDirtySamplersToKeep = 0;
	for ( uint i = 0; i < m_nNumDirtySamplers; ++i )
	{
		const uint nSamplerIndex = m_nDirtySamplers[i];
		if ( nUsedSamplerMask & ( 1u << nSamplerIndex ) )
			dirtySamplersToFlush[nNumDirtySamplersToFlush++] = (uint8)nSamplerIndex;
		else
			m_nDirtySamplers[nNumDirtySamplersToKeep++] = (uint8)nSamplerIndex;
	}
	m_nNumDirtySamplers = nNumDirtySamplersToKeep;

	GL_BATCH_PERF( m_FlushStats.m_nNumChangedSamplers += nNumDirtySamplersToFlush );

#if !defined( OSX ) // no support for sampler objects in OSX 10.6 (GL 2.1 profile)
	if ( m_bUseSamplerObjects)
	{
		while ( nNumDirtySamplersToFlush )
		{
			const uint nSamplerIndex = dirtySamplersToFlush[--nNumDirtySamplersToFlush];
			Assert( ( nSamplerIndex < GLM_SAMPLER_COUNT ) && ( !m_nDirtySamplerFlags[nSamplerIndex]) );

			m_nDirtySamplerFlags[nSamplerIndex] = 1;

			// On drivers where the texture object accepts the per-texture
			// coarse cap (GL_TEXTURE_MAX_LEVEL, probed at startup and emitted
			// from CGLMTex::WriteTexels), the sampler needs no clamping.  Only
			// fall back to a sampler-side GL_TEXTURE_MAX_LOD clamp for targets
			// the driver rejects (some mobile GLES drivers error on the pname
			// despite ES 3.2).  Keeping the clamp off where possible avoids
			// allocating a distinct sampler object per (texture, mip-streaming-
			// step) combination.
			GLMTexSamplingParams samp = m_samplers[nSamplerIndex].m_samp;
			{
				CGLMTex *pTex = m_samplers[nSamplerIndex].m_pBoundTex;
				if ( pTex && ( pTex->m_layout->m_key.m_texFlags & kGLMTexMipped ) &&
					 !gGL->HaveCoreTexLevelClamp( pTex->m_layout->m_key.m_texGLTarget ) )
				{
					int maxActiveMip = pTex->m_maxActiveMip;
					if ( maxActiveMip >= 0 && (int)samp.m_packed.m_maxLOD > maxActiveMip )
						samp.m_packed.m_maxLOD = maxActiveMip;
				}
			}
			// Source re-binds textures constantly, which re-dirties the sampler
			// even when the sampling state is unchanged.  FindSamplerObject
			// then returns the SAME sampler object that is already bound to
			// this unit.  Skip the glBindSampler in that case - on Mali every
			// redundant bind is a real driver descriptor-table update in the
			// per-draw command stream.
			const GLuint nSamplerObject = FindSamplerObject( samp );
			if ( m_nBoundSamplerObject[nSamplerIndex] != nSamplerObject )
			{
				m_nBoundSamplerObject[nSamplerIndex] = nSamplerObject;
				gGL->glBindSampler( nSamplerIndex, nSamplerObject );
			}

			GL_BATCH_PERF( m_FlushStats.m_nNumSamplingParamsChanged++ );

#if defined( OSX ) // valid for OSX only if using GL 3.3 context 
			CGLMTex *pTex = m_samplers[nSamplerIndex].m_pBoundTex;

			if( pTex && !( gGL->m_bHave_GL_EXT_texture_sRGB_decode ) )
			{
				// see if requested SRGB state differs from the known one
				bool texSRGB = ( pTex->m_layout->m_key.m_texFlags & kGLMTexSRGB ) != 0;
				bool glSampSRGB  = m_samplers[nSamplerIndex].m_samp.m_packed.m_srgb;

				if ( texSRGB != glSampSRGB ) // mismatch
				{
					pTex->HandleSRGBMismatch( glSampSRGB, pTex->m_srgbFlipCount );
				}
			}
#endif
		}
	}
	else
#endif // if !defined( OSX )
	{
		while ( nNumDirtySamplersToFlush )
		{
			const uint nSamplerIndex = dirtySamplersToFlush[--nNumDirtySamplersToFlush];
			Assert( ( nSamplerIndex < GLM_SAMPLER_COUNT ) && ( !m_nDirtySamplerFlags[nSamplerIndex]) );

			m_nDirtySamplerFlags[nSamplerIndex] = 1;

			CGLMTex *pTex = m_samplers[nSamplerIndex].m_pBoundTex;

			// See the sampler-object branch above: only clamp the sampler-side
			// GL_TEXTURE_MAX_LOD for texture targets that rejected the
			// texture-object level cap in the startup probe; accepted targets
			// carry the cap on the texture object itself.
			GLMTexSamplingParams samp = m_samplers[nSamplerIndex].m_samp;
			if ( pTex && ( pTex->m_layout->m_key.m_texFlags & kGLMTexMipped ) &&
				 !gGL->HaveCoreTexLevelClamp( pTex->m_layout->m_key.m_texGLTarget ) )
			{
				int maxActiveMip = pTex->m_maxActiveMip;
				if ( maxActiveMip >= 0 && (int)samp.m_packed.m_maxLOD > maxActiveMip )
					samp.m_packed.m_maxLOD = maxActiveMip;
			}

			if ( ( pTex ) && ( !( pTex->m_SamplingParams == samp ) ) )
			{
				SelectTMU( nSamplerIndex );

				samp.DeltaSetToTarget( pTex->m_texGLTarget, pTex->m_SamplingParams );

				pTex->m_SamplingParams = samp;

#if defined( OSX )
				if( pTex && !( gGL->m_bHave_GL_EXT_texture_sRGB_decode ) )
				{
					// see if requested SRGB state differs from the known one
					bool texSRGB = ( pTex->m_layout->m_key.m_texFlags & kGLMTexSRGB ) != 0;
					bool glSampSRGB  = m_samplers[nSamplerIndex].m_samp.m_packed.m_srgb;

					if ( texSRGB != glSampSRGB ) // mismatch
					{
						pTex->HandleSRGBMismatch( glSampSRGB, pTex->m_srgbFlipCount );
					}	
				}
#endif
			}
		}
	}
	}

	// vertex stage --------------------------------------------------------------------
	if ( m_bUseBoneUniformBuffers )
	{
		// vertex stage --------------------------------------------------------------------
		if ( m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone )
		{
			int firstDirtySlot = m_programParamsF[kGLMVertexProgram].m_firstDirtySlotNonBone;
			int dirtySlotHighWater = MIN( m_drawingProgram[kGLMVertexProgram]->m_descs[kGLMGLSL].m_highWater, m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone );
			const int revisionDirtySlotHighWater = dirtySlotHighWater;

			GLint vconstLoc = m_pBoundPair->m_locVertexParams;
			if ( ( vconstLoc >= 0 ) && ( dirtySlotHighWater > firstDirtySlot ) )
			{
#if GL_BATCH_TELEMETRY_ZONES
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "VSNonBoneUniformUpdate %u %u", firstDirtySlot, dirtySlotHighWater );
#endif
				int numSlots = dirtySlotHighWater - DXABSTRACT_VS_FIRST_BONE_SLOT;
				
				// consts after the bones (c217 onwards), since we use the concatenated destination array vc[], upload these consts starting from vc[58]
				if( numSlots > 0 )
				{
					gGL->glUniform4fv( m_pBoundPair->m_locVertexParams + DXABSTRACT_VS_FIRST_BONE_SLOT, numSlots, &m_programParamsF[kGLMVertexProgram].m_values[(DXABSTRACT_VS_LAST_BONE_SLOT+1)][0] );

					m_nGpuFrameUniformCalls++;
					m_nGpuFrameUniformsSet += numSlots;

					dirtySlotHighWater = DXABSTRACT_VS_FIRST_BONE_SLOT;

					GL_BATCH_PERF( m_nTotalVSUniformCalls++; )
					GL_BATCH_PERF( m_nTotalVSUniformsSet += numSlots; )

					GL_BATCH_PERF( m_FlushStats.m_nFirstVSConstant = DXABSTRACT_VS_FIRST_BONE_SLOT; )
					GL_BATCH_PERF( m_FlushStats.m_nNumVSConstants += numSlots; )
				}
				
				numSlots = dirtySlotHighWater - firstDirtySlot;

				// consts before the bones (c0-c57)
				if( numSlots > 0 )
				{
					gGL->glUniform4fv( m_pBoundPair->m_locVertexParams + firstDirtySlot, dirtySlotHighWater - firstDirtySlot, &m_programParamsF[kGLMVertexProgram].m_values[firstDirtySlot][0] );

					m_nGpuFrameUniformCalls++;
					m_nGpuFrameUniformsSet += dirtySlotHighWater - firstDirtySlot;

					GL_BATCH_PERF( m_nTotalVSUniformCalls++; )
					GL_BATCH_PERF( m_nTotalVSUniformsSet += dirtySlotHighWater - firstDirtySlot; )

					GL_BATCH_PERF( m_FlushStats.m_nFirstVSConstant = firstDirtySlot; )
					GL_BATCH_PERF( m_FlushStats.m_nNumVSConstants += (dirtySlotHighWater - firstDirtySlot); )
				}
			}

			const int nPreBoneRevisionHighWater = MIN( revisionDirtySlotHighWater, DXABSTRACT_VS_FIRST_BONE_SLOT );
			if ( nPreBoneRevisionHighWater > firstDirtySlot )
			{
				GLMCopyProgramParamRevisions(
					m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
					m_programParamRevisionF[kGLMVertexProgram],
					firstDirtySlot, nPreBoneRevisionHighWater - firstDirtySlot );
			}
			if ( revisionDirtySlotHighWater > DXABSTRACT_VS_FIRST_BONE_SLOT )
			{
				const int nPostBoneRevisionCount = MIN(
					revisionDirtySlotHighWater - DXABSTRACT_VS_FIRST_BONE_SLOT,
					kGLMProgramParamFloat4Limit - (DXABSTRACT_VS_LAST_BONE_SLOT + 1) );
				GLMCopyProgramParamRevisions(
					m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
					m_programParamRevisionF[kGLMVertexProgram],
					DXABSTRACT_VS_LAST_BONE_SLOT + 1, nPostBoneRevisionCount );
			}

			m_programParamsF[kGLMVertexProgram].m_firstDirtySlotNonBone = 256;
			m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone = 0;
		}

		if ( m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterBone )
		{
			const GLint vconstBoneLoc = m_pBoundPair->m_locVertexBoneParams;
			int shaderSlotsBone = 0;
			if ( ( m_drawingProgram[kGLMVertexProgram]->m_descs[kGLMGLSL].m_VSHighWaterBone > 0 ) && ( m_nMaxUsedVertexProgramConstantsHint > DXABSTRACT_VS_FIRST_BONE_SLOT ) )
			{
				shaderSlotsBone = MIN( m_drawingProgram[kGLMVertexProgram]->m_descs[kGLMGLSL].m_VSHighWaterBone, m_nMaxUsedVertexProgramConstantsHint - DXABSTRACT_VS_FIRST_BONE_SLOT );
			}

			const int dirtySlotHighWaterBone = MIN( shaderSlotsBone, m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterBone );
			if ( ( vconstBoneLoc >= 0 ) && dirtySlotHighWaterBone )
			{
				const uint nNumBoneRegs = dirtySlotHighWaterBone;
#if GL_BATCH_TELEMETRY_ZONES								
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "VSBoneUniformUpdate %u", nNumBoneRegs );
#endif

				gGL->glUniform4fv( vconstBoneLoc, nNumBoneRegs, &m_programParamsF[kGLMVertexProgram].m_values[DXABSTRACT_VS_FIRST_BONE_SLOT][0] );

				m_nGpuFrameUniformCalls++;
				m_nGpuFrameUniformsSet += nNumBoneRegs;

				GL_BATCH_PERF( m_nTotalVSUniformBoneCalls++; )
				GL_BATCH_PERF( m_nTotalVSUniformsBoneSet += nNumBoneRegs; )
				GL_BATCH_PERF( m_FlushStats.m_nNumVSBoneConstants += nNumBoneRegs; )
			}

			GLMCopyProgramParamRevisions(
				m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
				m_programParamRevisionF[kGLMVertexProgram],
				DXABSTRACT_VS_FIRST_BONE_SLOT, dirtySlotHighWaterBone );
			m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterBone = 0;
		}

	}
	else
	{
		if ( m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone )
		{
			const int nMaxUsedShaderSlots = m_drawingProgram[kGLMVertexProgram]->m_descs[kGLMGLSL].m_highWater;

			int firstDirtySlot = m_programParamsF[kGLMVertexProgram].m_firstDirtySlotNonBone;
			int dirtySlotHighWater = MIN( nMaxUsedShaderSlots, m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone );

			GLint vconstLoc = m_pBoundPair->m_locVertexParams;
			if ( ( vconstLoc >= 0 ) && ( dirtySlotHighWater > firstDirtySlot ) )
			{
	#if GL_BATCH_TELEMETRY_ZONES
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "VSNonBoneUniformUpdate %u %u", firstDirtySlot, dirtySlotHighWater );
	#endif
				gGL->glUniform4fv( m_pBoundPair->m_locVertexParams + firstDirtySlot, dirtySlotHighWater - firstDirtySlot, &m_programParamsF[kGLMVertexProgram].m_values[firstDirtySlot][0] );

				m_nGpuFrameUniformCalls++;
				m_nGpuFrameUniformsSet += dirtySlotHighWater - firstDirtySlot;

				GL_BATCH_PERF( m_nTotalVSUniformCalls++; )
				GL_BATCH_PERF( m_nTotalVSUniformsSet += dirtySlotHighWater - firstDirtySlot; )

				GL_BATCH_PERF( m_FlushStats.m_nFirstVSConstant = firstDirtySlot; )
				GL_BATCH_PERF( m_FlushStats.m_nNumVSConstants += (dirtySlotHighWater - firstDirtySlot); )
			}

			if ( dirtySlotHighWater > firstDirtySlot )
			{
				GLMCopyProgramParamRevisions(
					m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMVertexProgram],
					m_programParamRevisionF[kGLMVertexProgram],
					firstDirtySlot, dirtySlotHighWater - firstDirtySlot );
			}
			m_programParamsF[kGLMVertexProgram].m_firstDirtySlotNonBone = 256;
			m_programParamsF[kGLMVertexProgram].m_dirtySlotHighWaterNonBone = 0;
		}
	}

	// see if VS uses i0, b0, b1, b2, b3.
	// use a glUniform1i to set any one of these if active.  skip all of them if no dirties reported.
	// my kingdom for the UBO extension!

	// ------- bools ---------- //
	if ( m_pBoundPair->m_bHasBoolOrIntUniforms )
	{
		if ( m_programParamsB[kGLMVertexProgram].m_dirtySlotCount )	// optimize this later after the float param pushes are proven out
		{
			const uint nLimit = MIN( CGLMShaderPair::cMaxVertexShaderBoolUniforms, m_programParamsB[kGLMVertexProgram].m_dirtySlotCount );
			for ( uint i = 0; i < nLimit; ++i )
			{
				GLint constBoolLoc = m_pBoundPair->m_locVertexBool[i];
				if ( constBoolLoc >= 0 )
					gGL->glUniform1i( constBoolLoc, m_programParamsB[kGLMVertexProgram].m_values[i] );
			}

			m_programParamsB[kGLMVertexProgram].m_dirtySlotCount = 0;
		}

		if ( m_programParamsB[kGLMFragmentProgram].m_dirtySlotCount )	// optimize this later after the float param pushes are proven out
		{
			const uint nLimit = MIN( CGLMShaderPair::cMaxFragmentShaderBoolUniforms, m_programParamsB[kGLMFragmentProgram].m_dirtySlotCount );
			for ( uint i = 0; i < nLimit; ++i )
			{
				GLint constBoolLoc = m_pBoundPair->m_locFragmentBool[i];
				if ( constBoolLoc >= 0 )
					gGL->glUniform1i( constBoolLoc, m_programParamsB[kGLMFragmentProgram].m_values[i] );
			}

			m_programParamsB[kGLMFragmentProgram].m_dirtySlotCount = 0;
		}

		if ( m_programParamsI[kGLMVertexProgram].m_dirtySlotCount )
		{
			GLint vconstInt0Loc = m_pBoundPair->m_locVertexInteger0;									//glGetUniformLocationARB( prog, "i0");
			if ( vconstInt0Loc >= 0 )
			{
				gGL->glUniform1i( vconstInt0Loc, m_programParamsI[kGLMVertexProgram].m_values[0][0] );	//FIXME magic number
			}
			m_programParamsI[kGLMVertexProgram].m_dirtySlotCount = 0;
		}
	}


	if( !gGL->m_bHave_GL_QCOM_alpha_test && m_pBoundPair->m_locAlphaRef != -1 )
	{
		const float alphaRef = m_AlphaTestEnable.GetData().enable ? m_AlphaTestFunc.GetData().ref : 0.0f;
		if ( m_pBoundPair->m_alphaRefValue != alphaRef )
		{
			gGL->glUniform1f( m_pBoundPair->m_locAlphaRef, alphaRef );
			m_pBoundPair->m_alphaRefValue = alphaRef;
		}
	}

	// Fake-SRGB mode: when the driver has no GL_FRAMEBUFFER_SRGB toggle
	// (OpenGL ES without GL_EXT_sRGB_write_control), WriteBlendEnableSRGB
	// shunts D3DRS_SRGBWRITEENABLE into m_FakeBlendEnableSRGB.  The compiled
	// shader then carries a flSRGBWrite uniform (D3DToGL_OptionSRGBWriteSuffix)
	// that must be driven per draw - without this upload it stays 0 and the
	// sRGB write emulation silently does nothing.
	if ( m_pBoundPair->m_locFragmentFakeSRGBEnable >= 0 )
	{
		const float flFakeSRGB = m_FakeBlendEnableSRGB ? 1.0f : 0.0f;
		if ( m_pBoundPair->m_fakeSRGBEnableValue != flFakeSRGB )
		{
			gGL->glUniform1f( m_pBoundPair->m_locFragmentFakeSRGBEnable, flFakeSRGB );
			m_pBoundPair->m_fakeSRGBEnableValue = flFakeSRGB;
		}
	}

	m_pBoundPair->m_uploadedProgramParamRevisionB[kGLMVertexProgram] =
		m_programParamRevisionB[kGLMVertexProgram];
	m_pBoundPair->m_uploadedProgramParamRevisionB[kGLMFragmentProgram] =
		m_programParamRevisionB[kGLMFragmentProgram];
	m_pBoundPair->m_uploadedProgramParamRevisionI[kGLMVertexProgram] =
		m_programParamRevisionI[kGLMVertexProgram];
	m_pBoundPair->m_uploadedProgramParamRevisionI[kGLMFragmentProgram] =
		m_programParamRevisionI[kGLMFragmentProgram];

	Assert( ( m_pDevice->m_streams[0].m_vtxBuffer && ( m_pDevice->m_streams[0].m_vtxBuffer->m_vtxBuffer == m_pDevice->m_vtx_buffers[0] ) ) || ( ( !m_pDevice->m_streams[0].m_vtxBuffer ) && ( m_pDevice->m_vtx_buffers[0] == m_pDevice->m_pDummy_vtx_buffer ) ) );
	Assert( ( m_pDevice->m_streams[1].m_vtxBuffer && ( m_pDevice->m_streams[1].m_vtxBuffer->m_vtxBuffer == m_pDevice->m_vtx_buffers[1] ) ) || ( ( !m_pDevice->m_streams[1].m_vtxBuffer ) && ( m_pDevice->m_vtx_buffers[1] == m_pDevice->m_pDummy_vtx_buffer ) ) );
	Assert( ( m_pDevice->m_streams[2].m_vtxBuffer && ( m_pDevice->m_streams[2].m_vtxBuffer->m_vtxBuffer == m_pDevice->m_vtx_buffers[2] ) ) || ( ( !m_pDevice->m_streams[2].m_vtxBuffer ) && ( m_pDevice->m_vtx_buffers[2] == m_pDevice->m_pDummy_vtx_buffer ) ) );
	Assert( ( m_pDevice->m_streams[3].m_vtxBuffer && ( m_pDevice->m_streams[3].m_vtxBuffer->m_vtxBuffer == m_pDevice->m_vtx_buffers[3] ) ) || ( ( !m_pDevice->m_streams[3].m_vtxBuffer ) && ( m_pDevice->m_vtx_buffers[3] == m_pDevice->m_pDummy_vtx_buffer ) ) );

	uint nCurTotalBufferRevision;
	{
		// If any of these inputs have changed, we need to enumerate through all of the expected GL vertex attribs and modify anything in the GL layer that have changed.
		// This is not always a win, but it is a net win on NVidia (by 1-4.8% depending on whether driver threading is enabled).
		//
		// Cheap early-out: when the decl/shader input set is unchanged, only the
		// vertex buffers actually referenced by the current shader can affect the
		// attrib pointers.  The engine relocks dynamic buffers every frame (each
		// fresh ring slice bumps m_nRevision), so summing ALL four stream
		// revisions forced the 52.2% branch below even when the shader never
		// reads the relocked stream.  Sum only the used streams; the full loop
		// still runs whenever the input layout itself changes.
		// (Scoped so the earlier goto flush_error_exit does not cross these
		// initializations.)
		nCurTotalBufferRevision = 0;
		const bool bInputLayoutChanged = ( m_CurAttribs.m_nVertexInputRevision != m_pDevice->m_nVertexInputRevision );
		if ( bInputLayoutChanged )
		{
			nCurTotalBufferRevision = m_pDevice->m_vtx_buffers[0]->m_nRevision + m_pDevice->m_vtx_buffers[1]->m_nRevision + m_pDevice->m_vtx_buffers[2]->m_nRevision + m_pDevice->m_vtx_buffers[3]->m_nRevision;
		}
		else
		{
			for ( uint nStream = 0; nStream < 4; ++nStream )
			{
				if ( m_CurAttribs.m_nUsedStreamsMask & ( 1U << nStream ) )
				{
					nCurTotalBufferRevision += m_pDevice->m_vtx_buffers[nStream]->m_nRevision;
				}
			}
		}
	}

	if ( ( nCurTotalBufferRevision != m_CurAttribs.m_nTotalBufferRevision ) ||
		( m_CurAttribs.m_nVertexInputRevision != m_pDevice->m_nVertexInputRevision ) )
	{
		// This branch is taken 52.2% of the time in the L4D2 test1 (long) timedemo.

#if GL_BATCH_TELEMETRY_ZONES
		tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "SetVertexAttribs" );
#endif

		m_CurAttribs.m_nTotalBufferRevision = nCurTotalBufferRevision;
		m_CurAttribs.m_nVertexInputRevision = m_pDevice->m_nVertexInputRevision;

		unsigned char *pVertexShaderAttribMap = m_pDevice->m_vertexShader->m_vtxAttribMap;
		const int nMaxVertexAttributesToCheck = m_drawingProgram[ kGLMVertexProgram ]->m_maxVertexAttrs;

		IDirect3DVertexDeclaration9	*pVertDecl = m_pDevice->m_pVertDecl;
		const uint8	*pVertexAttribDescToStreamIndex = pVertDecl->m_VertexAttribDescToStreamIndex;

		uint nUsedStreamsMask = 0;

		for( int nMask = 1, nIndex = 0; nIndex < nMaxVertexAttributesToCheck; ++nIndex, nMask <<= 1 )
		{
			uint8 vertexShaderAttrib = pVertexShaderAttribMap[ nIndex ];

			uint nDeclIndex = pVertexAttribDescToStreamIndex[vertexShaderAttrib];
			if ( nDeclIndex == 0xFF )
			{
				// Not good - the vertex shader has an attribute which can't be located in the decl! 
				// The D3D9 debug runtime is also going to complain.
				Assert( 0 );

				if ( m_lastKnownVertexAttribMask & nMask )
				{
					m_lastKnownVertexAttribMask &= ~nMask;
					gGL->glDisableVertexAttribArray( nIndex );
				}
				continue;
			}

			D3DVERTEXELEMENT9_GL *pDeclElem = &pVertDecl->m_elements[nDeclIndex];

			Assert( ( ( vertexShaderAttrib >> 4 ) == pDeclElem->m_dxdecl.Usage ) && ( ( vertexShaderAttrib & 0x0F ) == pDeclElem->m_dxdecl.UsageIndex) );

			const uint nStreamIndex = pDeclElem->m_dxdecl.Stream;
			nUsedStreamsMask |= ( 1U << nStreamIndex );
			const D3DStreamDesc *pStream = &m_pDevice->m_streams[ nStreamIndex ];

			CGLMBuffer *pBuf = m_pDevice->m_vtx_buffers[ nStreamIndex ];
			if ( pBuf == m_pDevice->m_pDummy_vtx_buffer )
			{
				Assert( pStream->m_vtxBuffer == NULL );

				// this shader doesn't use that pair.
				if ( m_lastKnownVertexAttribMask & nMask )
				{
					m_lastKnownVertexAttribMask &= ~nMask;
					gGL->glDisableVertexAttribArray( nIndex );
				}
				continue;
			}
			Assert( pStream->m_vtxBuffer->m_vtxBuffer == pBuf );

			int nBufOffset = pDeclElem->m_gldecl.m_offset + pStream->m_offset;
			Assert( nBufOffset >= 0 );
			Assert( nBufOffset < (int)pBuf->m_nSize );
			if ( pBuf->m_bUsingPersistentBuffer )
			{
				nBufOffset += pBuf->m_nPersistentBufferStartOffset;
			}

			SetBufAndVertexAttribPointer( nIndex, pBuf->GetHandle(), 
				pStream->m_stride, pDeclElem->m_gldecl.m_datatype, pDeclElem->m_gldecl.m_normalized, pDeclElem->m_gldecl.m_nCompCount, 
				reinterpret_cast< const GLvoid * >( reinterpret_cast< intp >( pBuf->m_pPseudoBuf ) + nBufOffset ), 
				pBuf->m_nRevision );

			if ( !( m_lastKnownVertexAttribMask & nMask ) )
			{
				m_lastKnownVertexAttribMask |= nMask;
				gGL->glEnableVertexAttribArray( nIndex );
			}
		}

		for( int nIndex = nMaxVertexAttributesToCheck; nIndex < m_nNumSetVertexAttributes; nIndex++ )
		{
			gGL->glDisableVertexAttribArray( nIndex );
			m_lastKnownVertexAttribMask &= ~(1 << nIndex);
		}

		m_nNumSetVertexAttributes = nMaxVertexAttributesToCheck;
		m_CurAttribs.m_nUsedStreamsMask = nUsedStreamsMask;
	}

	// fragment stage --------------------------------------------------------------------
	if ( m_programParamsF[kGLMFragmentProgram].m_dirtySlotHighWaterNonBone )
	{
		const GLint fconstLoc = m_pBoundPair->m_locFragmentParams;
		const int nMaxUsedShaderSlots = m_drawingProgram[kGLMFragmentProgram]->m_descs[kGLMGLSL].m_highWater;
		const int firstDirtySlot = m_programParamsF[kGLMFragmentProgram].m_firstDirtySlotNonBone;
		const int dirtySlotHighWater = MIN( nMaxUsedShaderSlots, m_programParamsF[kGLMFragmentProgram].m_dirtySlotHighWaterNonBone );
		if ( fconstLoc >= 0 )
		{
			if ( dirtySlotHighWater > firstDirtySlot )
			{
#if GL_BATCH_TELEMETRY_ZONES
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "PSUniformUpdate %u %u", firstDirtySlot, dirtySlotHighWater );
#endif

				gGL->glUniform4fv( m_pBoundPair->m_locFragmentParams + firstDirtySlot, dirtySlotHighWater - firstDirtySlot, &m_programParamsF[kGLMFragmentProgram].m_values[firstDirtySlot][0] );

				m_nGpuFrameUniformCalls++;
				m_nGpuFrameUniformsSet += dirtySlotHighWater - firstDirtySlot;

				GL_BATCH_PERF( m_nTotalPSUniformCalls++; )
				GL_BATCH_PERF( m_nTotalPSUniformsSet += dirtySlotHighWater - firstDirtySlot; )

				GL_BATCH_PERF( m_FlushStats.m_nFirstPSConstant = firstDirtySlot; )
				GL_BATCH_PERF( m_FlushStats.m_nNumPSConstants += (dirtySlotHighWater - firstDirtySlot); )
			}
		}

		if ( dirtySlotHighWater > firstDirtySlot )
		{
			GLMCopyProgramParamRevisions(
				m_pBoundPair->m_uploadedProgramParamRevisionF[kGLMFragmentProgram],
				m_programParamRevisionF[kGLMFragmentProgram],
				firstDirtySlot, dirtySlotHighWater - firstDirtySlot );
		}
		m_programParamsF[kGLMFragmentProgram].m_firstDirtySlotNonBone = 256;
		m_programParamsF[kGLMFragmentProgram].m_dirtySlotHighWaterNonBone = 0;
	}

	m_pBoundPair->m_nProgramParamRevisionEpoch = m_nProgramParamRevisionEpoch;

	return;

flush_error_exit:
	m_pBoundPair = NULL;
	m_bDirtyPrograms = true;
	return;
}
