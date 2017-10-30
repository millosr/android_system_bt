/*****************************************************************************
**
**  Name:           bta_fm_co.c
**
**  Description:    This file contains the FM callout function implementation
**                   for Insight.
**
**  Copyright (c) 2003-2010, Broadcom Corp., All Rights Reserved.
**  Widcomm Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/

#include "bta_api.h"
#include "bta_sys.h"
#include "bta_fm_api.h"
#include "bta_fm_co.h"
#include "btif_fm.h"


/*******************************************************************************
**
** Function         bta_fm_co_init
**
** Description      This callout function is executed by FM when it is
**                  started by calling BTA_FmSetRDSMode().  This function can be
**                  used by the phone to initialize RDS decoder.
**
**
** Returns
**
*******************************************************************************/
extern "C" tBTA_FM_STATUS bta_fm_co_init(tBTA_FM_RDS_B rds_mode)
{
    APPL_TRACE_DEBUG("bta_fm_init_co");

    BTA_FmRDSInitDecoder();

    BTA_FmRDSRegister(rds_mode, 0xffffffff,
            /*BTA_FM_RDS_PI_BIT|BTA_FM_RDS_AF_BIT|BTA_FM_RDS_PS_BIT|BTA_FM_RDS_RT_BIT,*/
                btif_fm_rdsp_cback, BTIF_RDS_APP_ID);

    BTA_FmRDSResetDecoder(BTIF_RDS_APP_ID);

    return BTA_FM_OK;
}

/*******************************************************************************
**
** Function         bta_fm_co_close
**
** Description      This callout function is executed by FM when it is
**                  started by calling BTA_FmSetRDSMode() to turn off RDS mode.
**                  This function can be used by the phone to reset RDS decoder.
**
** Returns
**
*******************************************************************************/
extern "C" void bta_fm_co_close(void)
{
    BTA_FmRDSResetDecoder(BTIF_RDS_APP_ID);
}

/*******************************************************************************
**
** Function         bta_fm_co_reset_rds_engine
**
** Description      This function can be used by the phone to reset RDS decoder.
**                  after the TUNE/SEARCH operation
**
** Returns
**
*******************************************************************************/
void bta_fm_co_reset_rds_engine(void)
{
    BTA_FmRDSResetDecoder(BTIF_RDS_APP_ID);
}

/*******************************************************************************
**
** Function         bta_fm_co_rds_data/
**
** Description      This function is called by FM when RDS data is ready.
**
**
**
** Returns          void
**
*******************************************************************************/
extern "C" tBTA_FM_STATUS bta_fm_co_rds_data(uint8_t * p_data, uint16_t len)
{
    return BTA_FmRDSDecode(BTIF_RDS_APP_ID, p_data, len);
}

