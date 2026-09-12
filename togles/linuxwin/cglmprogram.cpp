//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// cglmprogram.cpp
//
//===============================================================================

#include "togles/rendermechanism.h"

#include "filesystem.h"
#include "tier1/fmtstr.h"
#include "tier1/KeyValues.h"
#include "tier0/fasttimer.h"
#include "tier1/checksum_md5.h"

#if GLMDEBUG && defined( _MSC_VER )
#include <direct.h>
#endif

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

#if GLMDEBUG
#define GLM_FREE_SHADER_TEXT 0
#else
#define GLM_FREE_SHADER_TEXT 1
#endif

#ifndef GL_PROGRAM_BINARY_LENGTH
#define GL_PROGRAM_BINARY_LENGTH 0x8741
#endif

//===============================================================================

ConVar	gl_shaderpair_cacherows_lg2( "gl_paircache_rows_lg2", "10");		// 10 is minimum
ConVar	gl_shaderpair_cacheways_lg2( "gl_paircache_ways_lg2", "5");		// 5 is minimum
ConVar	gl_shaderpair_cachelog( "gl_shaderpair_cachelog", "0" );

static CCycleCount	gShaderCompileTime;
static int			gShaderCompileCount = 0;
static CCycleCount	gShaderCompileQueryTime;
static CCycleCount	gShaderLinkTime;
static int			gShaderLinkCount = 0;
static CCycleCount	gShaderLinkQueryTime;
CON_COMMAND( gl_shader_compile_time_dump, "Dump  stats shader compile time." )
{
	ConMsg( "Shader Compile Time: %u ms (Count: %d) / Query: %u ms \n", (uint32)gShaderCompileTime.GetMilliseconds(), gShaderCompileCount, (uint32)gShaderCompileQueryTime.GetMilliseconds() );
	ConMsg( "Shader Link Time   : %u ms (Count: %d) / Query: %u ms \n", (uint32)gShaderLinkTime.GetMilliseconds(), gShaderLinkCount, (uint32)gShaderLinkQueryTime.GetMilliseconds() );
}

//===============================================================================


GLenum	GLMProgTypeToARBEnum( EGLMProgramType type )
{
	GLenum	result = 0;
	switch(type)
	{
		case	kGLMVertexProgram:		result = GL_VERTEX_PROGRAM_ARB; break;
		case	kGLMFragmentProgram:	result = GL_FRAGMENT_PROGRAM_ARB; break;
		default:	Assert( !"bad program type"); result = 0; break;
	}
	return result;
}

GLenum	GLMProgTypeToGLSLEnum( EGLMProgramType type )
{
	GLenum	result = 0;
	switch(type)
	{
		case	kGLMVertexProgram:		result = GL_VERTEX_SHADER; break;
		case	kGLMFragmentProgram:	result = GL_FRAGMENT_SHADER; break;
		default:	Assert( !"bad program type"); result = 0; break;
	}
	return result;
}

CGLMProgram::CGLMProgram( GLMContext *ctx, EGLMProgramType type )
{
	m_ctx = ctx;
	m_ctx->CheckCurrent();

	m_type		= type;
	m_nHashTag	= rand() ^ ( rand() << 15 );
	m_text		= NULL;	// no text yet
	
#if GLMDEBUG
	m_editable	= NULL;
#endif

	memset( &m_descs, 0, sizeof( m_descs ) );

	m_samplerMask    = 0;	// dxabstract sets this field later
	m_samplerTypes   = 0;
	m_fragDataMask   = 0;
	m_numDrawBuffers = 0;
	memset( &m_drawBuffers, 0, sizeof( m_drawBuffers ) );

	m_maxSamplers    = GLM_SAMPLER_COUNT;
	m_nNumUsedSamplers = GLM_SAMPLER_COUNT;
	m_maxVertexAttrs = kGLMVertexAttributeIndexMax;

	// create a GLSL shader object.
	GLMShaderDesc *glslDesc = &m_descs[ kGLMGLSL ];
	GLenum glslStage = GLMProgTypeToGLSLEnum( m_type );

	glslDesc->m_object.glsl = gGL->glCreateShader( glslStage );;

	m_shaderName[0] = '\0';

	m_bTranslatedProgram = false;

	m_nCentroidMask = 0;
	m_nShadowDepthSamplerMask = 0;

	m_labelName[0]	= '\0';
	m_labelIndex	= -1;
	m_labelCombo	= -1;

	// no text has arrived yet.  That's done in SetProgramText.
}

CGLMProgram::~CGLMProgram( )
{
	m_ctx->CheckCurrent();

	// if there is a GLSL shader, delete it
	GLMShaderDesc *glslDesc = &m_descs[kGLMGLSL];
	if (glslDesc->m_object.glsl)
	{
		gGL->glDeleteShader( (uint)glslDesc->m_object.glsl );	// why do I need a cast here again ?
		glslDesc->m_object.glsl = 0;
	}

#if GLMDEBUG
	if (m_editable)
	{
		delete m_editable;
		m_editable = NULL;
	}
#endif

	if (m_text)
	{
		free( m_text );
		m_text = NULL;
	}
	m_ctx = NULL;
}

enum EShaderSection
{
	kGLMARBVertex,		kGLMARBVertexDisabled,
	kGLMARBFragment,	kGLMARBFragmentDisabled,
	kGLMGLSLVertex,		kGLMGLSLVertexDisabled,
	kGLMGLSLFragment,	kGLMGLSLFragmentDisabled,
};

void	CGLMProgram::SetShaderName( const char *name )
{
	V_strncpy( m_shaderName, name, sizeof( m_shaderName ) );
}

