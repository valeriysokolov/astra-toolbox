/*
-----------------------------------------------------------------------
Copyright: 2010-2014, iMinds-Vision Lab, University of Antwerp
                2014, CWI, Amsterdam

Contact: astra@uantwerpen.be
Website: http://sf.net/projects/astra-toolbox

This file is part of the ASTRA Toolbox.


The ASTRA Toolbox is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

The ASTRA Toolbox is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the ASTRA Toolbox. If not, see <http://www.gnu.org/licenses/>.

-----------------------------------------------------------------------
$Id$
*/

#include "astra/CudaSirtAlgorithm3D.h"

#include <boost/lexical_cast.hpp>

#include "astra/AstraObjectManager.h"

#include "astra/ConeProjectionGeometry3D.h"
#include "astra/ParallelProjectionGeometry3D.h"
#include "astra/ParallelVecProjectionGeometry3D.h"
#include "astra/ConeVecProjectionGeometry3D.h"

#include "../cuda/3d/astra3d.h"

using namespace std;

namespace astra {

// type of the algorithm, needed to register with CAlgorithmFactory
std::string CCudaSirtAlgorithm3D::type = "SIRT3D_CUDA";

//----------------------------------------------------------------------------------------
// Constructor
CCudaSirtAlgorithm3D::CCudaSirtAlgorithm3D() 
{
	m_bIsInitialized = false;
	m_pSirt = 0;
	m_iGPUIndex = -1;
	m_iVoxelSuperSampling = 1;
	m_iDetectorSuperSampling = 1;
}

//----------------------------------------------------------------------------------------
// Constructor with initialization
CCudaSirtAlgorithm3D::CCudaSirtAlgorithm3D(CProjector3D* _pProjector, 
								   CFloat32ProjectionData3DMemory* _pProjectionData, 
								   CFloat32VolumeData3DMemory* _pReconstruction)
{
	_clear();
	initialize(_pProjector, _pProjectionData, _pReconstruction);
}

//----------------------------------------------------------------------------------------
// Destructor
CCudaSirtAlgorithm3D::~CCudaSirtAlgorithm3D() 
{
	delete m_pSirt;
	m_pSirt = 0;

	CReconstructionAlgorithm3D::_clear();
}


//---------------------------------------------------------------------------------------
// Check
bool CCudaSirtAlgorithm3D::_check()
{
	// check base class
	ASTRA_CONFIG_CHECK(CReconstructionAlgorithm3D::_check(), "SIRT3D", "Error in ReconstructionAlgorithm3D initialization");


	return true;
}

//---------------------------------------------------------------------------------------
// Initialize - Config
bool CCudaSirtAlgorithm3D::initialize(const Config& _cfg)
{
	ASTRA_ASSERT(_cfg.self);
	ConfigStackCheck<CAlgorithm> CC("CudaSirtAlgorithm3D", this, _cfg);


	// if already initialized, clear first
	if (m_bIsInitialized) {
		clear();
	}

	// initialization of parent class
	if (!CReconstructionAlgorithm3D::initialize(_cfg)) {
		return false;
	}

	m_iGPUIndex = (int)_cfg.self->getOptionNumerical("GPUindex", -1);
	CC.markOptionParsed("GPUindex");
	m_iDetectorSuperSampling = (int)_cfg.self->getOptionNumerical("DetectorSuperSampling", 1);
	CC.markOptionParsed("DetectorSuperSampling");
	m_iVoxelSuperSampling = (int)_cfg.self->getOptionNumerical("VoxelSuperSampling", 1);
	CC.markOptionParsed("VoxelSuperSampling");

	m_pSirt = new AstraSIRT3d();

	m_bAstraSIRTInit = false;


	// success
	m_bIsInitialized = _check();
	return m_bIsInitialized;
}

//----------------------------------------------------------------------------------------
// Initialize - C++
bool CCudaSirtAlgorithm3D::initialize(CProjector3D* _pProjector, 
								  CFloat32ProjectionData3DMemory* _pSinogram, 
								  CFloat32VolumeData3DMemory* _pReconstruction)
{
	// if already initialized, clear first
	if (m_bIsInitialized) {
		clear();
	}

	// required classes
	m_pProjector = _pProjector;
	m_pSinogram = _pSinogram;
	m_pReconstruction = _pReconstruction;

	m_pSirt = new AstraSIRT3d;

	m_bAstraSIRTInit = false;

	// success
	m_bIsInitialized = _check();
	return m_bIsInitialized;
}

//---------------------------------------------------------------------------------------
// Information - All
map<string,boost::any> CCudaSirtAlgorithm3D::getInformation() 
{
	map<string, boost::any> res;
	return mergeMap<string,boost::any>(CAlgorithm::getInformation(), res);
};

//---------------------------------------------------------------------------------------
// Information - Specific
boost::any CCudaSirtAlgorithm3D::getInformation(std::string _sIdentifier) 
{
	return CAlgorithm::getInformation(_sIdentifier);
};

//----------------------------------------------------------------------------------------
// Iterate
void CCudaSirtAlgorithm3D::run(int _iNrIterations)
{
	// check initialized
	ASTRA_ASSERT(m_bIsInitialized);

	const CProjectionGeometry3D* projgeom = m_pSinogram->getGeometry();
	const CConeProjectionGeometry3D* conegeom = dynamic_cast<const CConeProjectionGeometry3D*>(projgeom);
	const CParallelProjectionGeometry3D* par3dgeom = dynamic_cast<const CParallelProjectionGeometry3D*>(projgeom);
	const CParallelVecProjectionGeometry3D* parvec3dgeom = dynamic_cast<const CParallelVecProjectionGeometry3D*>(projgeom);
	const CConeVecProjectionGeometry3D* conevec3dgeom = dynamic_cast<const CConeVecProjectionGeometry3D*>(projgeom);
	const CVolumeGeometry3D& volgeom = *m_pReconstruction->getGeometry();

	bool ok = true;

	if (!m_bAstraSIRTInit) {

		ok &= m_pSirt->setGPUIndex(m_iGPUIndex);

		ok &= m_pSirt->setReconstructionGeometry(volgeom.getGridColCount(),
		                                         volgeom.getGridRowCount(),
		                                         volgeom.getGridSliceCount());

		if (conegeom) {
			ok &= m_pSirt->setConeGeometry(conegeom->getProjectionCount(),
			                               conegeom->getDetectorColCount(),
			                               conegeom->getDetectorRowCount(),
			                               conegeom->getOriginSourceDistance(),
			                               conegeom->getOriginDetectorDistance(),
			                               conegeom->getDetectorSpacingX(),
			                               conegeom->getDetectorSpacingY(),
			                               conegeom->getProjectionAngles());
		} else if (par3dgeom) {
			ok &= m_pSirt->setPar3DGeometry(par3dgeom->getProjectionCount(),
			                                par3dgeom->getDetectorColCount(),
			                                par3dgeom->getDetectorRowCount(),
			                                par3dgeom->getDetectorSpacingX(),
			                                par3dgeom->getDetectorSpacingY(),
			                                par3dgeom->getProjectionAngles());
		} else if (parvec3dgeom) {
			ok &= m_pSirt->setPar3DGeometry(parvec3dgeom->getProjectionCount(),
			                                parvec3dgeom->getDetectorColCount(),
			                                parvec3dgeom->getDetectorRowCount(),
			                                parvec3dgeom->getProjectionVectors());
		} else if (conevec3dgeom) {
			ok &= m_pSirt->setConeGeometry(conevec3dgeom->getProjectionCount(),
			                               conevec3dgeom->getDetectorColCount(),
			                               conevec3dgeom->getDetectorRowCount(),
			                               conevec3dgeom->getProjectionVectors());
		} else {
			ASTRA_ASSERT(false);
		}

		ok &= m_pSirt->enableSuperSampling(m_iVoxelSuperSampling, m_iDetectorSuperSampling);

		if (m_bUseReconstructionMask)
			ok &= m_pSirt->enableVolumeMask();
		if (m_bUseSinogramMask)
			ok &= m_pSirt->enableSinogramMask();

		ASTRA_ASSERT(ok);

		ok &= m_pSirt->init();

		ASTRA_ASSERT(ok);

		m_bAstraSIRTInit = true;

	}

	CFloat32ProjectionData3DMemory* pSinoMem = dynamic_cast<CFloat32ProjectionData3DMemory*>(m_pSinogram);
	ASTRA_ASSERT(pSinoMem);

	ok = m_pSirt->setSinogram(pSinoMem->getDataConst(), m_pSinogram->getGeometry()->getDetectorColCount());

	ASTRA_ASSERT(ok);

	if (m_bUseReconstructionMask) {
		CFloat32VolumeData3DMemory* pRMaskMem = dynamic_cast<CFloat32VolumeData3DMemory*>(m_pReconstructionMask);
		ASTRA_ASSERT(pRMaskMem);
		ok &= m_pSirt->setVolumeMask(pRMaskMem->getDataConst(), volgeom.getGridColCount());
	}
	if (m_bUseSinogramMask) {
		CFloat32ProjectionData3DMemory* pSMaskMem = dynamic_cast<CFloat32ProjectionData3DMemory*>(m_pSinogramMask);
		ASTRA_ASSERT(pSMaskMem);
		ok &= m_pSirt->setSinogramMask(pSMaskMem->getDataConst(), m_pSinogramMask->getGeometry()->getDetectorColCount());
	}

	CFloat32VolumeData3DMemory* pReconMem = dynamic_cast<CFloat32VolumeData3DMemory*>(m_pReconstruction);
	ASTRA_ASSERT(pReconMem);
	ok &= m_pSirt->setStartReconstruction(pReconMem->getDataConst(),
	                                      volgeom.getGridColCount());

	ASTRA_ASSERT(ok);

	if (m_bUseMinConstraint)
		ok &= m_pSirt->setMinConstraint(m_fMinValue);
	if (m_bUseMaxConstraint)
		ok &= m_pSirt->setMaxConstraint(m_fMaxValue);

	ok &= m_pSirt->iterate(_iNrIterations);
	ASTRA_ASSERT(ok);

	ok &= m_pSirt->getReconstruction(pReconMem->getData(),
	                                 volgeom.getGridColCount());
	ASTRA_ASSERT(ok);


}
//----------------------------------------------------------------------------------------
void CCudaSirtAlgorithm3D::signalAbort()
{
	if (m_bIsInitialized && m_pSirt) {
		m_pSirt->signalAbort();
	}
}

bool CCudaSirtAlgorithm3D::getResidualNorm(float32& _fNorm)
{
	if (!m_bIsInitialized || !m_pSirt)
		return false;

	_fNorm = m_pSirt->computeDiffNorm();

	return true;
}


} // namespace astra
