/*****************************************************************************
 * 
 *  Copyright (c) 2020 by SonicMastr <sonicmastr@gmail.com>
 * 
 *  This file is part of Pigs In A Blanket
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * 
 ****************************************************************************/

#ifndef PIB_H_
#define PIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <psp2/types.h>

/**
 * @brief PIB Error Codes.
 * 
 */
typedef enum _PibError {
    PIB_ERROR_NOT_INIT = -6,    //!< PIB wasn't Initialized
    PIB_ERROR_ALREADY_INIT,     //!< PIB is already Initialized
    PIB_ERROR_FIOS2_FAILED,     //!< SceFios2 Module Failed to Start
    PIB_ERROR_LIBC_FAILED,      //!< SceLibc Module Failed to Start
    PIB_ERROR_PIGLET_FAILED,    //!< libScePiglet Module Failed to Start
    PIB_ERROR_SHACCCG_FAILED,   //!< SceShaccCg Module Failed to Start
    PIB_SUCCESS
} PibError;

/**
 * @brief Initialization options for PIB.
 * 
 */
typedef enum _PibOptions {
    PIB_NONE = 0,               //!< Defaults
    PIB_SHACCCG = 1,            //!< Enable Runtime CG Shader Compiler
    PIB_NOSTDLIB = 2,           //!< Enable support for -nostdlib usage
    PIB_GET_PROC_ADDR_CORE = 4, //!< Enable EGL 1.5-like support for getting core GL functions with eglGetProcAddress
    PIB_SYSTEM_MODE = 8,        //!< Enable use for System Applications
    PIB_ENABLE_MSAA = 16        //!< Enable forced MSAA x4 Support
} PibOptions;

/**
 * @brief Initializes Piglet and optionally SceShaccCg, the Native Runtime Shader Compiler. 
 *  Searches the "ur0:data/external" directory
 * 
 * @param[in] pibOptions 
 *  Intialization options for PIB
 * 
 * @return PibError Code
 * 
 */
PibError pibInit(PibOptions pibOptions);

/**
 * @brief Terminates and unloads Piglet and optionally SceShaccCg if specified by pibInit
 * 
 * @return PibError Code
 * 
 */
PibError pibTerm(void);

#ifdef __cplusplus
}
#endif

#endif /* PIB_H_ */