void	CGLMProgram::SetProgramText( char *text )
{
	// free old text if any
	// clone new text
	// scan newtext to find sections
	// walk sections, and mark descs to indicate where text is at
	
	if (m_text)
	{
		free( m_text );
		m_text = NULL;
	}
	
	// scrub desc text references
	for( int i=0; i<kGLMNumProgramTypes; i++)
	{
		GLMShaderDesc	*desc = &m_descs[i];
		
		desc->m_textPresent = false;
		desc->m_textOffset	= 0;
		desc->m_textLength	= 0;
	}
	
	m_text = strdup( text );
	Assert( m_text != NULL );	

	// -gl_dumpshaders: write the translated GLSL text to shaderdump/ for
	// debugging generated shaders on device.  The text carries the
	// "// trans#N label:..." comment identifying the shader.
	if ( CommandLine()->FindParm( "-gl_dumpshaders" ) )
	{
		static int s_nShaderDumpCount = 0;
		char szPath[256];
		V_snprintf( szPath, sizeof( szPath ), "shaderdump/%s_%03d.glsl",
			m_type == kGLMVertexProgram ? "vs" : "ps", s_nShaderDumpCount++ );
		FILE *pFile = fopen( szPath, "w" );
		if ( pFile )
		{
			fprintf( pFile, "%s", m_text );
			fclose( pFile );
		}
	}

	#if GLMDEBUG
		// create editable text item, if it does not already exist
		if (!m_editable)
		{
			char	*suffix = "";

			switch(m_type)
			{
				case	kGLMVertexProgram:		suffix = ".vsh"; break;
				case	kGLMFragmentProgram:	suffix = ".fsh"; break;
				default:	GLMDebugger();
			}

#ifdef POSIX
            CFmtStr debugShaderPath( "%s/debugshaders/", getenv( "HOME" ) );
#else
			CFmtStr debugShaderPath( "debugshaders/" );
#endif
			_mkdir( debugShaderPath.Access() );
			m_editable = new CGLMEditableTextItem( m_text, strlen(m_text), false, debugShaderPath.Access(), suffix );
			
			// pull our string back from the editable (it has probably munged it)
			if (m_editable->HasData())
			{
				ReloadStringFromEditable();
			}
		}
	#endif

#if 0
	// scan the text and find sections
	CGLMTextSectioner		sections( m_text, strlen( m_text ), g_shaderSectionMarkers );
	
	int sectionCount = sections.Count();
	for( int i=0; i < sectionCount; i++ )
	{
		uint subtextOffset	= 0;
		uint subtextLength	= 0;
		int markerIndex		= 0;
		
		sections.GetSection( i, &subtextOffset, &subtextLength, &markerIndex );
#endif
		
	uint subtextOffset	= 0;
	uint subtextLength	= strlen( m_text );
	int markerIndex		= 0;

	// act on the section
	GLMShaderDesc *desc = NULL;
	desc = &m_descs[kGLMGLSL];
		
	// these steps are generic across both langs
	desc->m_textPresent	= true;
	desc->m_textOffset	= subtextOffset;
	desc->m_textLength	= subtextLength;
	desc->m_compiled	= false;
	desc->m_valid		= false;
		
	// find the label string
	// example:
	// trans#2871 label:vs-file vertexlit_and_unlit_generic_vs20 vs-index 294912 vs-combo 1234

	char *lineStr = strstr( m_text, "// trans#" );
	if (lineStr)
	{
		int		scratch = -1;
		
		if (this->m_type == kGLMVertexProgram)
		{
			sscanf( lineStr, "// trans#%d label:vs-file %s vs-index %d vs-combo %d", &scratch, m_labelName, &m_labelIndex, &m_labelCombo );
		}
		else
		{
			sscanf( lineStr, "// trans#%d label:ps-file %s ps-index %d ps-combo %d", &scratch, m_labelName, &m_labelIndex, &m_labelCombo );
		}
	}
}

void	CGLMProgram::CompileActiveSources	( void )
{
	// compile everything we have text for
	for( int i=0; i<kGLMNumProgramTypes; i++)
	{
		if (m_descs[i].m_textPresent)
		{
			Compile( (EGLMProgramLang)i );
		}
	}
}

void	CGLMProgram::Compile( EGLMProgramLang lang )
{
	bool bTimeShaderCompiles = (CommandLine()->FindParm( "-gl_time_shader_compiles" ) != 0);
	// If using "-gl_time_shader_compiles", keeps track of total cycle count spent on shader compiles.
	CFastTimer shaderCompileTimer;
	if (bTimeShaderCompiles)
	{
		shaderCompileTimer.Start();
	}
	
	bool noisy = false; noisy;
	int loglevel = gl_shaderpair_cachelog.GetInt();
	
	switch( lang )
	{
		case kGLMGLSL:
		{
			GLMShaderDesc *glslDesc;
			
			glslDesc = &m_descs[ kGLMGLSL ];

			GLenum glslStage = GLMProgTypeToGLSLEnum( m_type );
			glslStage;
			
			// no GLSL program either
			gGL->glUseProgram(0);
			
			// pump text into GLSL shader object

			char *section = m_text + glslDesc->m_textOffset;
			char *lastCharOfSection = section + glslDesc->m_textLength;	// actually it's one past the last textual character
			lastCharOfSection;

			#if GLMDEBUG
				if(noisy)
				{
					GLMPRINTF((">-D- CGLMProgram::Compile submitting following text for GLSL %s program (name %d) ---------------------",
						glslStage == GL_FRAGMENT_SHADER ? "fragment" : "vertex",
						glslDesc->m_object.glsl ));

					// we don't have a "print this many chars" call yet
					// just temporarily null terminate the text we want to print
					
					char saveChar = *lastCharOfSection;
					
					*lastCharOfSection= 0;
					GLMPRINTTEXT(( section, eDebugDump ));
					*lastCharOfSection= saveChar;

					GLMPRINTF(("<-D- CGLMProgram::Compile GLSL EOT--" ));
				}
			#endif

			gGL->glShaderSource( glslDesc->m_object.glsl, 1, (const GLchar **)&section, &glslDesc->m_textLength);	

			// compile
			gGL->glCompileShader( glslDesc->m_object.glsl );

			GLint isCompiled = 0;
			gGL->glGetShaderiv(glslDesc->m_object.glsl, GL_COMPILE_STATUS, &isCompiled);

			if(isCompiled == GL_FALSE)
			{
				GLint maxLength = 0;
				gGL->glGetShaderiv(glslDesc->m_object.glsl , GL_INFO_LOG_LENGTH, &maxLength);

				GLchar  log[4096];
				gGL->glGetShaderInfoLog( glslDesc->m_object.glsl, sizeof(log), &maxLength, log );
				Msg("shader compile log: %s\n", log);
				Msg("Shader %d source is:\n===============\n%s\nn===============\n", glslDesc->m_object.glsl, section);											
			}

#if 0 //GLM_FREE_SHADER_TEXT
			// Free the shader program text - not needed anymore (GL has its own copy)
			if ( m_text && !m_descs[kGLMARB].m_textPresent )
			{
				free( m_text );
				m_text = NULL;
			}
#endif

			glslDesc->m_compiled = true;	// compiled but not necessarily valid

			// Check shader validity at creation time.  This will cause the driver to not be able to
			// multi-thread/defer shader compiles, but it is useful for getting error messages on the
			// shader when it is compiled
			bool bValidateShaderEarly = (CommandLine()->FindParm( "-gl_validate_shader_early" ) != 0);
			if (bValidateShaderEarly)
			{
				CheckValidity( lang );
			}

			if (loglevel>=2)
			{
				char tempname[128];
				//int tempindex = -1;
				//int tempcombo = -1;

				//GetLabelIndexCombo( tempname, sizeof(tempname), &tempindex, &tempcombo );
				//printf("\ncompile: - [ %s/%d/%d ] on GL name %d ", tempname, tempindex, tempcombo, glslDesc->m_object.glsl );
				

				GetComboIndexNameString( tempname, sizeof(tempname) );
				printf("\ncompile: %s on GL name %d ", tempname, glslDesc->m_object.glsl );
			}
		}
		break;
	}

	if (bTimeShaderCompiles)
	{
		shaderCompileTimer.End();
		gShaderCompileTime += shaderCompileTimer.GetDuration();
		gShaderCompileCount++;
	}
}

#if GLMDEBUG

	bool CGLMProgram::PollForChanges( void )
	{
		bool result = false;
		if (m_editable)
		{
			result = m_editable->PollForChanges();
		}
		return result;
	}

	void	CGLMProgram::ReloadStringFromEditable( void )
	{
		uint	dataSize=0;
		char	*data=NULL;
		
		m_editable->GetCurrentText( &data, &dataSize );
		
		char *buf = (char *)malloc( dataSize+1 );	// we will NULL terminate it, since the mirror copy might not be
		memcpy( buf, data, dataSize );
		buf[dataSize] = 0;
		
		SetProgramText( buf );
		
		free( buf );
	}

	bool	CGLMProgram::SyncWithEditable( void )
	{
		bool result = false;
		
		if (m_editable->PollForChanges())
		{
			ReloadStringFromEditable();

			CompileActiveSources();
			
			// invalidate shader pair cache entries using this shader..
			m_ctx->m_pairCache->PurgePairsWithShader( this );
			
			result = true;	// result true means "it changed"
		}
		return result;
	}
	
#endif


// attributes which are general to both stages
//	VP and FP:
//	
//	0x88A0         PROGRAM_INSTRUCTIONS_ARB                         VP  FP
//	0x88A1         MAX_PROGRAM_INSTRUCTIONS_ARB                     VP  FP
//	0x88A2         PROGRAM_NATIVE_INSTRUCTIONS_ARB                  VP  FP
//	0x88A3         MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB              VP  FP
//	
//	0x88A4         PROGRAM_TEMPORARIES_ARB                          VP  FP
//	0x88A5         MAX_PROGRAM_TEMPORARIES_ARB                      VP  FP
//	0x88A6         PROGRAM_NATIVE_TEMPORARIES_ARB                   VP  FP
//	0x88A7         MAX_PROGRAM_NATIVE_TEMPORARIES_ARB               VP  FP
//	
//	0x88A8         PROGRAM_PARAMETERS_ARB                           VP  FP
//	0x88A9         MAX_PROGRAM_PARAMETERS_ARB                       VP  FP
//	0x88AA         PROGRAM_NATIVE_PARAMETERS_ARB                    VP  FP
//	0x88AB         MAX_PROGRAM_NATIVE_PARAMETERS_ARB                VP  FP
//	
//	0x88AC         PROGRAM_ATTRIBS_ARB                              VP  FP
//	0x88AD         MAX_PROGRAM_ATTRIBS_ARB                          VP  FP
//	0x88AE         PROGRAM_NATIVE_ATTRIBS_ARB                       VP  FP
//	0x88AF         MAX_PROGRAM_NATIVE_ATTRIBS_ARB                   VP  FP
//	
//	0x88B4         MAX_PROGRAM_LOCAL_PARAMETERS_ARB                 VP  FP
//	0x88B5         MAX_PROGRAM_ENV_PARAMETERS_ARB                   VP  FP
//	0x88B6         PROGRAM_UNDER_NATIVE_LIMITS_ARB                  VP  FP
//
//	VP only:
//	
//	0x88B0         PROGRAM_ADDRESS_REGISTERS_ARB                    VP
//	0x88B1         MAX_PROGRAM_ADDRESS_REGISTERS_ARB                VP
//	0x88B2         PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB             VP
//	0x88B3         MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB         VP
//	
//	FP only:
//	
//	0x8805         PROGRAM_ALU_INSTRUCTIONS_ARB                         FP
//	0x880B         MAX_PROGRAM_ALU_INSTRUCTIONS_ARB                     FP
//	0x8808         PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB                  FP
//	0x880E         MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB              FP

//	0x8806         PROGRAM_TEX_INSTRUCTIONS_ARB                         FP
//	0x880C         MAX_PROGRAM_TEX_INSTRUCTIONS_ARB                     FP
//	0x8809         PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB                  FP
//	0x880F         MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB              FP

//	0x8807         PROGRAM_TEX_INDIRECTIONS_ARB                         FP
//	0x880D         MAX_PROGRAM_TEX_INDIRECTIONS_ARB                     FP
//	0x880A         PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB                  FP
//	0x8810         MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB              FP

struct GLMShaderLimitDesc
{
	GLenum	m_valueEnum;
	GLenum	m_limitEnum;
	const char	*m_debugName;
	char	m_flags;
	// m_flags - 0x01 for VP, 0x02 for FP, or set both if applicable to both
};

// macro to help make the table of what to check
#ifndef	LMD
#define	LMD( val, flags )	{ GL_PROGRAM_##val, GL_MAX_PROGRAM_##val, #val, flags }
#else
#error you need to use a different name for this macro.
#endif

GLMShaderLimitDesc	g_glmShaderLimitDescs[] = 
{
	// VP and FP..
//	LMD( INSTRUCTIONS,				3 ),
//	LMD( NATIVE_INSTRUCTIONS,		3 ),
//	LMD( NATIVE_TEMPORARIES,		3 ),
//	LMD( PARAMETERS,				3 ),
//	LMD( NATIVE_PARAMETERS,			3 ),
//	LMD( ATTRIBS,					3 ),
//	LMD( NATIVE_ATTRIBS,			3 ),

	// VP only..
//	LMD( ADDRESS_REGISTERS,			1 ),
//	LMD( NATIVE_ADDRESS_REGISTERS,	1 ),

	// FP only..
//	LMD( ALU_INSTRUCTIONS,			2 ),
//	LMD( NATIVE_ALU_INSTRUCTIONS,	2 ),
//	LMD( TEX_INSTRUCTIONS,			2 ),
//	LMD( NATIVE_TEX_INSTRUCTIONS,	2 ),
//	LMD( TEX_INDIRECTIONS,			2 ),
//	LMD( NATIVE_TEX_INDIRECTIONS,	2 ),
	
	{ 0, 0, NULL, 0 }
};

#undef LMD

bool CGLMProgram::CheckValidity( EGLMProgramLang lang )
{
	static char *targnames[] = { "vertex", "fragment" };

	bool bTimeShaderCompiles = (CommandLine()->FindParm( "-gl_time_shader_compiles" ) != 0);
	// If using "-gl_time_shader_compiles", keeps track of total cycle count spent on shader compiles.
	CFastTimer shaderCompileTimer;
	if (bTimeShaderCompiles)
	{
		shaderCompileTimer.Start();
	}

	bool bValid = false;

//#error "What the fuck?"

	switch(lang)
	{
		case kGLMGLSL:
		{
			GLMShaderDesc *glslDesc;
			glslDesc = &m_descs[ kGLMGLSL ];
			
			GLenum glslStage = GLMProgTypeToGLSLEnum( m_type ); glslStage;

			// When the program was loaded from a cached binary, the individual
			// shader objects were never compiled.  Just mark valid — the link
			// status of the program object is the real authority.
			if ( !glslDesc->m_compiled )
			{
				glslDesc->m_valid = true;
				return true;
			}
	
			glslDesc->m_valid = true;	// assume success til we see otherwise

			// GLSL error check
			int compiled = 0;

			gGL->glGetShaderiv(glslDesc->m_object.glsl, GL_COMPILE_STATUS, &compiled);

			if (!compiled)
				glslDesc->m_valid = false;

			bValid = glslDesc->m_valid;
		}
		break;
	}

	AssertOnce( bValid );

	if (bTimeShaderCompiles)
	{
		shaderCompileTimer.End();
		gShaderCompileQueryTime += shaderCompileTimer.GetDuration();
	}

	return bValid;
}

void	CGLMProgram::LogSlow( EGLMProgramLang lang )
{
	// find the desc, see if it's marked
	GLMShaderDesc *desc = &m_descs[ lang ];

	if (!desc->m_slowMark)
	{
#if !GLM_FREE_SHADER_TEXT
		// log it
		printf(	"\n-------------- Slow %s ( CGLMProgram @ %p, lang %s, name %d ) : \n%s \n",
				m_type==kGLMVertexProgram ? "VS" : "FS",
				this,
				lang==kGLMGLSL ? "GLSL" : "ARB",
				(int)(lang==kGLMGLSL ? (int)desc->m_object.glsl : (int)desc->m_object.arb),
				m_text
		);
#endif
	}
	else	// complain on a decreasing basis (powers of two)
	{
		if ( (desc->m_slowMark & (desc->m_slowMark-1)) == 0 )
		{
			// short blurb
			printf(	"\n               Slow %s ( CGLMProgram @ %p, lang %s, name %d ) (%d times)",
					m_type==kGLMVertexProgram ? "VS" : "FS",
					this,
					lang==kGLMGLSL ? "GLSL" : "ARB",
					(int)(lang==kGLMGLSL ? (int)desc->m_object.glsl : (int)desc->m_object.arb),
					desc->m_slowMark+1
			);
		}
	}

	// mark it
	desc->m_slowMark++;
		

}

void	CGLMProgram::GetLabelIndexCombo		( char *labelOut, int labelOutMaxChars, int *indexOut, int *comboOut )
{
	// find the label string
	// example:
	// trans#2871 label:vs-file vertexlit_and_unlit_generic_vs20 vs-index 294912 vs-combo 1234
	// Done in SetProgramText
	
	*labelOut = 0;
	*indexOut = -1;

	if ((strlen( m_labelName ) != 0))
	{
		Q_strncpy( labelOut, m_labelName, labelOutMaxChars );
		*indexOut = m_labelIndex;
		*comboOut = m_labelCombo;
	}
}

void	CGLMProgram::GetComboIndexNameString	( char *stringOut, int stringOutMaxChars )		// mmmmmmmm-nnnnnnnn-filename
{
	// find the label string
	// example:
	// trans#2871 label:vs-file vertexlit_and_unlit_generic_vs20 vs-index 294912 vs-combo 1234
	// Done in SetProgramText
	
	*stringOut = 0;

	int len = strlen( m_labelName );
		
	if ( (len+20) < stringOutMaxChars )
	{
		// output formatted version
		sprintf( stringOut, "%08X-%08X-%s", m_labelCombo, m_labelIndex, m_labelName );
	}
}

//===============================================================================


CGLMShaderPair::CGLMShaderPair( GLMContext *ctx  )
{
	m_ctx = ctx;
	m_vertexProg = m_fragmentProg = NULL;

	m_program = gGL->glCreateProgram();

	m_locVertexParams = -1;
	m_locAlphaRef = -1;
	m_alphaRefValue = -1.0f;
	m_locVertexBoneParams = -1;
	m_locVertexScreenParams = -1;
	m_nScreenWidthHeight = 0xFFFFFFFF;
	m_locClipPlane0 = -1;
	m_locClipPlane1 = -1;
	memset( m_flClipPlaneUploaded, 0, sizeof( m_flClipPlaneUploaded ) );
	memset( m_bClipPlaneUploaded, 0, sizeof( m_bClipPlaneUploaded ) );
	m_nClipPlaneStateRevision = 0xFFFFFFFF;
	m_locVertexInteger0 = -1;	// "i0"
	memset( m_locVertexBool, 0xFF, sizeof( m_locVertexBool ) );
	memset( m_locFragmentBool, 0xFF, sizeof( m_locFragmentBool ) );
	m_bHasBoolOrIntUniforms = false;
	
	m_locFragmentParams = -1;
	
	m_locFragmentFakeSRGBEnable = -1;
	m_fakeSRGBEnableValue = -1.0f;
	
	memset( m_locSamplers, 0xFF, sizeof( m_locSamplers ) );
	
	m_valid = false;
	m_bCheckLinkStatus = false;
	m_revision = 0;				// bumps to 1 once linked
	m_nProgramParamRevisionEpoch = 0;
	memset( m_uploadedProgramParamRevisionF, 0xFF, sizeof( m_uploadedProgramParamRevisionF ) );
	memset( m_uploadedProgramParamRevisionB, 0xFF, sizeof( m_uploadedProgramParamRevisionB ) );
	memset( m_uploadedProgramParamRevisionI, 0xFF, sizeof( m_uploadedProgramParamRevisionI ) );
}

CGLMShaderPair::~CGLMShaderPair( )
{
	if (m_program)
	{
		gGL->glDeleteProgram( m_program );
		m_program = 0;
	}
}

bool CGLMShaderPair::ValidateProgramPair()
{
	if ( m_vertexProg && m_vertexProg->m_descs[kGLMGLSL].m_textPresent && !m_vertexProg->m_descs[kGLMGLSL].m_valid )
	{
		m_vertexProg->CheckValidity( kGLMGLSL );
	}
	if (m_fragmentProg && m_fragmentProg->m_descs[kGLMGLSL].m_textPresent && !m_fragmentProg->m_descs[kGLMGLSL].m_valid)
	{
		m_fragmentProg->CheckValidity( kGLMGLSL );
	}
	
	if ( !m_valid && m_bCheckLinkStatus )
	{
		bool bTimeShaderCompiles = (CommandLine()->FindParm( "-gl_time_shader_compiles" ) != 0);
		// If using "-gl_time_shader_compiles", keeps track of total cycle count spent on shader compiles.
		CFastTimer shaderCompileTimer;
		if (bTimeShaderCompiles)
		{
			shaderCompileTimer.Start();
		}

		// check for success
		GLint result = GL_TRUE;
		gGL->glGetProgramiv(m_program, GL_LINK_STATUS, &result);
		m_bCheckLinkStatus = false;

		if (result == GL_TRUE)
		{
			// success

			m_valid = true;
			m_revision++;
		}
		else
		{
			GLint length = 0;
			GLint laux = 0;

			// do some digging
#if !GLM_FREE_SHADER_TEXT
			char *vtemp = strdup( m_vertexProg->m_text );
			vtemp[m_vertexProg->m_descs[kGLMGLSL].m_textOffset + m_vertexProg->m_descs[kGLMGLSL].m_textLength] = 0;

			char *ftemp = strdup( m_fragmentProg->m_text );
			ftemp[m_fragmentProg->m_descs[kGLMGLSL].m_textOffset + m_fragmentProg->m_descs[kGLMGLSL].m_textLength] = 0;

			GLMPRINTF( ("-D- ----- GLSL vertex program selected: %08x (handle %08x)", m_vertexProg, m_vertexProg->m_descs[kGLMGLSL].m_object.glsl) );
			GLMPRINTTEXT( (vtemp + m_vertexProg->m_descs[kGLMGLSL].m_textOffset, eDebugDump, GLMPRINTTEXT_NUMBEREDLINES) );

			GLMPRINTF( ("-D- ----- GLSL fragment program selected: %08x (handle %08x)", m_fragmentProg, m_fragmentProg->m_descs[kGLMGLSL].m_object.glsl) );
			GLMPRINTTEXT( (ftemp + m_fragmentProg->m_descs[kGLMGLSL].m_textOffset, eDebugDump, GLMPRINTTEXT_NUMBEREDLINES) );

			free( ftemp );
			free( vtemp );
#endif
			GLMPRINTF( ("-D- -----end-----") );
		}

		if (m_valid)
		{
			gGL->glUseProgram( m_program );

			m_ctx->NewLinkedProgram();

			m_locVertexParams = gGL->glGetUniformLocation( m_program, "vc" );
			m_locVertexBoneParams = gGL->glGetUniformLocation( m_program, "vcbones" );
			m_locVertexScreenParams = gGL->glGetUniformLocation( m_program, "vcscreen" );
			m_locClipPlane0 = gGL->glGetUniformLocation( m_program, "uClipPlane0" );
			m_locClipPlane1 = gGL->glGetUniformLocation( m_program, "uClipPlane1" );
			if( !gGL->m_bHave_GL_QCOM_alpha_test )
				m_locAlphaRef = gGL->glGetUniformLocation( m_program, "alpha_ref" );
			m_alphaRefValue = -1.0f;

			m_nScreenWidthHeight = 0xFFFFFFFF;
			memset( m_flClipPlaneUploaded, 0, sizeof( m_flClipPlaneUploaded ) );
			memset( m_bClipPlaneUploaded, 0, sizeof( m_bClipPlaneUploaded ) );
			m_nClipPlaneStateRevision = 0xFFFFFFFF;

			m_locVertexInteger0 = gGL->glGetUniformLocation( m_program, "i0" );

			m_bHasBoolOrIntUniforms = false;
			if (m_locVertexInteger0 >= 0)
				m_bHasBoolOrIntUniforms = true;

			for (uint i = 0; i < cMaxVertexShaderBoolUniforms; i++)
			{
				char buf[256];
				V_snprintf( buf, sizeof(buf), "b%d", i );
				m_locVertexBool[i] = gGL->glGetUniformLocation( m_program, buf );
				if (m_locVertexBool[i] != -1)
					m_bHasBoolOrIntUniforms = true;
			}

			for (uint i = 0; i < cMaxFragmentShaderBoolUniforms; i++)
			{
				char buf[256];
				V_snprintf( buf, sizeof(buf), "fb%d", i );
				m_locFragmentBool[i] = gGL->glGetUniformLocation( m_program, buf );
				if (m_locFragmentBool[i] != -1)
					m_bHasBoolOrIntUniforms = true;
			}

			m_locFragmentParams = gGL->glGetUniformLocation( m_program, "pc" );

			// No per-element location queries: for a uniform array the elements
			// occupy contiguous locations (GL spec), so the flush addresses
			// vc[i]/pc[i] as m_locVertexParams/m_locFragmentParams + i. This
			// removes ~256 glGetUniformLocation driver round-trips per program,
			// which dominated the startup precache (267 pairs x ~500 calls).
			m_NumUniformBufferParams[0] = m_NumUniformBufferParams[1] = 0;

			m_locFragmentFakeSRGBEnable = gGL->glGetUniformLocation( m_program, "flSRGBWrite" );
			m_fakeSRGBEnableValue = -1.0f;

			for (int sampler = 0; sampler < 16; sampler++)
			{
				char tmp[16];
				sprintf( tmp, "sampler%d", sampler );	// sampler0 .. sampler1.. etc

				GLint nLoc = gGL->glGetUniformLocation( m_program, tmp );
				m_locSamplers[sampler] = nLoc;
				if (nLoc >= 0)
				{
					gGL->glUniform1i( nLoc, sampler );
				}
			}
		}
		else
		{
			m_locVertexParams = -1;
			m_locAlphaRef = -1;
			m_alphaRefValue = -1.0f;
			m_locVertexBoneParams = -1;
			m_locVertexScreenParams = -1;
			m_locClipPlane0 = -1;
			m_locClipPlane1 = -1;
			m_nScreenWidthHeight = 0xFFFFFFFF;
			memset( m_flClipPlaneUploaded, 0, sizeof( m_flClipPlaneUploaded ) );
			memset( m_bClipPlaneUploaded, 0, sizeof( m_bClipPlaneUploaded ) );
			m_nClipPlaneStateRevision = 0xFFFFFFFF;

			m_locVertexInteger0 = -1;
			memset( m_locVertexBool, 0xFF, sizeof(m_locVertexBool) );
			memset( m_locFragmentBool, 0xFF, sizeof(m_locFragmentBool) );
			m_bHasBoolOrIntUniforms = false;

			m_locFragmentParams = -1;
			m_locFragmentFakeSRGBEnable = -1;
			m_fakeSRGBEnableValue = -999;

			memset( m_locSamplers, 0xFF, sizeof(m_locSamplers) );

			m_revision = 0;
		}

		if (bTimeShaderCompiles)
		{
			shaderCompileTimer.End();
			gShaderLinkQueryTime += shaderCompileTimer.GetDuration();
		}
	}
	
	return m_valid;
}

// glProgramBinary on-disk cache.
//
// Mali's GLSL compiler is slow; without a binary cache every launch re-links
// every shader pair from source, producing long load hitches and first-frame
// stutter.  GLES 3.0 exposes glGetProgramBinary / glProgramBinary, so we cache
// the driver-native program binary to disk keyed on an MD5 of (driver version
// string + renderer string + vertex source + fragment source).  The driver
// string is part of the key so a driver/GPU change invalidates the cache
// automatically.  If glProgramBinary ever fails (corrupt/stale binary), we fall
// back to the normal source attach+link path, so this is always safe.
// -----------------------------------------------------------------------------
// gl_program_binary_cache: 0 = off, 1 = on, 2 = on + verbose per-pair logging.
// On warm starts every pair hit skips shader source attach+compile+link, which
// is the dominant cost on Mali's slow compiler.
static ConVar gl_program_binary_cache( "gl_program_binary_cache", "1", FCVAR_NONE, "Cache compiled GL program binaries to disk to skip relinking on startup (0=off, 1=on, 2=on+verbose)" );

static int s_nProgramBinaryHits = 0;
static int s_nProgramBinaryMisses = 0;
static int s_nProgramBinarySaves = 0;

static void ReportProgramBinaryCacheStats( const char *pszPairName, bool bHit, uint nMicros )
{
	if ( bHit )
		++s_nProgramBinaryHits;
	else
		++s_nProgramBinaryMisses;

	const int nTotal = s_nProgramBinaryHits + s_nProgramBinaryMisses;
	if ( gl_program_binary_cache.GetInt() >= 2 )
	{
		Msg( "[glshadercache] %s %s in %u us (hits=%d misses=%d)\n",
			pszPairName ? pszPairName : "?",
			bHit ? "HIT" : "MISS", nMicros,
			s_nProgramBinaryHits, s_nProgramBinaryMisses );
	}
	if ( ( nTotal % 256 ) == 0 )
	{
		Msg( "[glshadercache] cumulative: %d hits, %d misses, %d saves (hit rate %.1f%%)\n",
			s_nProgramBinaryHits, s_nProgramBinaryMisses, s_nProgramBinarySaves,
			s_nProgramBinaryHits * 100.0 / nTotal );
	}
}

#define GL_PROGRAM_BINARY_CACHE_DIR "glshadercache"

static void ComputeShaderPairHash( CGLMProgram *vp, CGLMProgram *fp, MD5Value_t &outHash )
{
	MD5Context_t ctx;
	MD5Init( &ctx );

	// Fold the driver version + renderer strings into the key so a driver update
	// or different GPU invalidates the cache (binaries are not portable).
	const char *versionStr  = gGL->m_pGLDriverStrings[cGLVersionString]  ? gGL->m_pGLDriverStrings[cGLVersionString]  : "";
	const char *rendererStr = gGL->m_pGLDriverStrings[cGLRendererString] ? gGL->m_pGLDriverStrings[cGLRendererString] : "";
	MD5Update( &ctx, (const unsigned char *)versionStr,  (unsigned int)V_strlen( versionStr ) );
	MD5Update( &ctx, (const unsigned char *)rendererStr, (unsigned int)V_strlen( rendererStr ) );

	// Hash the actual GLSL source text of both shaders (offset+length into m_text).
	for ( int pass = 0; pass < 2; ++pass )
	{
		CGLMProgram *p = (pass == 0) ? vp : fp;
		if ( !p || !p->m_text )
			continue;
		GLMShaderDesc *desc = &p->m_descs[kGLMGLSL];
		const char *section = p->m_text + desc->m_textOffset;
		MD5Update( &ctx, (const unsigned char *)section, (unsigned int)desc->m_textLength );
	}

	MD5Final( outHash.bits, &ctx );
}

static void ShaderPairCacheFileName( const MD5Value_t &hash, char *out, int outLen )
{
	char hex[33];
	for ( int i = 0; i < MD5_DIGEST_LENGTH; ++i )
	{
		V_snprintf( hex + i*2, sizeof(hex) - i*2, "%02x", hash.bits[i] );
	}
	hex[32] = 0;
	V_snprintf( out, outLen, "%s/%s.bin", GL_PROGRAM_BINARY_CACHE_DIR, hex );
}

// Attempt to load a cached binary into m_program.  Returns true on success
// (program is linked and ready); false on any failure (caller falls back to
// source attach+link).  The GL context must be current.
static bool LoadCachedProgramBinary( GLuint program, const MD5Value_t &hash )
{
	if ( !gGL->glProgramBinary )
		return false;

	char path[MAX_PATH];
	ShaderPairCacheFileName( hash, path, sizeof(path) );

	if ( !g_pFullFileSystem->FileExists( path, "MOD" ) )
		return false;

	FileHandle_t fh = g_pFullFileSystem->Open( path, "rb", "MOD" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
		return false;

	// File layout: 4-byte GLenum binaryFormat, then the binary blob.
	GLenum binaryFormat = 0;
	if ( g_pFullFileSystem->Read( &binaryFormat, sizeof(binaryFormat), fh ) != sizeof(binaryFormat) )
	{
		g_pFullFileSystem->Close( fh );
		return false;
	}

	int binLen = g_pFullFileSystem->Size( fh ) - (int)sizeof(binaryFormat);
	if ( binLen <= 0 )
	{
		g_pFullFileSystem->Close( fh );
		return false;
	}

	void *binary = malloc( binLen );
	if ( !binary )
	{
		g_pFullFileSystem->Close( fh );
		return false;
	}

	if ( g_pFullFileSystem->Read( binary, binLen, fh ) != binLen )
	{
		free( binary );
		g_pFullFileSystem->Close( fh );
		return false;
	}
	g_pFullFileSystem->Close( fh );

	gGL->glProgramBinary( program, binaryFormat, binary, binLen );
	free( binary );

	// Verify the binary actually linked.  If not, the binary was stale/corrupt
	// (e.g. driver upgrade with a colliding hash path) -> caller falls back.
	GLint linked = GL_FALSE;
	gGL->glGetProgramiv( program, GL_LINK_STATUS, &linked );
	return linked == GL_TRUE;
}

// After a successful source link, retrieve the program binary and persist it.
// Best-effort: silently ignores any failure.
static void SaveCachedProgramBinary( GLuint program, const MD5Value_t &hash )
{
	if ( !gGL->glGetProgramBinary )
		return;

	GLint binLen = 0;
	gGL->glGetProgramiv( program, GL_PROGRAM_BINARY_LENGTH, &binLen );
	if ( binLen <= 0 )
		return;

	void *binary = malloc( binLen );
	if ( !binary )
		return;

	GLenum binaryFormat = 0;
	GLsizei written = 0;
	gGL->glGetProgramBinary( program, binLen, &written, &binaryFormat, binary );
	if ( written <= 0 )
	{
		free( binary );
		return;
	}

	g_pFullFileSystem->CreateDirHierarchy( GL_PROGRAM_BINARY_CACHE_DIR, "MOD" );

	char path[MAX_PATH];
	ShaderPairCacheFileName( hash, path, sizeof(path) );
	FileHandle_t fh = g_pFullFileSystem->Open( path, "wb", "MOD" );
	if ( fh != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Write( &binaryFormat, sizeof(binaryFormat), fh );
		g_pFullFileSystem->Write( binary, written, fh );
		g_pFullFileSystem->Close( fh );
	}
	free( binary );
}

// glUseProgram() will be called as a side effect!
bool CGLMShaderPair::SetProgramPair( CGLMProgram *vp, CGLMProgram *fp )
{
	bool bTimeShaderCompiles = (CommandLine()->FindParm( "-gl_time_shader_compiles" ) != 0);
	// If using "-gl_time_shader_compiles", keeps track of total cycle count spent on shader compiles.
	CFastTimer shaderCompileTimer;
	if (bTimeShaderCompiles)
	{
		shaderCompileTimer.Start();
	}
	
	m_valid	= false;			// assume failure
	// Linking creates fresh uniform storage even when the CGLMShaderPair object
	// itself is reused.  Force one complete refresh in the context's current
	// revision epoch before any pair-local comparisons are trusted.
	m_nProgramParamRevisionEpoch = 0;
	memset( m_uploadedProgramParamRevisionF, 0xFF, sizeof( m_uploadedProgramParamRevisionF ) );
	memset( m_uploadedProgramParamRevisionB, 0xFF, sizeof( m_uploadedProgramParamRevisionB ) );
	memset( m_uploadedProgramParamRevisionI, 0xFF, sizeof( m_uploadedProgramParamRevisionI ) );
	
	// No need to check that vp and fp are valid at this point (ie shader compile succeed)
	// It is permissible to attach a shader object to a program before source code has been loaded
	// into the shader object or before the shader object has been compiled. The program won't 
	// link if one or more of the shader objects has not been successfully compiled.
	// (Defer querying the compile and link status to take advantage of GLSL shaders
	// building in parallels)
	bool vpgood = (vp != NULL);
	bool fpgood = (fp != NULL);
	
	if ( !fpgood )
	{
		// fragment side allowed to be "null".
		fp = m_ctx->m_pNullFragmentProgram;
	}

	if ( vpgood && fpgood )
	{
		if ( vp->m_nCentroidMask != fp->m_nCentroidMask )
		{
			Warning( "CGLMShaderPair::SetProgramPair: Centroid masks differ at link time of vertex shader %s and pixel shader %s!\n", 
				vp->m_shaderName, fp->m_shaderName );
		}

		// attempt link. but first, detach any previously attached programs
		if (m_vertexProg)
		{
			gGL->glDetachShader(m_program, m_vertexProg->m_descs[kGLMGLSL].m_object.glsl);
			m_vertexProg = NULL;			
		}
		
		if (m_fragmentProg)
		{
			gGL->glDetachShader(m_program, m_fragmentProg->m_descs[kGLMGLSL].m_object.glsl);
			m_fragmentProg = NULL;			
		}

		// Record the pair now (needed by the uniform-location query path even
		// when we link from a cached binary, since it reads m_vertexProg/m_fragmentProg).
		m_vertexProg = vp;
		m_fragmentProg = fp;

		// force the locations for input attributes v0-vN to be at locations 0-N
		// use the vertex attrib map to know which slots are live or not... oy!  we don't have that map yet... but it's OK.
		// fallback - just force v0-v15 to land in locations 0-15 as a standard.
		// (Must be done before BOTH glLinkProgram and glProgramBinary.)
		for( int i = 0; i < 16; i++ )
		{
			char tmp[16];
			sprintf(tmp, "v%d", i);	// v0 v1 v2 ... et al
				
			gGL->glBindAttribLocation( m_program, i, tmp );
		}

		// Try to load a cached, driver-native program binary first.  This skips
		// the source attach + compile + link entirely on warm starts, which is
		// the dominant cost on Mali's slow compiler.  On any failure (no cache
		// file, stale/corrupt binary, driver changed) we fall through to the
		// normal source attach+link path, so this is always safe.
		bool bUsedBinaryCache = false;
		if ( gl_program_binary_cache.GetInt() && gGL->glProgramBinary && gGL->glGetProgramBinary )
		{
			MD5Value_t pairHash;
			ComputeShaderPairHash( vp, fp, pairHash );

			CFastTimer binaryCacheTimer;
			binaryCacheTimer.Start();
			const bool bLoaded = LoadCachedProgramBinary( m_program, pairHash );
			binaryCacheTimer.End();
			ReportProgramBinaryCacheStats( vp->m_shaderName, bLoaded, binaryCacheTimer.GetDuration().GetMicroseconds() );

			if ( bLoaded )
			{
				bUsedBinaryCache = true;
				m_bCheckLinkStatus = true;	// ValidateProgramPair will confirm m_valid + query uniforms
			}
		}

		if ( !bUsedBinaryCache )
		{
#if !GLM_FREE_SHADER_TEXT
			if (CommandLine()->CheckParm("-dumpallshaders"))
			{
				// Dump all shaders, for debugging.
				FILE* pFile = fopen("shaderdump.txt", "a+");
				if (pFile)
				{
					fprintf(pFile, "--------------VP:%s\n%s\n", vp->m_shaderName, vp->m_text);
					fprintf(pFile, "--------------FP:%s\n%s\n", fp->m_shaderName, fp->m_text);
					fclose(pFile);
				}
			}
#endif

			// Deferred compile: NewProgram skipped CompileActiveSources().
			// We only need the compiled shader objects on the cache-miss path.
			if ( !vp->m_descs[kGLMGLSL].m_compiled )
				vp->CompileActiveSources();
			if ( !fp->m_descs[kGLMGLSL].m_compiled )
				fp->CompileActiveSources();

			// now attach
			gGL->glAttachShader( m_program, vp->m_descs[kGLMGLSL].m_object.glsl );
			gGL->glAttachShader( m_program, fp->m_descs[kGLMGLSL].m_object.glsl );

			// now link
			gGL->glLinkProgram( m_program );

			GLint isLinked = 0;
			gGL->glGetProgramiv(m_program, GL_LINK_STATUS, &isLinked);
			if(isLinked == GL_FALSE)
			{
				GLint maxLength = 0;
				// m_program is a PROGRAM handle: glGetShaderiv would raise
				// GL_INVALID_VALUE here and the link-failure log would never
				// be fetched.
				gGL->glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &maxLength);

				GLchar  log[4096];
				gGL->glGetProgramInfoLog( m_program, sizeof(log), &maxLength, log );
				if( maxLength )
				{
					Msg("vp: \n%s\nfp: \n%s\n", vp->m_text, fp->m_text );
					Msg("shader %d link log: %s\n", m_program, log);
				}
			}
			else
			{
				// Link succeeded from source: persist the binary for next launch.
				if ( gl_program_binary_cache.GetInt() && gGL->glGetProgramBinary )
				{
					MD5Value_t pairHash;
					ComputeShaderPairHash( vp, fp, pairHash );
					SaveCachedProgramBinary( m_program, pairHash );
					++s_nProgramBinarySaves;
				}
			}
			
			m_bCheckLinkStatus = true;
		}
	}
	else
	{
		// fail
		Assert(!"Can't link these programs");
	}

	// Check shader validity at creation time.  This will cause the driver to not be able to
	// multi-thread/defer shader compiles, but it is useful for getting error messages on the
	// shader when it is compiled
	bool bValidateShaderEarly = (CommandLine()->FindParm( "-gl_validate_shader_early" ) != 0);
	if (bValidateShaderEarly)
	{
		ValidateProgramPair();
	}

	if (bTimeShaderCompiles)
	{
		shaderCompileTimer.End();
		gShaderLinkTime += shaderCompileTimer.GetDuration();
		gShaderLinkCount++;
	}

	return m_valid;
}


bool	CGLMShaderPair::RefreshProgramPair		( void )
{
	// re-link and re-query the uniforms.
	
	// since SetProgramPair knows how to detach previously attached shader objects, just pass the same ones in again.
	CGLMProgram	*vp = m_vertexProg;
	CGLMProgram	*fp = m_fragmentProg;
	
	bool vpgood = (vp!=NULL) && (vp->m_descs[ kGLMGLSL ].m_valid);
	bool fpgood = (fp!=NULL) && (fp->m_descs[ kGLMGLSL ].m_valid);

	if (vpgood && fpgood)
	{
		SetProgramPair( vp, fp );
	}
	else
	{
		DebuggerBreak();
		return false;
	}
	
	return false;
}


//===============================================================================

CGLMShaderPairCache::CGLMShaderPairCache( GLMContext *ctx  )
{
	m_ctx = ctx;
	
	m_mark = 1;
	
	m_rowsLg2 = gl_shaderpair_cacherows_lg2.GetInt();
	if (m_rowsLg2 < 10)
			m_rowsLg2 = 10;
	m_rows = 1<<m_rowsLg2;
	m_rowsMask = m_rows - 1;

	m_waysLg2 = gl_shaderpair_cacheways_lg2.GetInt();
	if ( V_stristr(gGL->m_pGLDriverStrings[cGLVendorString], "arm") != NULL )
	{
		// On mobile TBDR GPUs (Mali G31 etc.), 8 ways (lg2=3) is ample
		// and saves ~960 KiB per context (32,768→4,096 entries × 40 B).
		// The desktop default (lg2=5 → 32 ways) wastes RAM with no culling
		// benefit on low-vertex-count mobile workloads.  Users who set the
		// convar explicitly still get their value as long as it's ≥3.
		if (m_waysLg2 < 3)
			m_waysLg2 = 3;
		if (m_waysLg2 == 5)		// convar likely still at default "5"
			m_waysLg2 = 3;
	}
	else
	{
		if (m_waysLg2 < 5)
			m_waysLg2 = 5;
	}
	m_ways = 1<<m_waysLg2;

	m_entryCount = m_rows * m_ways;
	
	uint entryTableSize = m_rows * m_ways * sizeof(CGLMPairCacheEntry);
	m_entries = (CGLMPairCacheEntry*)malloc( entryTableSize );				// array[ m_rows ][ m_ways ]
	memset( m_entries, 0, entryTableSize );
	
	uint evictTableSize = m_rows * sizeof(uint);
	m_evictions = (uint*)malloc( evictTableSize );
	memset (m_evictions, 0, evictTableSize);

#if GL_SHADER_PAIR_CACHE_STATS
	// hit counter table is same size
	m_hits = (uint*)malloc( evictTableSize );
	memset (m_hits, 0, evictTableSize);
#endif
}

CGLMShaderPairCache::~CGLMShaderPairCache( )
{
	if (gl_shaderpair_cachelog.GetInt())
	{
		DumpStats();
	}

	// free all the built pairs
	// free the entry table
	bool purgeResult = this->Purge();
	(void)purgeResult;
	Assert( !purgeResult );
	
	if (m_entries)
	{
		free( m_entries );
		m_entries = NULL;
	}

	if (m_evictions)
	{
		free( m_evictions );
		m_evictions = NULL;
	}

#if GL_SHADER_PAIR_CACHE_STATS
	if (m_hits)
	{
		free( m_hits );
		m_hits = NULL;
	}
#endif
}

// Set this convar internally to build or add to the shader pair cache file (link hints)
// We really only expect this to work on POSIX
static ConVar glm_cacheprograms( "glm_cacheprograms", "0", FCVAR_DEVELOPMENTONLY );

#define PROGRAM_CACHE_FILE "program_cache.cfg"

static void WriteToProgramCache( CGLMShaderPair *pair )
{
	KeyValues *pProgramCache = new KeyValues( "programcache" );
	pProgramCache->LoadFromFile( g_pFullFileSystem, PROGRAM_CACHE_FILE, "MOD" );

	if ( !pProgramCache )
	{
		Warning( "Could not write to program cache file!\n" );
		return;
	}

	// extract values of interest which represent a pair of shaders
	
	char	vprogramName[128];
	int		vprogramStaticIndex = -1;
	int		vprogramDynamicIndex = -1;
	pair->m_vertexProg->GetLabelIndexCombo( vprogramName, sizeof(vprogramName), &vprogramStaticIndex, &vprogramDynamicIndex );

	
	char	pprogramName[128];
	int		pprogramStaticIndex = -1;
	int		pprogramDynamicIndex = -1;
	pair->m_fragmentProg->GetLabelIndexCombo( pprogramName, sizeof(pprogramName), &pprogramStaticIndex, &pprogramDynamicIndex );

	// make up a key - this thing is really a list of tuples, so need not be keyed by anything particular
	KeyValues *pProgramKey = pProgramCache->CreateNewKey();
	Assert( pProgramKey );

	pProgramKey->SetString	( "vs", vprogramName );
	pProgramKey->SetString	( "ps", pprogramName );

	pProgramKey->SetInt		( "vs_static", vprogramStaticIndex );
	pProgramKey->SetInt		( "ps_static", pprogramStaticIndex );

	pProgramKey->SetInt		( "vs_dynamic", vprogramDynamicIndex );
	pProgramKey->SetInt		( "ps_dynamic", pprogramDynamicIndex );

	pProgramCache->SaveToFile( g_pFullFileSystem, PROGRAM_CACHE_FILE, "MOD" );
	pProgramCache->deleteThis();
}

// Calls glUseProgram() as a side effect
CGLMShaderPair	*CGLMShaderPairCache::SelectShaderPairInternal( CGLMProgram *vp, CGLMProgram *fp, uint extraKeyBits, int rowIndex )
{
	CGLMShaderPair	*result = NULL;
		
#if GLMDEBUG
	int loglevel = gl_shaderpair_cachelog.GetInt();
#else
	const int loglevel = 0;
#endif

	char vtempname[128];
	int vtempindex = -1; vtempindex;
	int vtempcombo = -1; vtempcombo;

	char ptempname[128];
	int ptempindex = -1; ptempindex;
	int ptempcombo = -1; ptempcombo;
	
	CGLMPairCacheEntry *row = HashRowPtr( rowIndex );
	
	// Re-probe to find the oldest and first unoccupied entry (this func should be very rarely called if the cache is properly configured so re-scanning shouldn't matter).
	int hitway, emptyway, oldestway;
	HashRowProbe( row, vp, fp, extraKeyBits, hitway, emptyway, oldestway );
	Assert( hitway == -1 );

	// we missed.  if there is no empty way, then somebody's getting evicted.
	int destway = -1;
		
	if (emptyway>=0)
	{			
		destway = emptyway;

		if (loglevel >= 2)  // misses logged at level 3 and higher
		{
			printf("\nSSP: miss - row %05d - ", rowIndex );
		}
	}
	else
	{
		// evict the oldest way
		Assert( oldestway >= 0);	// better not come back negative
			
		CGLMPairCacheEntry *evict = row + oldestway;
			
		Assert( evict->m_pair != NULL );
		Assert( evict->m_pair != m_ctx->m_pBoundPair );	// just check
			
		///////////////////////FIXME may need to do a shoot-down if the pair being evicted is currently active in the context
			
		m_evictions[ rowIndex ]++;

		// log eviction if desired
		if (loglevel >= 2)  // misses logged at level 3 and higher
		{
			//evict->m_vertexProg->GetLabelIndexCombo( vtempname, sizeof(vtempname), &vtempindex, &vtempcombo );
			//evict->m_fragmentProg->GetLabelIndexCombo( ptempname, sizeof(ptempname), &ptempindex, &ptempcombo );
			//printf("\nSSP: miss - row %05d - [ %s/%d/%d %s/%d/%d ]'s %d'th eviction - ", rowIndex, vtempname, vtempindex, vtempcombo, ptempname, ptempindex, ptempcombo, m_evictions[ rowIndex ] );

			evict->m_vertexProg->GetComboIndexNameString( vtempname, sizeof(vtempname) );
			evict->m_fragmentProg->GetComboIndexNameString( ptempname, sizeof(ptempname) );				
			printf("\nSSP: miss - row %05d - [ %s + %s ]'s %d'th eviction - ", rowIndex, vtempname, ptempname, m_evictions[ rowIndex ] );
		}

		delete evict->m_pair;	evict->m_pair = NULL;
		memset( evict, 0, sizeof(*evict) );
			
		destway = oldestway;
	}

	// make the new entry
	CGLMPairCacheEntry *newentry = row + destway;
		
	newentry->m_lastMark = m_mark;
	newentry->m_vertexProg = vp;
	newentry->m_fragmentProg = fp;
	newentry->m_extraKeyBits = extraKeyBits;
	newentry->m_pair = new CGLMShaderPair( m_ctx );
	Assert( newentry->m_pair );
	newentry->m_pair->SetProgramPair( vp, fp );

	if (loglevel >= 2)  // say a little bit more
	{
		//newentry->m_vertexProg->GetLabelIndexCombo( vtempname, sizeof(vtempname), &vtempindex, &vtempcombo );
		//newentry->m_fragmentProg->GetLabelIndexCombo( ptempname, sizeof(ptempname), &ptempindex, &ptempcombo );			
		//printf("new [ %s/%d/%d %s/%d/%d ]", vtempname, vtempindex, vtempcombo, ptempname, ptempindex, ptempcombo );

		newentry->m_vertexProg->GetComboIndexNameString( vtempname, sizeof(vtempname) );
		newentry->m_fragmentProg->GetComboIndexNameString( ptempname, sizeof(ptempname) );				
		printf("new [ %s + %s ]", vtempname, ptempname );
	}

	m_mark = m_mark+1;
	if (!m_mark)		// somewhat unlikely this will ever be reached.. but we need to avoid zero as a mark value
	{
		m_mark = 1;
	}

	result = newentry->m_pair;

	if (glm_cacheprograms.GetInt())
	{
		WriteToProgramCache( newentry->m_pair );
	}
		
	return result;
}

void	CGLMShaderPairCache::QueryShaderPair( int index, GLMShaderPairInfo *infoOut )
{
	if ( (index<0) || ( static_cast<uint>(index) >= (m_rows*m_ways) ) )
	{
		// no such location
		memset( infoOut, 0, sizeof(*infoOut) );
		
		infoOut->m_status = -1;
	}
	else
	{
		// locate the entry, and see if an active pair is present.
		// if so, extract info and return with m_status=1.
		// if not, exit with m_status = 0.

		CGLMPairCacheEntry *entry = &m_entries[index];
		
		if (entry->m_pair)
		{
			// live
			// extract values of interest for caller

			entry->m_pair->m_vertexProg->GetLabelIndexCombo		( infoOut->m_vsName, sizeof(infoOut->m_vsName), &infoOut->m_vsStaticIndex, &infoOut->m_vsDynamicIndex );
			entry->m_pair->m_fragmentProg->GetLabelIndexCombo	( infoOut->m_psName, sizeof(infoOut->m_psName), &infoOut->m_psStaticIndex, &infoOut->m_psDynamicIndex );

			infoOut->m_status = 1;
		}
		else
		{
			// not
			memset( infoOut, 0, sizeof(*infoOut) );
			infoOut->m_status = 0;
		}
	}
}

bool CGLMShaderPairCache::PurgePairsWithShader( CGLMProgram *prog )
{
	bool result = false;
	
	// walk all rows*ways
	int limit = m_rows * m_ways;
	for( int i=0; i < limit; i++)
	{
		CGLMPairCacheEntry *entry = &m_entries[i];
		
		if (entry->m_pair)
		{
			//scrub it, if not currently bound, and if the supplied shader matches either stage
			if ( (entry->m_vertexProg==prog) || (entry->m_fragmentProg==prog) )
			{
				// found it, but does it conflict with bound pair ?
				if (entry->m_pair == m_ctx->m_pBoundPair)
				{
					m_ctx->m_pBoundPair = NULL;
					m_ctx->m_bDirtyPrograms = true;
				}
				delete entry->m_pair;
				memset( entry, 0, sizeof(*entry) );
			}
		}
	}
	return result;
}

bool CGLMShaderPairCache::Purge( void )
{
	bool result = false;
	
	// walk all rows*ways
	int limit = m_rows * m_ways;
	for( int i=0; i < limit; i++)
	{
		CGLMPairCacheEntry *entry = &m_entries[i];
		
		if (entry->m_pair)
		{
			//scrub it, unless the pair is the currently bound pair in our parent glm context
			if (entry->m_pair != m_ctx->m_pBoundPair)
			{
				delete entry->m_pair;
				memset( entry, 0, sizeof(*entry) );
			}
			else
			{
				result = true;
			}
		}
	}
	return result;
}
	
void			CGLMShaderPairCache::DumpStats			( void )
{
#if GL_SHADER_PAIR_CACHE_STATS
	printf("\n------------------\npair cache stats");
	int total = 0;
	for( uint row=0; row < m_rows; row++ )
	{
		if ( (m_evictions[row] != 0) || (m_hits[row] != 0) )
		{
			printf("\n row %d : %d evictions, %d hits",row,m_evictions[row], m_hits[row]);
			total += m_evictions[row];
		}
	}
	printf("\n\npair cache evictions: %d\n-----------------------\n",total );
#endif
}
	
	//===============================


