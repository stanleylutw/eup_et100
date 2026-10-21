/* Project-owned EVB App manager peripheral and sensor central integration. */
#include "ble_reporter.h"
#include "gatt_client.h"
#include "app_gatt_server.h"
#include "gap_api.h"
#include "gatt_api.h"
#include "service_ota.h"

static adv_handle advertising;
static uint8_t manager_service;
static unsigned advertising_created;
static uint8_t local_name[]=ET100_APP_NAME;
/* The SDK adds flags; the complete name fits its 28-byte advertising budget. */
static uint8_t advertising_data[]={16,GAP_ADVTYPE_LOCAL_NAME_COMPLETE,
    'E','U','P','_','A','I','o','T','_','H','U','B','_','F','R'};
static uint8_t scan_response[]={17,0x07,
    0x50,0x55,0x45,0x4f,0x5f,0x7d,0x7b,0x9a,0x6f,0x4f,0x8a,0x9f,1,0,0x2f,0x8d};

static void start_advertising(void)
{
    advertising=gap_adv_create();
    gap_adv_param_t params={
        .own_addr_type=GAP_ADDR_TYPE_STATIC,.adv_mode=GAP_ADV_MODE_UNDIRECT,
        .disc_mode=GAP_ADV_DISC_MODE_GEN_DISC,.adv_chnl_map=GAP_ADV_CHAN_ALL,
        .filt_policy=GAP_ADV_FILTER_SCAN_ANY_CON_ANY,.phy_mode=GAPM_PHY_TYPE_LE_1M,
        .adv_intv_min=160,.adv_intv_max=160,
    };
    gap_adv_set_param(advertising,&params);
    gap_adv_set_adv_data(advertising,advertising_data,sizeof(advertising_data));
    gap_adv_set_scan_rsp(advertising,scan_response,sizeof(scan_response));
    gap_adv_start(advertising,0,0);
}

static uint16_t gap_callback(struct gap_event *event)
{
    et100_scan_event(event);
    et100_gatt_gap(event);
    et100_app_server_gap(event);
    switch(event->type) {
    case GATT_EVT_PROFILE_ADDED:
        if(!event->param.profile_added_status && !advertising_created) {
            advertising_created=1;start_advertising();
        }
        break;
    case GAP_EVT_ADV_START:
        if(!event->param.adv_start.status && et100_gatt_state()==0) et100_scan_start();
        break;
    case GAP_EVT_SLAVE_CONNECT:
        gatt_mtu_exchange_req(manager_service,event->param.connect.conidx,247);
        break;
    case GAP_EVT_DISCONNECT:
        if(event->param.disconnect.local_role==LOCAL_ROLE_SLAVE && advertising_created &&
           !et100_app_server_active())
            gap_adv_start(advertising,0,0);
        break;
    case GAP_EVT_NAME_REQ:
        gap_name_req_rsp(event->param.name_req.conidx,event->param.name_req.token,
            sizeof(local_name)-1,local_name);
        break;
    case GAP_EVT_APPEARANCE_REQ:
        gap_appearance_req_rsp(event->param.appearance_req.conidx,
            event->param.appearance_req.token,GAP_APPEARE_UNKNOWN);
        break;
    case GAP_EVT_LINK_PARAM_REQ: {
        struct gap_link_param_update_rsp response={0};
        response.accept=true;response.conidx=event->param.link_param_update_req.conidx;
        response.ce_len_max=response.ce_len_min=2;gap_param_update_rsp(&response);
        break;
    }
    default:break;
    }
    return 0;
}

void app_ble_init(void)
{
    gap_set_cb_func(gap_callback);
    struct gap_security_param security={0};
    security.bond=true;security.rsp_mode=ENABLE_AUTO_RSP;
    security.oob_used=GAP_OOB_AUTH_DATA_NOT_PRESENT;
    security.io_cap=GAP_IO_CAP_NO_INPUT_NO_OUTPUT;
    gap_security_param_init(&security);
    /* Register the App service first, before any PROFILE_ADDED starts advertising. */
    manager_service=et100_app_server_add();
    et100_gatt_add_profile();
    ota_gatt_add_service();
}
