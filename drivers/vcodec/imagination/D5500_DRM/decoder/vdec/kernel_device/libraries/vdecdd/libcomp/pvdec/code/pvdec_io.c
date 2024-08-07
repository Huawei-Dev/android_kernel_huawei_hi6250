﻿/*!
 *****************************************************************************
 *
 * @File       pvdec_io.c
 * @Title      Low-level PVDEC interface component
 * @Description    This file contains functions to communicate with an PVDEC core.
 * ---------------------------------------------------------------------------
 *
 * Copyright (c) Imagination Technologies Ltd.
 * 
 * The contents of this file are subject to the MIT license as set out below.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 * 
 * Alternatively, the contents of this file may be used under the terms of the 
 * GNU General Public License Version 2 ("GPL")in which case the provisions of
 * GPL are applicable instead of those above. 
 * 
 * If you wish to allow use of your version of this file only under the terms 
 * of GPL, and not to allow others to use your version of this file under the 
 * terms of the MIT license, indicate your decision by deleting the provisions 
 * above and replace them with the notice and other provisions required by GPL 
 * as set out in the file called "GPLHEADER" included in this distribution. If 
 * you do not delete the provisions above, a recipient may use your version of 
 * this file under the terms of either the MIT license or GPL.
 * 
 * This License is also included in this distribution in the file called 
 * "MIT_COPYING".
 *
 *****************************************************************************/

#include "img_include.h"
#include "tal.h"

#include "report_api.h"
#include "dma_ll.h"

#include "devaio.h"
#include "pvdec_io.h"

#if defined(SYSBRG_BRIDGING)
#include "vxd_pvdec.h"
#include "secure_device.h"
#else
#include "target.h"
#endif

#include "vxd.h"
#include "vxd_buf.h"

#include "report_api.h"
#include "img_defs.h"
#include "reg_io2.h"
#include "vdecfw.h"

#include "hwdefs/pvdec_core_regs.h"
#include "hwdefs/img_video_bus4_mmu_regs.h"
#include "hwdefs/pvdec_pixel_regs.h"
#include "hwdefs/pvdec_entropy_regs.h"
#include "hwdefs/pvdec_vec_be_regs.h"
#include "hwdefs/msvdx_mtx_regs.h"
#include "hwdefs/msvdx_vec_regs.h"
#include "hwdefs/msvdx_vdmc_regs.h"

#if defined(SYSBRG_BRIDGING)
#include <linux/delay.h>
#endif

#define PVDEC_DEF_REG_MASK -1
#define PVDEC_DEF_PIPE 0xFF

#define PVDEC_PIXEL_WritePipeRegister(h, p, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_PIXEL_PIPE_REGSPACE, o, v, PVDEC_DEF_REG_MASK, p)

#define PVDEC_PIXEL_ReadPipeRegister(h, p, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_PIXEL_PIPE_REGSPACE, o, p)

#define PVDEC_ENTROPY_WritePipeRegister(h, p, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_ENTROPY_PIPE_REGSPACE, o, v, PVDEC_DEF_REG_MASK, p)

#define PVDEC_ENTROPY_ReadPipeRegister(h, p, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_ENTROPY_PIPE_REGSPACE, o, p)

#define PVDEC_VEC_BE_WritePipeRegister(h, p, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_VEC_BE, o, v, PVDEC_DEF_REG_MASK, p)

#define PVDEC_VEC_BE_ReadPipeRegister(h, p, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_VEC_BE, o, p)

#define PVDEC_ENTROPY_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_ENTROPY_PIPE_REGSPACE, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define PVDEC_ENTROPY_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_ENTROPY_PIPE_REGSPACE, o, PVDEC_DEF_PIPE)

#define PVDEC_CORE_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_CORE_REGSPACE, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define PVDEC_CORE_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_CORE_REGSPACE, o, PVDEC_DEF_PIPE)

#define PVDEC_VEC_BE_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_VEC_BE, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define PVDEC_VEC_BE_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_VEC_BE, o, PVDEC_DEF_PIPE)

#define IMG_VIDEO_BUS4_MMU_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_IMG_VIDEO_BUS4_MMU_REGSPACE, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define IMG_VIDEO_BUS4_MMU_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_IMG_VIDEO_BUS4_MMU_REGSPACE, o, PVDEC_DEF_PIPE)

#define PVDEC_VLR_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_VLR_REGSPACE, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define PVDEC_VLR_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_VLR_REGSPACE, o, PVDEC_DEF_PIPE)

#define MTX_CORE_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_MTX_CORE_REGSPACE, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define MTX_CORE_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_MTX_CORE_REGSPACE, o, PVDEC_DEF_PIPE)

#define MSVDX_VEC_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_MSVDX_VEC, o, PVDEC_DEF_PIPE)

#define MSVDX_VEC_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_MSVDX_VEC, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define MSVDX_VEC_ReadPipeRegister(h, p, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_MSVDX_VEC, o, p)

#define MSVDX_VEC_WritePipeRegister(h, p, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_MSVDX_VEC, o, v, PVDEC_DEF_REG_MASK, p)

#define MSVDX_VDMC_ReadRegister(h, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_MSVDX_VDMC, o, PVDEC_DEF_PIPE)

#define MSVDX_VDMC_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_MSVDX_VDMC, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define MSVDX_VDMC_ReadPipeRegister(h, p, o) \
        PVDECIO_ReadRegister(h, REGION_PVDEC_MSVDX_VDMC, o, p)

#define MSVDX_VDMC_WritePipeRegister(h, p, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_MSVDX_VDMC, o, v, PVDEC_DEF_REG_MASK, p)

#define MSVDX_VDMC_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_MSVDX_VDMC, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define PVDEC_TEST_WriteRegister(h, o, v) \
        PVDECIO_WriteRegister(h, REGION_PVDEC_TEST_REGSPACE, o, v, PVDEC_DEF_REG_MASK, PVDEC_DEF_PIPE)

#define PVDEC_PIPE_OFFSET 0x10000

#if defined(CORE_CLOCK_FREQ_MHZ)
/* Using value from platform info */
#define PVDECIO_MTX_CLK_MHZ CORE_CLOCK_FREQ_MHZ
#else
/* Default */
#define PVDECIO_MTX_CLK_MHZ 200
#endif

#define PVDEC_TIMEOUT_COUNTER  (1000) /*!< Time out counter */

#define PIXEL_DMA_WriteRegMacro(_pMemSpceCtx, _pipe, _channel, _reg, _val) \
    ( IMG_ASSERT(_channel < DMAC_##_reg##_NO_ENTRIES), \
    PVDECIO_WriteRegister(_pMemSpceCtx, REGION_PVDEC_PIXEL_DMA_REGSPACE, \
                        (DMAC_##_reg##_OFFSET+(_channel*DMAC_##_reg##_STRIDE)), \
                        (IMG_UINT32) (_val), PVDEC_DEF_REG_MASK, _pipe))

#define PIXEL_DMA_ReadRegMacro(_pMemSpceCtx, _pipe, _channel, _reg) \
    ( IMG_ASSERT(_channel < DMAC_##_reg##_NO_ENTRIES), \
    PVDECIO_ReadRegister(_pMemSpceCtx, \
                       REGION_PVDEC_PIXEL_DMA_REGSPACE, \
                       (DMAC_##_reg##_OFFSET+(_channel*DMAC_##_reg##_STRIDE)), \
                       _pipe))

/* Number of words needed to enable MTX: two words for setting PC + one word for MTX enable */
#define PVDECIO_PC_CONF_WORDS       2
#define PVDECIO_ENABLE_MTX_WORDS    1
#define PVDECIO_MTX_EN_TOTAL_WORDS  (PVDECIO_ENABLE_MTX_WORDS + PVDECIO_PC_CONF_WORDS)

#ifdef VDEC_USE_PVDEC_SEC
#define CHECK_IO_BYPASS(b) if (psContext->bIOBypass == b)
#else
#define CHECK_IO_BYPASS(b)
#endif

/*!
******************************************************************************
 This structure contains MTX RAM information.
 @brief  MTX RAM Information
******************************************************************************/
typedef struct
{
    IMG_UINT32  ui32MTXBankSize;
    IMG_UINT32  ui32MTXRamSize;
    IMG_UINT32  ui32MTXRamMask;

} PVDECIO_sMTXRamInfo;

/*!
******************************************************************************
 This structure contains firmware Blob information.
 @brief  MTX RAM Information
******************************************************************************/
typedef struct
{
    IMG_UINT32  ui32BlobSize;
    IMG_UINT32  ui32CoreSize;
    IMG_UINT32  ui32BlobDevVirtAddr;
#ifdef PVDEC_REG_FW_UPLOAD
    IMG_VOID *  pvBlobCpuAddr;
#endif /* PVDEC_REG_FW_UPLOAD */

} PVDECIO_sFwInfo;


typedef struct
{
    IMG_BOOL                bInitialised;
    IMG_BOOL                bClocks;                    /*!< Clocks are on/off for the core.        */

    PVDECIO_sMTXRamInfo     sMTXRamInfo;                /*!< MTX RAM information                    */
    PVDECIO_sFwInfo         sFwInfo;

    DEVAIO_sContext         sMsgContext;                /*!<                                        */
    DEVAIO_sBufConfig       sStateInfo;                 /*!< Core state buffer info.                */

    IMG_UINT32              ui32NumPixelPipes;          /*!< Core state buffer info.                */

    IMG_HANDLE            * pahMemSpace;                /*!< Array of mem spaces handles            */
    IMG_UINT32              ui32MemSpaceCount;          /*!< Count of mem spaces in pahMemSpace     */

    IMG_BOOL                bSecureFw;                  /*!< Secure fw in use when true             */
    IMG_BOOL                bIOBypass;                  /*!< Indicates if upper layer owns
                                                             restricted registers access */


} PVDECIO_sContext;


static IMG_VOID pvdecio_SelectPipe(
    PVDECIO_sContext  * psContext,
    IMG_UINT8           ui8Pipe
)
{
    IMG_ASSERT(psContext);
    if (ui8Pipe != PVDEC_DEF_PIPE)
    {
        IMG_UINT32          ui32Reg = 0;
        IMG_UINT32          ui32PipeSelectOffset = PVDEC_CORE_CR_PVDEC_HOST_PIPE_SELECT_OFFSET;

        IMG_ASSERT(ui8Pipe < 8);
        REGIO_WRITE_FIELD(ui32Reg, PVDEC_CORE, CR_PVDEC_HOST_PIPE_SELECT, CR_PVDEC_HOST_PIPE_SELECT, ui8Pipe);
        TALREG_WriteWord32(psContext->pahMemSpace[REGION_PVDEC_CORE_REGSPACE], ui32PipeSelectOffset, ui32Reg);
    }
}

/*!
******************************************************************************

 @Function              PVDECIO_ReadRegister

******************************************************************************/
IMG_UINT32
PVDECIO_ReadRegister(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT8           ui8Pipe
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32          ui32Value = 0;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An video decoder device context handle must be provided (%d)", __LINE__);
        goto error;
    }

    if(!psContext->bInitialised)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "PVDEC IO must be initialised before reading register (%d)", __LINE__);
        goto error;
    }

    //after correct initialisation this value must be set
    IMG_ASSERT(psContext->ui32MemSpaceCount != 0);

    if (psContext->bClocks)
    {
        //select pipe after checking if it's relevant
        pvdecio_SelectPipe(psContext, ui8Pipe);

        TALREG_ReadWord32(psContext->pahMemSpace[ui32MemRegion], ui32Offset, &ui32Value);
    }
    else
    {
        REPORT(REPORT_MODULE_VXDIO,
               REPORT_NOTICE,
               "Didn't READ register while clocks are turned off");
    }

    return ui32Value;

error:
    return 0;
}


/*!
******************************************************************************

 @Function              PVDECIO_WriteRegister

******************************************************************************/
IMG_RESULT
PVDECIO_WriteRegister(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32Value,
    IMG_UINT32          ui32Mask,
    IMG_UINT8           ui8Pipe
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32          ui32Result;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An video decoder device context handle must be provided (%d)", __LINE__);
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }

    if(!psContext->bInitialised)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "PVDEC IO must be initialised before reading register (%d)", __LINE__);
        ui32Result =  IMG_ERROR_NOT_INITIALISED;
        goto error;
    }

    //after correct initialisation this value must be set
    IMG_ASSERT(psContext->ui32MemSpaceCount != 0);

    if (psContext->bClocks)
    {
        IMG_UINT32 ui32RegValue;

        pvdecio_SelectPipe(psContext, ui8Pipe);

        if (ui32Mask != -1)
        {
            TALREG_ReadWord32(psContext->pahMemSpace[ui32MemRegion], ui32Offset, &ui32RegValue);
            ui32RegValue &= ~ui32Mask;
            ui32RegValue |= ui32Value;
        }
        else
        {
            ui32RegValue = ui32Value;
        }

        TALREG_WriteWord32(psContext->pahMemSpace[ui32MemRegion], ui32Offset, ui32RegValue);
    }
    else
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_NOTICE,
            "Didn't WRITE register while clocks are turned off");
        ui32Result = IMG_ERROR_NOT_INITIALISED;
        goto error;
    }

    return IMG_SUCCESS;

