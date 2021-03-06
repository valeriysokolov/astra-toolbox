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

#ifndef _CUDA_FAN_BP_H
#define _CUDA_FAN_BP_H

namespace astraCUDA {

_AstraExport bool FanBP(float* D_volumeData, unsigned int volumePitch,
           float* D_projData, unsigned int projPitch,
           const SDimensions& dims, const SFanProjection* angles);

_AstraExport bool FanBP_SART(float* D_volumeData, unsigned int volumePitch,
                float* D_projData, unsigned int projPitch,
                unsigned int angle,
                const SDimensions& dims, const SFanProjection* angles);

_AstraExport bool FanBP_FBPWeighted(float* D_volumeData, unsigned int volumePitch,
           float* D_projData, unsigned int projPitch,
           const SDimensions& dims, const SFanProjection* angles);


}

#endif