error:
    return ui32Result;
}



/*!
******************************************************************************

 @Function              PVDECIO_VLRWriteWords

******************************************************************************/
IMG_RESULT
PVDECIO_VLRWriteWords(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Addr,
    IMG_UINT32          ui32NumWords,
    const IMG_UINT32  * pui32Values
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32          ui32WordCnt;
    IMG_UINT32          ui32Result;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An video decoder device context handle must be provided (%d)", __LINE__);
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }

#if 0
    if (ui32MemRegion != REGION_VLR_REGSPACE &&
        ui32MemRegion != REGION_VLRFE_REGSPACE &&
        ui32MemRegion != REGION_VLRBE_REGSPACE)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid VLR memory space must be provided");
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }
#endif

    if ((ui32Addr & 0x3) != 0 ||
        ui32NumWords == 0 ||
        pui32Values == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid VLR location (offset word aligned) must be provided");
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }

    // Repeat for all words.
    for (ui32WordCnt = 0; ui32WordCnt < ui32NumWords; ui32WordCnt++)
    {
#ifdef SECURE_CPU_REG_ACCESS
        {
            IMG_ASSERT(psVxdCtx->sCoreFuncs.pfnSecRegAccess != IMG_NULL);
            psVxdCtx->sCoreFuncs.pfnSecRegAccess(psVxdCtx->hCoreCtx);
        }
#endif
        ui32Result = TALREG_WriteWord32(psContext->pahMemSpace[ui32MemRegion],
                                        ui32Addr,
                                        *pui32Values);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }

        pui32Values++;
        ui32Addr += 4;
    }

    return IMG_SUCCESS;

error:
    return ui32Result;
}


/*!
******************************************************************************

 @Function              PVDECIO_VLRReadWords

******************************************************************************/
IMG_RESULT
PVDECIO_VLRReadWords(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Addr,
    IMG_UINT32          ui32NumWords,
    IMG_UINT32        * pui32Values,
    IMG_BOOL            bValidate
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32          ui32WordCnt = 0;
    IMG_UINT32          ui32Result = IMG_SUCCESS;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An video decoder device context handle must be provided (%d)", __LINE__);
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }

#if 0
    if (ui32MemRegion != REGION_VLR_REGSPACE &&
        ui32MemRegion != REGION_VLRFE_REGSPACE &&
        ui32MemRegion != REGION_VLRBE_REGSPACE)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid VLR memory space must be provided");
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }
#endif

    if ((ui32Addr & 0x3) != 0 ||
        ui32NumWords == 0 ||
        pui32Values == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid VLR location (offset word aligned) must be provided");
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }

    // Repeat for all words.
    for (ui32WordCnt = 0; ui32WordCnt < ui32NumWords; ui32WordCnt++)
    {
#ifdef SECURE_CPU_REG_ACCESS
        {
            IMG_ASSERT(psVxdCtx->sCoreFuncs.pfnSecRegAccess != IMG_NULL);
            psVxdCtx->sCoreFuncs.pfnSecRegAccess(psVxdCtx->hCoreCtx);
        }
#endif

        ui32Result = TALREG_ReadWord32(psContext->pahMemSpace[ui32MemRegion],
                                       ui32Addr,
                                       pui32Values);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }

#if !defined(SYSBRG_BRIDGING)
        if (bValidate)
        {
            IMG_UINT32 ui32Mask = 0xFFFFFFFF;
            ui32Result = PVDECIO_PDUMPVerifPoll(psContext,
                                              ui32MemRegion,
                                              ui32Addr,
                                              *pui32Values,
                                              ui32Mask);
            if (ui32Result != IMG_SUCCESS)
            {
                goto error;
            }
        }
#endif

        pui32Values++;
        ui32Addr += 4;
    }

error:

    return ui32Result;
}


/*
******************************************************************************

 @Function              pvdecio_SetClocks

******************************************************************************/
static IMG_VOID pvdecio_SetClocks(
    IMG_HANDLE  hCoreContext,
    IMG_UINT32  ui32ReqClocks
)
{
    PVDECIO_sContext * psContext = (PVDECIO_sContext * )hCoreContext;
    CHECK_IO_BYPASS(IMG_FALSE)
    {
        /* Turn off all the clocks except core */
        IMG_UINT32 ui32RegCoreClock = 0, ui32RegValue;

        REGIO_WRITE_FIELD(ui32RegCoreClock, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                          CR_PVDEC_REG_MAN_CLK_ENABLE, 1);
        REGIO_WRITE_FIELD(ui32RegCoreClock, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                          CR_CORE_MAN_CLK_ENABLE, 1);

        REGIO_WRITE_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                             ui32RegCoreClock);

        ui32RegValue = REGIO_READ_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE);

        /* Make sure all the clocks are off except core */
        IMG_ASSERT(ui32RegValue == ui32RegCoreClock);

        /* Write requested clocks value */
        REGIO_WRITE_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                             ui32ReqClocks);
    }
}

/*!
******************************************************************************

 @Function              PVDECIO_Poll

******************************************************************************/
IMG_RESULT PVDECIO_Poll(
    const IMG_HANDLE  hCoreContext,
    IMG_UINT32        eMemRegion,
    IMG_UINT32        ui32Offset,
    IMG_UINT32        ui32RequValue,
    IMG_UINT32        ui32Enable,
    VXDIO_ePollMode   ePollMode
)
{
    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;
    IMG_RESULT          ui32Result = IMG_SUCCESS;
    IMG_INT32           ui32PollCount = 100000;
    IMG_INT32           ui32TimeOut = 100;
    IMG_UINT32          ui32PollMode;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    if (eMemRegion >= REGION_PVDEC_MAX)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid PVDEC memory space must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    switch(ePollMode)
    {
    case VXDIO_POLL_EQUAL:
        ui32PollMode = TAL_CHECKFUNC_ISEQUAL;
        break;
#if !defined(SYSBRG_BRIDGING)
    case VXDIO_POLL_NOT_EQUAL:
        ui32PollMode = TAL_CHECKFUNC_NOTEQUAL;
        break;
#endif
    default:
        IMG_ASSERT("Invalid poll mode" == 0);
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR, "Invalid poll mode");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    /* Call on to the TAL...*/
    ui32Result = TALREG_Poll32(psContext->pahMemSpace[eMemRegion],
                               ui32Offset,
                               ui32PollMode,
                               ui32RequValue,
                               ui32Enable,
                               ui32PollCount,
                               ui32TimeOut);

    IMG_ASSERT((IMG_SUCCESS == ui32Result) || (IMG_ERROR_TIMEOUT == ui32Result));
    return ui32Result;
}

/*!
******************************************************************************

 @Function              PVDECIO_GetCoreState

******************************************************************************/
IMG_RESULT PVDECIO_GetCoreState(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32NumPixelPipes,
    IMG_UINT32          ui32NumEntPipes,
    VXDIO_sState      * psState
)
{
    PVDECIO_sContext  *         psContext = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT8                   ui8Pipe = 0;
    VDECFW_sPVDECFirmwareState  sFirmwareState = {{0}};

    if (hCoreContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    if (psContext->bClocks)
    {
        IMG_UINT32 ui32RegVal;

        /* If core state is requested for the first time... */
        if(0 == psContext->sStateInfo.ui32Size)
        {
            /* ...get the state buffer info. */
            PVDECIO_VLRReadWords(psContext,
                               REGION_PVDEC_VLR_REGSPACE,
                               PVDEC_COM_RAM_STATE_BUF_SIZE_AND_OFFSET_OFFSET,
                               1,
                               &ui32RegVal,
                               IMG_FALSE);
            psContext->sStateInfo.ui32Size   = PVDEC_COM_RAM_BUF_GET_SIZE(ui32RegVal, STATE);
            psContext->sStateInfo.ui32Offset = PVDEC_COM_RAM_BUF_GET_OFFSET(ui32RegVal, STATE);
        }

        /* If state buffer is available... */
        if(psContext->sStateInfo.ui32Size > 0)
        {
            // Determine the latest transaction to have passed each checkpoint in the firmware.
            // Read the firmware state from VEC Local RAM
            PVDECIO_VLRReadWords(psContext,
                               REGION_PVDEC_VLR_REGSPACE,
                               psContext->sStateInfo.ui32Offset,
                               psContext->sStateInfo.ui32Size >> 2,
                               (IMG_UINT32 *)&sFirmwareState,
                               IMG_FALSE);

            // Update firmware state fields.
            IMG_MEMCPY(psState->sFwState.aui8ControlFenceID,
                       sFirmwareState.aui8ControlFenceID,
                       sizeof(psState->sFwState.aui8ControlFenceID));
            IMG_MEMCPY(psState->sFwState.aui8DecodeFenceID,
                       sFirmwareState.aui8DecodeFenceID,
                       sizeof(psState->sFwState.aui8DecodeFenceID));
            IMG_MEMCPY(psState->sFwState.aui8CompletionFenceID,
                       sFirmwareState.aui8CompletionFenceID,
                       sizeof(psState->sFwState.aui8CompletionFenceID));

            for(ui8Pipe = 0; ui8Pipe < ui32NumPixelPipes; ui8Pipe++)
            {
                /* Set pipe presence. */
                psState->sFwState.asPipeState[ui8Pipe].bPipePresent = IMG_TRUE;

                /* For checkpoints copy message ids here. These will be translated into transaction ids later. */
                IMG_MEMCPY(psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint,
                           sFirmwareState.asPipeState[ui8Pipe].aui32CheckPoint,
                           sizeof(psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint));
                psState->sFwState.asPipeState[ui8Pipe].eFirmwareAction      = sFirmwareState.asPipeState[ui8Pipe].eFirmwareAction;
                psState->sFwState.asPipeState[ui8Pipe].ui32FenceValue       = sFirmwareState.asPipeState[ui8Pipe].ui32FenceValue;
                psState->sFwState.asPipeState[ui8Pipe].ui8CurCodec          = sFirmwareState.asPipeState[ui8Pipe].ui8CurCodec;
                psState->sFwState.asPipeState[ui8Pipe].ui32FeSlices         = sFirmwareState.asPipeState[ui8Pipe].ui32FESlices;
                psState->sFwState.asPipeState[ui8Pipe].ui32BeSlices         = sFirmwareState.asPipeState[ui8Pipe].ui32BESlices;
                psState->sFwState.asPipeState[ui8Pipe].ui32FeErroredSlices  = sFirmwareState.asPipeState[ui8Pipe].ui32FEErroredSlices;
                psState->sFwState.asPipeState[ui8Pipe].ui32BeErroredSlices  = sFirmwareState.asPipeState[ui8Pipe].ui32BEErroredSlices;
                psState->sFwState.asPipeState[ui8Pipe].ui32BeMbsDropped     = sFirmwareState.asPipeState[ui8Pipe].ui32BEMbsDropped;
                psState->sFwState.asPipeState[ui8Pipe].ui32BeMbsRecovered   = sFirmwareState.asPipeState[ui8Pipe].ui32BEMbsRecovered;
#ifdef VDEC_USE_PVDEC_SEC
                psState->sFwState.asPipeState[ui8Pipe].eFwBspStep      = sFirmwareState.asPipeState[ui8Pipe].eFwBspStep;
#endif

                CHECK_IO_BYPASS(IMG_FALSE)
                {
                    //Reading channels 2/3 (DMA_CHANNEL_SR1/2)
                    ui32RegVal = PIXEL_DMA_ReadRegMacro(psContext, ui8Pipe, 2, DMAC_COUNT);
                    psState->sFwState.asPipeState[ui8Pipe].aui32DMACStatus[0] = REGIO_READ_FIELD(ui32RegVal, DMAC, DMAC_COUNT, CNT);
                    ui32RegVal = PIXEL_DMA_ReadRegMacro(psContext, ui8Pipe, 3, DMAC_COUNT);
                    psState->sFwState.asPipeState[ui8Pipe].aui32DMACStatus[1] = REGIO_READ_FIELD(ui32RegVal, DMAC, DMAC_COUNT, CNT);

                    if(VDEC_CODEC_NONE != sFirmwareState.asPipeState[ui8Pipe].ui8CurCodec)
                    {
                        // FE or PE and BE current MB number.
                        if(VDECFW_CODEC_HEVC == sFirmwareState.asPipeState[ui8Pipe].ui8CurCodec)
                        {
                            if(ui8Pipe < ui32NumEntPipes)
                            {
                                // Obtain the last processed MB on Entropy Pipe HW expressed as an X,Y offset into the frame.
                                ui32RegVal = REGIO_READ_PIPE_REGISTER(psContext, ui8Pipe+1, PVDEC_ENTROPY, CR_ENTROPY_LAST_MB);
                                psState->sFwState.asPipeState[ui8Pipe].sFeMb.ui32X = REGIO_READ_FIELD(ui32RegVal, PVDEC_ENTROPY, CR_ENTROPY_LAST_MB, ENTROPY_LAST_MB_NO_X);
                                psState->sFwState.asPipeState[ui8Pipe].sFeMb.ui32Y = REGIO_READ_FIELD(ui32RegVal, PVDEC_ENTROPY, CR_ENTROPY_LAST_MB, ENTROPY_LAST_MB_NO_Y);
                            }
                            // Obtain the last processed MB on PVDEC back-end HW expressed as an X,Y offset into the frame.
                            ui32RegVal = REGIO_READ_PIPE_REGISTER(psContext, ui8Pipe+1, PVDEC_VEC_BE, CR_VEC_BE_STATUS);
                            psState->sFwState.asPipeState[ui8Pipe].sBeMb.ui32X = REGIO_READ_FIELD(ui32RegVal, PVDEC_VEC_BE, CR_VEC_BE_STATUS, COMMAND_MB_ADDR_X);
                            psState->sFwState.asPipeState[ui8Pipe].sBeMb.ui32Y = REGIO_READ_FIELD(ui32RegVal, PVDEC_VEC_BE, CR_VEC_BE_STATUS, COMMAND_MB_ADDR_Y);
                        }
                        else if(sFirmwareState.asPipeState[ui8Pipe].ui8CurCodec < VDECFW_CODEC_MAX)
                        {
                            // Obtain the last processed MB on Pixel Pipe front-end HW expressed as an X,Y offset into the frame.
                            ui32RegVal = REGIO_READ_PIPE_REGISTER(psContext, ui8Pipe+1, MSVDX_VEC, CR_VEC_ENTDEC_INFORMATION);
                            psState->sFwState.asPipeState[ui8Pipe].sFeMb.ui32X = REGIO_READ_FIELD(ui32RegVal, MSVDX_VEC, CR_VEC_ENTDEC_INFORMATION, FE_ENTDEC_LATEST_MB_ADDR_X);
                            psState->sFwState.asPipeState[ui8Pipe].sFeMb.ui32Y = REGIO_READ_FIELD(ui32RegVal, MSVDX_VEC, CR_VEC_ENTDEC_INFORMATION, FE_ENTDEC_LATEST_MB_ADDR_Y);
                            // Obtain the last processed MB on back-end HW expressed as an X,Y offset into the frame.
                            ui32RegVal = REGIO_READ_PIPE_REGISTER(psContext, ui8Pipe+1, MSVDX_VDMC, CR_VDMC_MACROBLOCK_NUMBER);
                            psState->sFwState.asPipeState[ui8Pipe].sBeMb.ui32X = REGIO_READ_FIELD(ui32RegVal, MSVDX_VDMC, CR_VDMC_MACROBLOCK_NUMBER, CR_VDMC_MACROBLOCK_X_OFFSET);
                            psState->sFwState.asPipeState[ui8Pipe].sBeMb.ui32Y = REGIO_READ_FIELD(ui32RegVal, MSVDX_VDMC, CR_VDMC_MACROBLOCK_NUMBER, CR_VDMC_MACROBLOCK_Y_OFFSET);
                        }
                    }
                }
#if 0
                ML_PRINT(">>>>> State Report >>>>>>>>>>>>\n");
                ML_PRINT("  DMACStatus[ch2] : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32DMACStatus[0]);
                ML_PRINT("  DMACStatus[ch3] : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32DMACStatus[1]);
                ML_PRINT("  FeMB            : %d,%d\n" , psState->sFwState.asPipeState[ui8Pipe].sFeMb.ui32X, psState->sFwState.asPipeState[ui8Pipe].sFeMb.ui32Y);
                ML_PRINT("  BeMB            : %d,%d\n" , psState->sFwState.asPipeState[ui8Pipe].sBeMb.ui32X, psState->sFwState.asPipeState[ui8Pipe].sBeMb.ui32Y);
                ML_PRINT("  FESlices        : %d\n"    , psState->sFwState.asPipeState[ui8Pipe].ui32FeSlices);
                ML_PRINT("  BESlices        : %d\n"    , psState->sFwState.asPipeState[ui8Pipe].ui32BeSlices);
                ML_PRINT("--------------------------------\n");
                ML_PRINT("  CP_PICTURE_STARTED     : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 1]);
                ML_PRINT("  CP_FIRMWARE_READY      : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 2]);
                ML_PRINT("  CP_SYNC_COMPLETE       : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 3]);
                ML_PRINT("  CP_PICMAN_COMPLETE     : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 4]);
                ML_PRINT("  CP_FIRMWARE_SAVED      : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 5]);
                ML_PRINT("  CP_ENTDEC_STARTED      : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 6]);
                ML_PRINT("  CP_FE_1SLICE_DONE      : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 7]);
                ML_PRINT("  CP_FE_PARSE_DONE       : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 8]);
                ML_PRINT("  CP_FE_PICTURE_COMPLETE : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[ 9]);
                ML_PRINT("  CP_BE_PICTURE_STARTED  : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[10]);
                ML_PRINT("  CP_BE_1SLICE_DONE      : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[11]);
                ML_PRINT("  CP_BE_PICTURE_COMPLETE : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[12]);
                ML_PRINT("  CP_FI_INIT_START       : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[13]);
                ML_PRINT("  CP_FI_INIT_END         : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[14]);
                ML_PRINT("  CP_FI_POST_MSG         : 0x%08x\n", psState->sFwState.asPipeState[ui8Pipe].aui32CheckPoint[15]);
                ML_PRINT("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
#endif
            }
        }
        // Get runtime information from MSVDX.
        // MTX status.
        CHECK_IO_BYPASS(IMG_FALSE)
        {
            ui32RegVal = 0;
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RNW, 1);         // Read
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_USPECIFIER, 5);  // PC or PCX
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RSPECIFIER, 0);  // PC
            REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, ui32RegVal);
            psState->sEPRuntimeStatus.ui32MTX_PC = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_DATA);

            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RNW, 1);         // Read
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_USPECIFIER, 5);  // PC or PCX
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RSPECIFIER, 1);  // PCX
            REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, ui32RegVal);
            psState->sEPRuntimeStatus.ui32MTX_PCX = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_DATA);

            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RNW, 1);         // Read
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_USPECIFIER, 3);  // A0StP
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RSPECIFIER, 0);
            REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, ui32RegVal);
            psState->sEPRuntimeStatus.ui32MTX_A0StP = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_DATA);

            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RNW, 1);         // Read
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_USPECIFIER, 3);  // A0FrP
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RSPECIFIER, 1);
            REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_REQUEST, ui32RegVal);
            psState->sEPRuntimeStatus.ui32MTX_A0FrP = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_REGISTER_READ_WRITE_DATA);

            psState->sEPRuntimeStatus.ui32MTX_ENABLE = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_ENABLE);
            psState->sEPRuntimeStatus.ui32MTX_STATUS_BITS = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_STATUS);
            psState->sEPRuntimeStatus.ui32MTX_FAULT0 = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_FAULT0);
        }
#if 0
#ifndef SYSBRG_BRIDGING
        printf("  MTX_PC              : 0x%08x\n", psState->sEPRuntimeStatus.ui32MTX_PC);
        printf("  MTX_PCX             : 0x%08x\n", psState->sEPRuntimeStatus.ui32MTX_PCX);
        printf("  MTX_A0StP           : 0x%08x\n", psState->sEPRuntimeStatus.ui32MTX_A0StP);
        printf("  MTX_A0FrP           : 0x%08x\n", psState->sEPRuntimeStatus.ui32MTX_A0FrP);
        printf("  MTX_ENABLE          : 0x%08x\n", psState->sEPRuntimeStatus.ui32MTX_ENABLE);
        printf("  MTX_MTX_STATUS_BITS : 0x%08x\n", psState->sEPRuntimeStatus.ui32MTX_STATUS_BITS);
        printf("  MTX_FAULT0          : 0x%08x\n", psState->sEPRuntimeStatus.ui32MTX_FAULT0);
        printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
#endif
#endif

#if 0
        psState->sEPRuntimeStatus.ui32DMA_COUNT[0] = REGIO_READ_TABLE_REGISTER(psContext,  DMAC, DMAC_COUNT, 0);
        psState->sEPRuntimeStatus.ui32DMA_COUNT[1] = REGIO_READ_TABLE_REGISTER(psContext,  DMAC, DMAC_COUNT, 1);
        psState->sEPRuntimeStatus.ui32DMA_COUNT[2] = REGIO_READ_TABLE_REGISTER(psContext,  DMAC, DMAC_COUNT, 2);

        psState->sEPRuntimeStatus.ui32DMA_SETUP[0] = REGIO_READ_TABLE_REGISTER(psContext,  DMAC, DMAC_SETUP, 0);
        psState->sEPRuntimeStatus.ui32DMA_SETUP[1] = REGIO_READ_TABLE_REGISTER(psContext,  DMAC, DMAC_SETUP, 1);
        psState->sEPRuntimeStatus.ui32DMA_SETUP[2] = REGIO_READ_TABLE_REGISTER(psContext,  DMAC, DMAC_SETUP, 2);

        psState->sEPRuntimeStatus.ui32DMA_PERIPHERAL_ADDR[0] =
            REGIO_READ_TABLE_REGISTER(psContext, DMAC, DMAC_PERIPHERAL_ADDR, 0);
        psState->sEPRuntimeStatus.ui32DMA_PERIPHERAL_ADDR[1] =
            REGIO_READ_TABLE_REGISTER(psContext, DMAC, DMAC_PERIPHERAL_ADDR, 1);
        psState->sEPRuntimeStatus.ui32DMA_PERIPHERAL_ADDR[2] =
            REGIO_READ_TABLE_REGISTER(psContext, DMAC, DMAC_PERIPHERAL_ADDR, 2);
#endif
    }

    return IMG_SUCCESS;

}

/*
******************************************************************************

 @Function              PVDECIO_EnableCoreClocks

******************************************************************************/
IMG_VOID PVDECIO_EnableCoreClocks(
    IMG_HANDLE  hCoreContext
)
{
    PVDECIO_sContext * psContext = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32 ui32Clocks = 0;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return;
    }

    psContext->bClocks = IMG_TRUE;

    REGIO_WRITE_FIELD(ui32Clocks, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PVDEC_REG_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Clocks, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_CORE_MAN_CLK_ENABLE, 1);

    pvdecio_SetClocks(psContext, ui32Clocks);
}

/*
******************************************************************************

 @Function              PVDECIO_DisableClocks

******************************************************************************/
IMG_RESULT PVDECIO_DisableClocks(
    const IMG_HANDLE  hCoreContext
)
{
    PVDECIO_sContext * psContext = (PVDECIO_sContext * )hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    pvdecio_SetClocks(psContext, 0);

    /* Mark clocks as disabled. */
    psContext->bClocks = IMG_FALSE;

    /* Also assume the firmware will be reloaded. */
    psContext->sMsgContext.bReady = IMG_FALSE;

#if defined(IMG_DEBUG_FLAG) || defined(FW_PERFORMANCE_LOGGING)
    PVDECIO_DisableDebugFifo(hCoreContext);
#endif

    return IMG_SUCCESS;
}

/*
******************************************************************************

 @Function              PVDECIO_EnableCoreAndProcClocks

******************************************************************************/
IMG_RESULT PVDECIO_EnableCoreAndProcClocks(
    const IMG_HANDLE  hCoreContext
)
{
    PVDECIO_sContext * psContext = (PVDECIO_sContext * )hCoreContext;
    IMG_UINT32 ui32Clocks = 0;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    psContext->bClocks = IMG_TRUE;

    REGIO_WRITE_FIELD(ui32Clocks, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PVDEC_REG_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Clocks, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_CORE_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Clocks, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_MEM_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Clocks, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PROC_MAN_CLK_ENABLE, 1);

    pvdecio_SetClocks(psContext, ui32Clocks);

    return IMG_SUCCESS;
}

/*
******************************************************************************

 @Function              pvdecio_MTXGetRamInfo

******************************************************************************/
IMG_RESULT pvdecio_MTXGetRamInfo(
    PVDECIO_sContext  * psContext
)
{
    IMG_UINT32 ui32RamInfo, ui32BankCount, ui32RAMBankSize, ui32LastRAMBankSize;
    PVDECIO_sMTXRamInfo * psMTXRamInfo;

    IMG_ASSERT(psContext);

    psMTXRamInfo = &psContext->sMTXRamInfo;

    ui32RamInfo =
        REGIO_READ_REGISTER(psContext, PVDEC_CORE, CR_PROC_DEBUG);

    ui32BankCount = REGIO_READ_FIELD(ui32RamInfo, PVDEC_CORE, CR_PROC_DEBUG, CR_MTX_RAM_BANKS);
    if (ui32BankCount == 0)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR, "Failed to get number of RAM banks.");
        return IMG_ERROR_FATAL;
    }
    ui32RAMBankSize =
        REGIO_READ_FIELD(ui32RamInfo, PVDEC_CORE, CR_PROC_DEBUG, CR_MTX_RAM_BANK_SIZE);
    ui32LastRAMBankSize =
        REGIO_READ_FIELD(ui32RamInfo, PVDEC_CORE, CR_PROC_DEBUG, CR_MTX_LAST_RAM_BANK_SIZE);

    psMTXRamInfo->ui32MTXRamSize =
        ((ui32BankCount-1)*(1<<(ui32RAMBankSize+2)))+(1<<(ui32LastRAMBankSize+2));
    psMTXRamInfo->ui32MTXBankSize = ui32RAMBankSize;

    psMTXRamInfo->ui32MTXRamMask  = psMTXRamInfo->ui32MTXRamSize - 1;
    psMTXRamInfo->ui32MTXRamMask |= psMTXRamInfo->ui32MTXRamMask >> 1;
    psMTXRamInfo->ui32MTXRamMask |= psMTXRamInfo->ui32MTXRamMask >> 2;
    psMTXRamInfo->ui32MTXRamMask |= psMTXRamInfo->ui32MTXRamMask >> 4;
    psMTXRamInfo->ui32MTXRamMask |= psMTXRamInfo->ui32MTXRamMask >> 8;
    psMTXRamInfo->ui32MTXRamMask |= psMTXRamInfo->ui32MTXRamMask >> 16;

    REPORT(REPORT_MODULE_VXDIO, REPORT_INFO,
           "Got MTX RAM info: %u banks, 0x%x bank size (last 0x%x), %u total.",
            ui32BankCount, ui32RAMBankSize, ui32LastRAMBankSize, psMTXRamInfo->ui32MTXRamSize);

    return IMG_SUCCESS;
}

#if !defined(SYSBRG_BRIDGING)
/*
******************************************************************************

 @Function              PVDECIO_PDUMPSync

******************************************************************************/
IMG_RESULT PVDECIO_PDUMPSync(
    const IMG_HANDLE  hCoreContext
)
{
    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    TALPDUMP_SyncWithId(psContext->pahMemSpace[REGION_PVDEC_VLRFE_REGSPACE], VDEC_BE_FE_SYNC_ID);
    TALPDUMP_SyncWithId(psContext->pahMemSpace[REGION_PVDEC_VLRBE_REGSPACE], VDEC_BE_FE_SYNC_ID);

    return IMG_SUCCESS;
}

/*
******************************************************************************

 @Function              PVDECIO_PDUMPLock

******************************************************************************/
IMG_RESULT PVDECIO_PDUMPLock(
    const IMG_HANDLE  hCoreContext
)
{

    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    TALPDUMP_Lock(psContext->pahMemSpace[REGION_PVDEC_VLRFE_REGSPACE], VDEC_BE_FE_PDUMP_SEM);

    return IMG_SUCCESS;
}

/*
******************************************************************************

 @Function              PVDECIO_PDUMPUnLock

******************************************************************************/
IMG_RESULT PVDECIO_PDUMPUnLock(
    const IMG_HANDLE  hCoreContext
)
{

    PVDECIO_sContext * psContext = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    TALPDUMP_Unlock(psContext->pahMemSpace[REGION_PVDEC_VLRBE_REGSPACE], VDEC_BE_FE_PDUMP_SEM);

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              PVDECIO_PDUMPVerifPoll

******************************************************************************/
IMG_RESULT
PVDECIO_PDUMPVerifPoll(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32RequValue,
    IMG_UINT32          ui32Enable
)
{
    PVDECIO_sContext    * psContext  = (PVDECIO_sContext *)hCoreContext;
    IMG_RESULT          eResult;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    eResult = TALREG_Poll32(psContext->pahMemSpace[ui32MemRegion],
                            ui32Offset,
                            TAL_CHECKFUNC_ISEQUAL,
                            ui32RequValue,
                            ui32Enable,
                            5,
                            1);
    IMG_ASSERT(eResult == IMG_SUCCESS);
    return eResult;
}

/*!
******************************************************************************

 @Function              PVDECIO_PDUMPDisableCmds

******************************************************************************/
IMG_UINT32
PVDECIO_PDUMPDisableCmds(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32DisableFlags
)
{
    PVDECIO_sContext *psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext != IMG_NULL);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An valid PVDEC context handle must be provided (%d)", __LINE__);
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    IMG_ASSERT(psContext->ui32MemSpaceCount != 0);
    if (ui32MemRegion >= psContext->ui32MemSpaceCount)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid VXD memory region must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    TALPDUMP_DisableCmds(psContext->pahMemSpace[ui32MemRegion],
        ui32Offset, ui32DisableFlags);

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              PVDECIO_PDUMPGetDisableCmds

******************************************************************************/
IMG_UINT32
PVDECIO_PDUMPGetDisableCmds(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset
)
{
    PVDECIO_sContext *psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext != IMG_NULL);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An valid PVDEC context handle must be provided (%d)", __LINE__);
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    IMG_ASSERT(psContext->ui32MemSpaceCount != 0);
    if (ui32MemRegion >= psContext->ui32MemSpaceCount)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid VXD memory region must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    return TALPDUMP_GetDisableCmds(psContext->pahMemSpace[ui32MemRegion], ui32Offset);
}

/*!
******************************************************************************

 @Function              PVDECIO_PDUMPPollCircBuff

******************************************************************************/
IMG_RESULT
PVDECIO_PDUMPPollCircBuff(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    IMG_UINT32          ui32Offset,
    IMG_UINT32          ui32WriteOffsetVal,
    IMG_UINT32          ui32PacketSize,
    IMG_UINT32          ui32BufferSize
)
{
    PVDECIO_sContext    * psContext  = (PVDECIO_sContext *)hCoreContext;
    IMG_RESULT          eResult;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    eResult = TALREG_CircBufPoll32(psContext->pahMemSpace[ui32MemRegion],
                                   ui32Offset,
                                   ui32WriteOffsetVal,
                                   ui32PacketSize,
                                   ui32BufferSize,
                                   1000000,
                                   100);
    IMG_ASSERT(eResult == IMG_SUCCESS);
    return eResult;
}

/*!
******************************************************************************

 @Function              PVDECIO_PDUMPComment

******************************************************************************/
IMG_RESULT
PVDECIO_PDUMPComment(
    const IMG_HANDLE    hCoreContext,
    IMG_UINT32          ui32MemRegion,
    const IMG_CHAR *    pszComment
)
{
    PVDECIO_sContext    * psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "Invalid parameters provided (%d)", __LINE__);
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    TALPDUMP_Comment( psContext->pahMemSpace[ui32MemRegion], pszComment );

    return IMG_SUCCESS;
}
#endif

/*
******************************************************************************

 @Function              PVDECIO_GetMemSpaces

******************************************************************************/
IMG_RESULT PVDECIO_GetMemSpaces(
    IMG_HANDLE  * pahMemSpace
)
{
    IMG_ASSERT(pahMemSpace);
    if (pahMemSpace == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "Mem space struct must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    pahMemSpace[REGION_PVDEC_REGSPACE]              = TAL_GetMemSpaceHandle("P0_REG_PVDEC");
    pahMemSpace[REGION_PVDEC_CORE_REGSPACE]         = TAL_GetMemSpaceHandle("P0_REG_PVDEC_CORE");
    pahMemSpace[REGION_IMG_VIDEO_BUS4_MMU_REGSPACE] = TAL_GetMemSpaceHandle("P0_REG_VIDEO_BUS4_MMU");
    pahMemSpace[REGION_PVDEC_PIXEL_PIPE_REGSPACE]   = TAL_GetMemSpaceHandle("P0_REG_PVDEC_PIXEL");
    pahMemSpace[REGION_PVDEC_ENTROPY_PIPE_REGSPACE] = TAL_GetMemSpaceHandle("P0_REG_PVDEC_ENTROPY");
    pahMemSpace[REGION_PVDEC_MTX_CORE_REGSPACE]     = TAL_GetMemSpaceHandle("P0_REG_PVDEC_PROC");
    pahMemSpace[REGION_PVDEC_VEC_BE]                = TAL_GetMemSpaceHandle("P0_REG_PVDEC_VEC_BE");
    pahMemSpace[REGION_PVDEC_VEC_BE_CODEC]          = TAL_GetMemSpaceHandle("P0_REG_PVDEC_VEC_BE_CODEC");
    pahMemSpace[REGION_PVDEC_PIXEL_DMA_REGSPACE]    = TAL_GetMemSpaceHandle("P0_REG_PIXEL_DMAC");
    pahMemSpace[REGION_PVDEC_MSVDX_VEC]             = TAL_GetMemSpaceHandle("P0_REG_MSVDX_VEC");
    pahMemSpace[REGION_PVDEC_MSVDX_VDMC]            = TAL_GetMemSpaceHandle("P0_REG_MSVDX_VDMC");
    pahMemSpace[REGION_PVDEC_MSVDX_VDEB]            = TAL_GetMemSpaceHandle("P0_REG_MSVDX_VDEB");
    pahMemSpace[REGION_PVDEC_MSVDX_CMD]             = TAL_GetMemSpaceHandle("P0_REG_MSVDX_CMD");
    pahMemSpace[REGION_PVDEC_VLR_REGSPACE]          = TAL_GetMemSpaceHandle("P0_REG_PVDEC_COMMS_RAM");
    pahMemSpace[REGION_PVDEC_VLRFE_REGSPACE]        = TAL_GetMemSpaceHandle("P0_REG_PVDEC_COMMS_RAM_FE");
    pahMemSpace[REGION_PVDEC_VLRBE_REGSPACE]        = TAL_GetMemSpaceHandle("P0_REG_PVDEC_COMMS_RAM_BE");
    pahMemSpace[REGION_PVDEC_TEST_REGSPACE]         = TAL_GetMemSpaceHandle("P0_REG_PVDEC_TEST");

    return IMG_SUCCESS;
}

/*
******************************************************************************

 @Function              PVDECIO_Initialise

******************************************************************************/
IMG_RESULT PVDECIO_Initialise(
    IMG_BOOL      bIOBypass,
    IMG_HANDLE  * phCoreContext
)
{
    PVDECIO_sContext  * psContext = IMG_NULL;
    IMG_UINT32          ui32Result;
    IMG_UINT32          ui32i;
    IMG_HANDLE        * pahMemSpace = IMG_NULL;
#ifdef SECURE_TAL
    SECURE_sDev         sDevInfo;
    IMG_UINT32          ui32I;
#endif

    if (phCoreContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

#ifdef SECURE_TAL
    for(ui32I = 0; ui32I < gsTargetConfig.ui32DevNum; ui32I++)
    {
        sDevInfo.pszDeviceName = gsTargetConfig.pasDevices[ui32I].pszDeviceName;

        ui32Result = SECDEV_Initialise();
        if(ui32Result != IMG_SUCCESS)
        {
            goto error;
        }
        ui32Result = SECDEV_LocateDevice(&sDevInfo, SECDEV_MAPAREA_REGISTER);
        if (ui32Result != IMG_SUCCESS)
        {
            goto error;
        }
        gsTargetConfig.pasDevices[ui32I].pvKmRegBase = sDevInfo.pvKmRegBase;
        gsTargetConfig.pasDevices[ui32I].ui32RegSize = sDevInfo.ui32RegSize;
    }

    TARGET_Initialise(&gsTargetConfig);
#else
    TARGET_Initialise(IMG_NULL);
#endif

    psContext = IMG_MALLOC(sizeof(*psContext));
    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "Failed to allocate memory for PVDECIO context");
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    pahMemSpace = IMG_MALLOC(REGION_PVDEC_MAX*sizeof(IMG_HANDLE));
    IMG_ASSERT(pahMemSpace);
    if (pahMemSpace == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
            "Failed to allocate memory for memory spaces handles!");
        IMG_FREE(psContext);
        psContext = IMG_NULL;
        return IMG_ERROR_OUT_OF_MEMORY;
    }
    IMG_MEMSET(psContext, 0, sizeof(*psContext));
    psContext->pahMemSpace = pahMemSpace;
    psContext->ui32MemSpaceCount = REGION_PVDEC_MAX;
    IMG_MEMSET(pahMemSpace, 0, psContext->ui32MemSpaceCount*sizeof(IMG_HANDLE));

    /* Get PVDEC specific memspaces */
    ui32Result = PVDECIO_GetMemSpaces(pahMemSpace);
    if (ui32Result != IMG_SUCCESS)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR, "Failed to get PVDEC memspaces");
        goto error;
    }

#ifdef VDEC_USE_PVDEC_SEC
    psContext->bIOBypass = bIOBypass;
#endif

    psContext->bInitialised = IMG_TRUE;

    PVDECIO_EnableCoreClocks((IMG_HANDLE)psContext);

    /* Clear comms header in VLR */
    for (ui32i=0; ui32i<COMMS_HEADER_SIZE; ui32i++)
    {
        PVDECIO_WriteRegister(psContext, REGION_PVDEC_VLR_REGSPACE,
                              ui32i*sizeof(IMG_UINT32), 0, -1, PVDEC_DEF_PIPE);
    }

    CHECK_IO_BYPASS(IMG_FALSE)
    {
        /* Get MTX RAM info for this core. */
        ui32Result = pvdecio_MTXGetRamInfo(psContext);
        if (ui32Result != IMG_SUCCESS)
        {
            REPORT(REPORT_MODULE_VXDIO, REPORT_ERR, "Failed to get RAM size");
            goto error;
        }
    }

    *phCoreContext = (IMG_HANDLE)psContext;

    return IMG_SUCCESS;

error:
    *phCoreContext = IMG_NULL;

    if(pahMemSpace)
    {
        IMG_FREE(pahMemSpace);
        pahMemSpace = IMG_NULL;
    }

    if(psContext)
    {
        IMG_FREE(psContext);
        psContext = IMG_NULL;
    }

    return ui32Result;
}

/*
******************************************************************************

 @Function              PVDECIO_DeInitialise

******************************************************************************/
IMG_RESULT PVDECIO_DeInitialise(
    IMG_HANDLE  hCoreContext
)
{
    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

#if (defined(IMG_DEBUG_FLAG) || defined(FW_PERFORMANCE_LOGGING))
    {
        IMG_RESULT ui32Result;
        ui32Result = DEVAIO_CloseDebugFifo(&psContext->sMsgContext);
        if (ui32Result != IMG_SUCCESS)
        {
            REPORT(REPORT_MODULE_VXDIO, REPORT_WARNING,
                   "Failed to close debug fifo!");
        }
    }
#endif /* (defined(IMG_DEBUG_FLAG) || defined(FW_PERFORMANCE_LOGGING)) */

    IMG_FREE(psContext->pahMemSpace);
    psContext->pahMemSpace = IMG_NULL;
    IMG_FREE(psContext);
    psContext = IMG_NULL;

    return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function              pvdecio_StartMTX

******************************************************************************/
static IMG_VOID pvdecio_EnableMTXInterrupts(
    PVDECIO_sContext  * psContext
)
{
    IMG_UINT32  ui32HostIntMask;

    if(psContext && psContext->bClocks)
    {
        /* When DMA is used, processor is enabled with DMA linked list elements.*/
#ifndef USE_MTX_DMA_LOAD
        REPORT(REPORT_MODULE_VXDIO, REPORT_WARNING,
               "Upload of firmware using registers is not enabled yet for PVDEC, using DMA upload.");
#endif /* USE_MTX_DMA_LOAD */

        /* Enable processor interrupts. */
        ui32HostIntMask = REGIO_READ_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_ENABLE);
        REGIO_WRITE_FIELD(ui32HostIntMask, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_STATUS, CR_HOST_PROC_IRQ, 1);
        REGIO_WRITE_FIELD(ui32HostIntMask, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_STATUS, CR_HOST_MMU_FAULT_IRQ, 1);
        REGIO_WRITE_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_ENABLE, ui32HostIntMask);
    }
}

/*!
******************************************************************************

 @Function              pvdecio_SetWDTConfiguration

******************************************************************************/
static IMG_VOID pvdecio_SetWDTConfiguration(
    PVDECIO_sContext  * psContext
)
{
    if(psContext && psContext->bClocks)
    {
        IMG_UINT32  ui32WDT = 0;

        /* HW WDT control default register values - same settings for all pipes */
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_PIXEL, CR_MSVDX_FE_WDT_CONTROL, FE_WDT_CNT_CTRL, 3);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_PIXEL, CR_MSVDX_FE_WDT_CONTROL, FE_WDT_ACTION0, 1);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_PIXEL, CR_MSVDX_FE_WDT_CONTROL, FE_WDT_CLEAR_SELECT, 1);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_PIXEL, CR_MSVDX_FE_WDT_CONTROL, FE_WDT_CLKDIV_SELECT, 7);
        REGIO_WRITE_PIPE_REGISTER(psContext, 1, PVDEC_PIXEL, CR_MSVDX_FE_WDT_CONTROL, ui32WDT);

        ui32WDT = 0;
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_ENTROPY, CR_ENTROPY_WDT_CONTROL, ENTROPY_WDT_CNT_CTRL, 3);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_ENTROPY, CR_ENTROPY_WDT_CONTROL, ENTROPY_WDT_ACTION1, 1);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_ENTROPY, CR_ENTROPY_WDT_CONTROL, ENTROPY_WDT_ACTION0, 1);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_ENTROPY, CR_ENTROPY_WDT_CONTROL, ENTROPY_WDT_CLEAR_SELECT, 1);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_ENTROPY, CR_ENTROPY_WDT_CONTROL, ENTROPY_WDT_CLKDIV_SELECT, 7);
        REGIO_WRITE_REGISTER(psContext, PVDEC_ENTROPY, CR_ENTROPY_WDT_CONTROL, ui32WDT);

        ui32WDT = 0;
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_PIXEL, CR_BE_WDT_CONTROL, BE_WDT_ACTION0, 1);
        REGIO_WRITE_FIELD(ui32WDT, PVDEC_PIXEL, CR_BE_WDT_CONTROL, BE_WDT_CLKDIV_SELECT, 7);
        REGIO_WRITE_PIPE_REGISTER(psContext, 1, PVDEC_PIXEL, CR_BE_WDT_CONTROL, ui32WDT);
    }
}

#ifdef PVDEC_REG_FW_UPLOAD
#define PVDECIO_MTX_CORE_MEMORY (0x18) /*!< 0x10 for code, 0x18 for data. */
/*!
******************************************************************************

 @Function              pvdecio_LoadFW

 @Description

 This function uploads firmware into MTX using register access.

******************************************************************************/
static IMG_VOID pvdecio_LoadFW(
    PVDECIO_sContext  * psContext,
    IMG_UINT8           ui8Pipe,
    PVDECIO_sFwInfo   * psFwInfo
)
{
    IMG_UINT32 ui32Channel, ui32i;
    IMG_UINT32 ui32RamId = (IMG_UINT32)-1, ui32RegVal = 0, ui32Addr = 0;
    IMG_UINT32 ui32MTXBankSize = 1 << (psContext->sMTXRamInfo.ui32MTXBankSize + 2);
    IMG_UINT32 ui32TransferSizeWrds = psContext->sFwInfo.ui32CoreSize;
    IMG_UINT32 * pui32FwBuf = (IMG_UINT32 *)psFwInfo->pvBlobCpuAddr;

    REGIO_WRITE_FIELD(ui32RegVal, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PVDEC_REG_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32RegVal, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_CORE_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32RegVal, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_MEM_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32RegVal, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PROC_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32RegVal, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PIXEL_PROCESSING_MAN_CLK_ENABLE, 1);
    pvdecio_SetClocks(psContext, ui32RegVal);

    psContext->bClocks = IMG_TRUE;

    REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
           "Trying to upload FW binary using register interface, "
           "bank: 0x%04x, vaddr: 0x%08x, cpuaddr: 0x%x, size: 0x%08x",
           ui32MTXBankSize, psFwInfo->ui32BlobDevVirtAddr,
           psFwInfo->pvBlobCpuAddr, ui32TransferSizeWrds);

    for (ui32i = 0; ui32i < ui32TransferSizeWrds; ui32i++)
    {
        IMG_UINT32 ui32MtxTimeOut;

        if ((PVDECIO_MTX_CORE_MEMORY + (ui32Addr / ui32MTXBankSize)) != ui32RamId)
        {
            /* Initiate write access to RAM block...*/
            ui32RamId = (PVDECIO_MTX_CORE_MEMORY + (ui32Addr / ui32MTXBankSize));
            ui32RegVal = 0;
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_RAM_ACCESS_CONTROL,
                              MTX_MCMID, ui32RamId);
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_RAM_ACCESS_CONTROL,
                              MTX_MCM_ADDR, (ui32Addr>>2));
            REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_RAM_ACCESS_CONTROL,
                              MTX_MCMR, 0);
            if (ui32TransferSizeWrds > 1)
            {
                REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_RAM_ACCESS_CONTROL,
                                  MTX_MCMAI, 1);
            }
            REGIO_WRITE_REGISTER(psContext, MTX_CORE,
                                 CR_MTX_RAM_ACCESS_CONTROL, ui32RegVal);

            REPORT(REPORT_MODULE_VXDIO, REPORT_ERR, "Configured RAM access: 0x%08x",
                   ui32RegVal);
        }

        REGIO_WRITE_REGISTER(psContext, MTX_CORE,
                             CR_MTX_RAM_ACCESS_DATA_TRANSFER, pui32FwBuf[ui32i]);

        if ((ui32i % 4 == 0) && (ui32i < 0x400) && (ui32i + 3 < ui32TransferSizeWrds))
        {
            /* Dump first 0x400 words of FW binary */
            REPORT(REPORT_MODULE_VXDIO, REPORT_ERR, "0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x",
                   ui32i*sizeof(IMG_UINT32), pui32FwBuf[ui32i], pui32FwBuf[ui32i+1],
                   pui32FwBuf[ui32i+2], pui32FwBuf[ui32i+3]);
        }

        /* Wait for write to complete...*/
        ui32MtxTimeOut = PVDEC_TIMEOUT_COUNTER;
        do
        {
             /* Check MTX is OK...*/
            ui32RegVal = REGIO_READ_REGISTER(psContext, MTX_CORE, CR_MTX_FAULT0);
            if (ui32RegVal != 0)
            {
                REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
                       "Writing word to MTX failed at i: %u, addr: 0x%08x, fault: 0x%08x",
                       ui32i, ui32Addr, ui32RegVal);
                break;
            }
            ui32MtxTimeOut--;
            ui32RegVal = REGIO_READ_REGISTER(psContext,
                                             MTX_CORE, CR_MTX_RAM_ACCESS_STATUS);
        }
        while ((REGIO_READ_FIELD(ui32RegVal, MTX_CORE,
                                 CR_MTX_RAM_ACCESS_STATUS, MTX_MTX_MCM_STAT) == 0) &&
               (ui32MtxTimeOut != 0));

        if (ui32MtxTimeOut == 0)
        {
            ui32RegVal = REGIO_READ_REGISTER(psContext,
                                             MTX_CORE, CR_MTX_RAM_ACCESS_STATUS);
            REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
                   "Writing word to MTX timed out at i: %u, addr: 0x%08x, status: 0x%08x",
                   ui32i, ui32Addr, ui32RegVal);
        }

        ui32Addr+=sizeof(IMG_UINT32);
    }

    /* Write virtual address of Blob into DMAC_SETUP,
     * as firmware reads it and uses to load modules. */
    PIXEL_DMA_WriteRegMacro(psContext, 0, 0,
                            DMAC_SETUP, psFwInfo->ui32BlobDevVirtAddr);

    /* Set PC */
    REGIO_WRITE_REGISTER(psContext, MTX_CORE,
                         CR_MTX_REGISTER_READ_WRITE_DATA, 0x80900000);

    ui32RegVal = 0;
    REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE,
                      CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RNW, 0); /* Write */
    REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE,
                      CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_USPECIFIER, 5);  /* PC or PCX */
    REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE,
                      CR_MTX_REGISTER_READ_WRITE_REQUEST, MTX_RSPECIFIER, 0);  /* PC */
    REGIO_WRITE_REGISTER(psContext, MTX_CORE,
                         CR_MTX_REGISTER_READ_WRITE_REQUEST, ui32RegVal);

    /* Set the MTX running. */
    REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_ENABLE, 1);
}
#endif /* PVDEC_REG_FW_UPLOAD */

/*!
******************************************************************************

 @Function              pvdecio_startDMA

******************************************************************************/
static IMG_RESULT pvdecio_startDMA(
    PVDECIO_sContext  * psContext,
    IMG_UINT8           ui8Pipe,
    IMG_UINT32          ui32TransferSize,
    IMG_UINT32          ui32BufDevVirtAddr
)
{
    IMG_UINT32  ui32Channel;
    IMG_UINT32  ui32Reg = 0;

    IMG_ASSERT(psContext);

    psContext->bClocks = IMG_TRUE;

    /* Also Enable the PP1 clocks for DMAC */
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PVDEC_REG_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_CORE_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_MEM_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PROC_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_CORE, CR_PVDEC_MAN_CLK_ENABLE,
                      CR_PIXEL_PROCESSING_MAN_CLK_ENABLE, 1);

    pvdecio_SetClocks(psContext, ui32Reg);

    /* Set the MTX timer divide register - the value used is system dependent. */
    ui32Reg = 0;
    REGIO_WRITE_FIELD(ui32Reg, MTX_CORE, CR_MTX_SYSC_TIMERDIV, TIMER_EN, 1);    /* Enabled. */
    /* Assuming core freq same for MTX */
    REGIO_WRITE_FIELD(ui32Reg, MTX_CORE, CR_MTX_SYSC_TIMERDIV, TIMER_DIV, PVDECIO_MTX_CLK_MHZ-1);  /* Base for 1MHz timer. */
    REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_SYSC_TIMERDIV, ui32Reg);

    ui32Reg = 0;
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_PIXEL, CR_PIXEL_MAN_CLK_ENABLE, CR_PIXEL_DMAC_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_PIXEL, CR_PIXEL_MAN_CLK_ENABLE, CR_PIXEL_REG_MAN_CLK_ENABLE, 1);
    REGIO_WRITE_PIPE_REGISTER(psContext, ui8Pipe, PVDEC_PIXEL, CR_PIXEL_MAN_CLK_ENABLE, ui32Reg);

    /* Setup MTX to received DMA */

    // DMA transfers to/from the MTX have to be 32-bit aligned and in multiples of 32 bits
    REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_SYSC_CDMAA, 0 /*+ 0x80900000*/);

    ui32Reg = 0;
    /* Burst size in multiples of 64 bits (allowed values are 2 or 4) */
    REGIO_WRITE_FIELD(ui32Reg, MTX_CORE, CR_MTX_SYSC_CDMAC, BURSTSIZE, 0);
    /* False means write to MTX memory, true means read from MTX memory */
    REGIO_WRITE_FIELD(ui32Reg, MTX_CORE, CR_MTX_SYSC_CDMAC, RNW, 0);
    /* Begin transfer */
    REGIO_WRITE_FIELD(ui32Reg, MTX_CORE, CR_MTX_SYSC_CDMAC, ENABLE, 1);
    /* Transfer size */
    REGIO_WRITE_FIELD(ui32Reg, MTX_CORE, CR_MTX_SYSC_CDMAC, LENGTH, ((ui32TransferSize +7)&(~7)) + 8);
    REGIO_WRITE_REGISTER(psContext, MTX_CORE, CR_MTX_SYSC_CDMAC, ui32Reg);


    // toggle channel 0 usage between MTX and other PVDEC peripherals
    ui32Reg = 0;
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_CORE, CR_PROC_DMAC_CONTROL, CR_BOOT_ON_DMA_CH0, 1 );
    REGIO_WRITE_REGISTER(psContext, PVDEC_CORE, CR_PROC_DMAC_CONTROL, ui32Reg);

    ui32Reg = 0;
    REGIO_WRITE_FIELD(ui32Reg, PVDEC_PIXEL, CR_PIXEL_CONTROL_0, CR_DMAC_CH_SEL_FOR_MTX, 0 );
    REGIO_WRITE_PIPE_REGISTER(psContext, ui8Pipe, PVDEC_PIXEL, CR_PIXEL_CONTROL_0, ui32Reg);

    /* Setup a Simple DMA for Ch0 */
    ui32Channel = 0;
    /* Specify the holdover period to use for the channel */
    ui32Reg = 0;
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_PER_HOLD, PER_HOLD, 7);
    PIXEL_DMA_WriteRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_PER_HOLD, ui32Reg);

    /* Clear the DMAC Stats */
    PIXEL_DMA_WriteRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_IRQ_STAT, 0);

    /* Buffer device virtual address */
    PIXEL_DMA_WriteRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_SETUP, ui32BufDevVirtAddr);

    ui32Reg = 0;
    REGIO_WRITE_FIELD_LITE(ui32Reg,DMAC, DMAC_PERIPHERAL_ADDR, ADDR, MTX_CORE_CR_MTX_SYSC_CDMAT_OFFSET);
    PIXEL_DMA_WriteRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_PERIPHERAL_ADDR, ui32Reg);

    /* Clear peripheral register address */
    ui32Reg = 0;
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_PERIPH, ACC_DEL,          0                   );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_PERIPH, INCR,             DMAC_INCR_OFF       );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_PERIPH, BURST,            DMAC_BURST_1        );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_PERIPH, EXT_BURST,        DMAC_EXT_BURST_0    );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_PERIPH, EXT_SA,           0                   );
    PIXEL_DMA_WriteRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_PERIPH, ui32Reg);

    ui32Reg = 0;
    /* Now start the transfer by setting the list enable bit in the count register */
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, TRANSFER_IEN,      1                   );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, PW,                DMAC_PWIDTH_32_BIT  );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, DIR,               0/*HOST_TO_MSVDX*/  );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, PI,                DMAC_INCR_ON        );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, LIST_FIN_CTL,      0                   );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, LIST_EN,           0                   );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, ENABLE_2D_MODE,    0                   );
    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, CNT, ui32TransferSize);
    PIXEL_DMA_WriteRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_COUNT, ui32Reg);

    REGIO_WRITE_FIELD(ui32Reg, DMAC, DMAC_COUNT, EN, 1);
    PIXEL_DMA_WriteRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_COUNT, ui32Reg);

#if defined(SYSBRG_BRIDGING)
    /* Wait for DMA to finish. */
    {
        IMG_UINT32 ui32Cnt = REGIO_READ_FIELD(ui32Reg, DMAC, DMAC_COUNT, CNT);
        IMG_UINT32 ui32PrevCnt = ui32Cnt;

        do {
            usleep_range(300, 600);

            ui32PrevCnt = ui32Cnt;
            ui32Reg = PIXEL_DMA_ReadRegMacro(psContext, ui8Pipe, ui32Channel, DMAC_COUNT);
            ui32Cnt = REGIO_READ_FIELD(ui32Reg, DMAC, DMAC_COUNT, CNT);

            if(ui32Cnt == ui32PrevCnt)
            {
                REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
                       "Firmware DMA failed!");
                return IMG_ERROR_FATAL;
            }
        } while(ui32Cnt > 0);
    }
#endif

   return IMG_SUCCESS;
}


/*
******************************************************************************

 @Function              PVDECIO_KickMTX

******************************************************************************/
IMG_VOID PVDECIO_KickMTX(
    const IMG_HANDLE hCoreContext
)
{
    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32 ui32RegVal = 0;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDECIO core context handle must be provided");
        return;
    }

    REGIO_WRITE_FIELD(ui32RegVal, MTX_CORE, CR_MTX_KICKI, MTX_KICKI, 1);
    TALREG_WriteWord32(psContext->pahMemSpace[REGION_PVDEC_MTX_CORE_REGSPACE],
                       MTX_CORE_CR_MTX_KICKI_OFFSET, ui32RegVal);
}


/*!
******************************************************************************

 @Function              PVDECIO_SendFirmwareMessage

******************************************************************************/
IMG_RESULT PVDECIO_SendFirmwareMessage(
    const IMG_HANDLE    hCoreContext,
    const IMG_VOID    * psMsgHdr
)
{
    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32          ui32Result;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }
    IMG_ASSERT(psMsgHdr);
    if (psMsgHdr == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid message buffer must be provided");
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }

    ui32Result = DEVAIO_SendMTXMsg(&psContext->sMsgContext,
                                   psMsgHdr,
                                   hCoreContext);
    IMG_ASSERT(ui32Result == IMG_SUCCESS);
    if (ui32Result != IMG_SUCCESS)
    {
        goto error;
    }

    PVDECIO_KickMTX(hCoreContext);

    return IMG_SUCCESS;

error:
    return ui32Result;
}


/*!
******************************************************************************

 @Function              PVDECIO_LoadBaseFirmware

******************************************************************************/
IMG_RESULT PVDECIO_LoadBaseFirmware(
    const IMG_HANDLE  hCoreContext
)
{
    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32          ui32Result = IMG_SUCCESS;
    IMG_UINT8           ui8Pipe = 1;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        ui32Result = IMG_ERROR_INVALID_PARAMETERS;
        goto error;
    }

    /* Need to initialise MTX comms area  */
    ui32Result = DEVAIO_InitMTXComms((IMG_HANDLE)psContext,
                                     &psContext->sMsgContext,
#if defined VDEC_USE_PVDEC_SEC
                                     psContext->bSecureFw
#else
                                     IMG_FALSE
#endif
                                     );
    if(IMG_SUCCESS != ui32Result)
    {
        REPORT(REPORT_MODULE_VXDIO,
                REPORT_ERR,
                "Failed to initialise MTX comms area");
        return IMG_FALSE;
    }

    // Load base component and start the MTX.
#ifndef PVDEC_REG_FW_UPLOAD
    ui32Result = pvdecio_startDMA(psContext, ui8Pipe,
                                  psContext->sFwInfo.ui32CoreSize,
                                  psContext->sFwInfo.ui32BlobDevVirtAddr);
    if(IMG_SUCCESS != ui32Result)
    {
        REPORT(REPORT_MODULE_VXDIO,
                REPORT_ERR,
                "Failed to start DMA");
        goto error;
    }
#else /* PVDEC_REG_FW_UPLOAD */
    pvdecio_LoadFW(psContext, ui8Pipe, &psContext->sFwInfo);
#endif /* PVDEC_REG_FW_UPLOAD */

    IMG_ASSERT(ui32Result == IMG_SUCCESS);

    pvdecio_SetWDTConfiguration(psContext);

    pvdecio_EnableMTXInterrupts(psContext);


error:
    return ui32Result;
}


/*!
******************************************************************************

 @Function              PVDECIO_HandleInterrupts

******************************************************************************/
IMG_RESULT PVDECIO_HandleInterrupts(
    const IMG_HANDLE    hCoreContext,
    VXD_sIntStatus    * psIntStatus
)
{
    PVDECIO_sContext  * psContext = (PVDECIO_sContext *)hCoreContext;
    IMG_UINT32          ui32Enable;
    IMG_UINT32          ui32Result = IMG_SUCCESS;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDECIO core context handle must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    IMG_ASSERT(psIntStatus);
    if (psIntStatus == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "A valid interrupt status context must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    psIntStatus->ui32Pending = REGIO_READ_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_STATUS);
    ui32Enable = REGIO_READ_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_ENABLE);

    psIntStatus->ui32Pending &= ui32Enable;

    if (psIntStatus->ui32Pending & PVDEC_CORE_CR_PVDEC_HOST_INTERRUPT_STATUS_CR_HOST_MMU_FAULT_IRQ_MASK)
    {
        IMG_UINT32 ui32Status0, ui32Status1;

        /* Disable MMU fault interrupts at all */
        REGIO_WRITE_FIELD(ui32Enable, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_STATUS, CR_HOST_MMU_FAULT_IRQ, 0);
        REGIO_WRITE_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_HOST_INTERRUPT_ENABLE, ui32Enable);

        ui32Result = DEVAIO_ProcessMMUReport(&psContext->sMsgContext, &ui32Status0, &ui32Status1);
        IMG_ASSERT(ui32Result == IMG_SUCCESS);

        psIntStatus->MMU_FAULT_ADDR = REGIO_READ_FIELD(ui32Status0, IMG_VIDEO_BUS4_MMU, MMU_STATUS0, MMU_FAULT_ADDR) << 12 ;
        psIntStatus->MMU_PF_N_RW = REGIO_READ_FIELD(ui32Status0, IMG_VIDEO_BUS4_MMU, MMU_STATUS0, MMU_PF_N_RW);
        psIntStatus->MMU_SECURE_FAULT = REGIO_READ_FIELD(ui32Status0, IMG_VIDEO_BUS4_MMU, MMU_STATUS0, MMU_SECURE_FAULT);

        psIntStatus->ui32Requestor = REGIO_READ_FIELD(ui32Status1, IMG_VIDEO_BUS4_MMU, MMU_STATUS1, MMU_FAULT_REQ_ID);
        psIntStatus->MMU_FAULT_RNW = REGIO_READ_FIELD(ui32Status1, IMG_VIDEO_BUS4_MMU, MMU_STATUS1, MMU_FAULT_RNW);
    }

    /* Process MTX messages when:
     * 1. New interrupt came
     * 2. Not all of the messages were processed previously (due to lack of items in FreeMsgList)*/
    if (psIntStatus->ui32Pending & PVDEC_CORE_CR_PVDEC_HOST_INTERRUPT_STATUS_CR_HOST_PROC_IRQ_MASK || !psIntStatus->psMsgQueue->bEmpty)
    {
        if (!LST_empty(&psIntStatus->psMsgQueue->sFreeMsgList))
        {
            /* Clear interrupt first to allow for others coming.
             * Interrupt has to be cleared before reading comms buffer, as MTX can be currently
             * waiting for free space in this buffer. If we tried to read the buffer first,
             * following scenario could occur (on assumption that comms buffer is full):
             * -> host reads buffer
             * -> MTX starts adding new messages, as there is a space for messages now
             * -> host ends reading buffer and checks if there are any new messages
             *    (there are none as MTX did not finish yet)
             * -> MTX ends writing to buffer and signal IRQ
             * -> host clears IRQ
             * Lockup occurs as host skipped interrupt. */
            // Clear firmware message interrupt.
            //
            REGIO_WRITE_REGISTER(psContext, PVDEC_CORE, CR_PVDEC_INTERRUPT_CLEAR,
                                 PVDEC_CORE_CR_PVDEC_HOST_INTERRUPT_STATUS_CR_HOST_PROC_IRQ_MASK);

            // Copy all the messages into a temporary host buffer.
            ui32Result = DEVAIO_ProcessMTXMsgs(&psContext->sMsgContext, psIntStatus->psMsgQueue);
            IMG_ASSERT(ui32Result == IMG_SUCCESS);
        }
    }

    return ui32Result;
}

/*!
******************************************************************************

 @Function              PVDECIO_PrepareFirmware

******************************************************************************/
IMG_RESULT PVDECIO_PrepareFirmware(
    const IMG_HANDLE  hCoreContext,
    const IMG_HANDLE  hFirmware
)
{
    PVDECIO_sContext                 * psContext  = (PVDECIO_sContext *)hCoreContext;
    const VXD_sFirmware              * psFw = (const VXD_sFirmware *)hFirmware;
    const VXDIO_sDdBufInfo           * psFwBufInfo;
    const VDECFW_sFirmwareBinInfo    * psFirmwareInfo;
    IMG_RESULT                         ui32Result;

    if (hCoreContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    CHECK_IO_BYPASS(IMG_FALSE)
    {
#if defined VDEC_USE_PVDEC_SEC
#if defined SYSBRG_BRIDGING
        /* Prevent secure fw being loaded without TEE only when BRIDGING mode is on !*/
        if (IMG_FALSE)
#else
        if (psContext->bSecureFw)
#endif /* SYSBRG_BRIDGING */
        {
            psFwBufInfo = &psFw->sFwSecBufInfo;
            psFirmwareInfo = &psFw->sFwSecBinInfo;
        }
        else
#endif /* VDEC_USE_PVDEC_SEC */
        {
            psFwBufInfo = &psFw->sFwBaseBufInfo;
            psFirmwareInfo = &psFw->sFwBaseBinInfo;
        }

        /* Setup MTX base component data. */
        psContext->sFwInfo.ui32BlobDevVirtAddr = psFwBufInfo->ui32DevVirt;
#ifdef PVDEC_REG_FW_UPLOAD
        psContext->sFwInfo.pvBlobCpuAddr       = psFwBufInfo->pvCpuVirt;
#endif /* PVDEC_REG_FW_UPLOAD */
        psContext->sFwInfo.ui32BlobSize        = psFirmwareInfo->ui32TextBufSize;
#if defined VDEC_USE_PVDEC_SEC && defined SYSBRG_BRIDGING  
        /* Take the first "text' word as it contains core size (in bytes) 
         * Only applicable for non secure fw - assuming this firmware is not encrypted */
        if (!psFirmwareInfo->aui32Text)
        {
            REPORT(REPORT_MODULE_VXDIO, REPORT_ERR, "psFirmwareInfo->aui32Text is null\n");
            return IMG_FALSE;
        }
        psContext->sFwInfo.ui32CoreSize        = (*psFirmwareInfo->aui32Text) / sizeof(IMG_UINT32);
#else
        psContext->sFwInfo.ui32CoreSize        = psFirmwareInfo->ui32TextDmaSize;
#endif

        /* Copy firmware Blob into device buffer as is */
        IMG_MEMCPY(psFwBufInfo->pvCpuVirt,
#if defined VDEC_USE_PVDEC_SEC && defined SYSBRG_BRIDGING
                   psFirmwareInfo->aui32Text + 4,  // need to skip blob header
                   (psFirmwareInfo->ui32TextBufSize - 4)*sizeof(IMG_UINT32)  // copy without first four words
#else
                   psFirmwareInfo->aui32Text,
                   psFirmwareInfo->ui32TextBufSize*sizeof(IMG_UINT32)
#endif
                  );

        UPDATE_DEVICE(psFwBufInfo);
    }

    {
    IMG_UINT32          ui32i;
    /* Clear comms header in VLR */
    for (ui32i=0; ui32i<COMMS_HEADER_SIZE; ui32i++)
    {
        PVDECIO_WriteRegister(psContext, REGION_PVDEC_VLR_REGSPACE,
                            ui32i*sizeof(IMG_UINT32), 0, -1, PVDEC_DEF_PIPE);
    }
    }

    // todo:xxx: called twice ?
    /* Need to initialise MTX comms area  */
    ui32Result = DEVAIO_InitMTXComms((IMG_HANDLE)psContext,
                                     &psContext->sMsgContext,
#if defined VDEC_USE_PVDEC_SEC
                                     psContext->bSecureFw
#else
                                     IMG_FALSE
#endif
                                     );
    if(IMG_SUCCESS != ui32Result)
    {
        REPORT(REPORT_MODULE_VXDIO,
                REPORT_ERR,
                "Failed to initialise MTX comms area");
        return IMG_FALSE;
    }

    return IMG_SUCCESS;
}

#if (defined(IMG_DEBUG_FLAG) || defined(FW_PERFORMANCE_LOGGING))
/*
******************************************************************************

 @Function              PVDECIO_HandleDebugFifo

******************************************************************************/
IMG_VOID PVDECIO_HandleDebugFifo(
    const IMG_HANDLE  * hCoreContext
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return;
    }


    if (psContext->bClocks)
    {
        DEVAIO_HandleDebugFifo(&psContext->sMsgContext);
    }
}

/*
******************************************************************************

 @Function              PVDECIO_EnableDebugFifo

******************************************************************************/
IMG_VOID PVDECIO_EnableDebugFifo(
    const IMG_HANDLE  * hCoreContext
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return;
    }
#ifdef VDEC_USE_PVDEC_SEC
    if (psContext->bSecureFw)
    {
        DEVAIO_SelectSecureFwLogs(&psContext->sMsgContext);
    }
#endif
    DEVAIO_EnableDebugFifo(&psContext->sMsgContext);
}

/*
******************************************************************************

 @Function              PVDECIO_DisableDebugFIFO

******************************************************************************/
IMG_VOID PVDECIO_DisableDebugFifo(
    const IMG_HANDLE  * hCoreContext
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return;
    }

    DEVAIO_DisableDebugFifo(&psContext->sMsgContext);
}

/*
******************************************************************************

 @Function              PVDECIO_GetDebugFifoSize

******************************************************************************/
IMG_UINT32 PVDECIO_GetDebugFifoSize(
    const IMG_HANDLE  * hCoreContext
)
{
    PVDECIO_sContext  * psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle must be provided");
        return IMG_ERROR_INVALID_PARAMETERS;
    }

    return DEVAIO_GetDebugFifoSize(&psContext->sMsgContext);
}
#endif /* (defined(IMG_DEBUG_FLAG) || defined(FW_PERFORMANCE_LOGGING)) */

/*
******************************************************************************

 @Function              PVDECIO_TriggerSim

******************************************************************************/
IMG_VOID PVDECIO_TriggerSim(
    const IMG_HANDLE    hCoreContext
)
{
    const IMG_UINT32 ui32RegOffset = 0;
    IMG_UINT32 ui32I = 1000;

#if !defined(SYSBRG_BRIDGING)
    /* Disable RDW PDUMP for VLR ui32RegOffset */
    IMG_UINT32 ui32DisableFlags;
    ui32DisableFlags = PVDECIO_PDUMPGetDisableCmds(hCoreContext,
        REGION_PVDEC_VLR_REGSPACE , ui32RegOffset);
    PVDECIO_PDUMPDisableCmds(hCoreContext, REGION_PVDEC_VLR_REGSPACE, ui32RegOffset,
        ui32DisableFlags | TAL_DISABLE_CAPTURE_RDW);
#endif

    while (ui32I-- > 0)
    {
        PVDECIO_ReadRegister(hCoreContext, REGION_PVDEC_VLR_REGSPACE, ui32RegOffset, 0);
    }

#if !defined(SYSBRG_BRIDGING)
    /* Restore PDUMP disable flags for VLR ui32RegOffset */
    PVDECIO_PDUMPDisableCmds(hCoreContext, REGION_PVDEC_VLR_REGSPACE, ui32RegOffset,
        ui32DisableFlags);
#endif
}

#ifdef VDEC_USE_PVDEC_SEC
/*!
******************************************************************************

 @Function              PVDECIO_SetIOBypass

******************************************************************************/
IMG_VOID
PVDECIO_SetIOBypass(
    const IMG_HANDLE    hCoreContext,
    IMG_BOOL bEnabled
)
{
    PVDECIO_sContext * psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return;
    }

    psContext->bIOBypass = bEnabled;
}

/*!
******************************************************************************

 @Function              PVDECIO_SetSecureFw

******************************************************************************/
IMG_VOID
PVDECIO_SetSecureFw(
    const IMG_HANDLE    hCoreContext,
    IMG_BOOL bEnabled
)
{
    PVDECIO_sContext * psContext  = (PVDECIO_sContext *)hCoreContext;

    IMG_ASSERT(psContext);
    if (psContext == IMG_NULL)
    {
        REPORT(REPORT_MODULE_VXDIO, REPORT_ERR,
               "An PVDEC core context handle container must be provided");
        return;
    }

    psContext->bSecureFw = bEnabled;
}
#endif

/* EOF */
