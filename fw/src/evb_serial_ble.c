/* Project-owned SDK peripheral profile with scan/report hooks. */
#include <string.h>
#include "ble_reporter.h"
#include "gatt_client.h"

#include "co_log.h"

#include "gap_api.h"
#include "gatt_api.h"
#include "fdb_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "portable.h"
#include "btdm_mem.h"

#include "service_ota.h"

#define SP_SVC_UUID              0xFFF0
#define SP_CHAR1_UUID            0xFFF3
#define SP_CHAR2_UUID            {0xba, 0x5c, 0xFF, 0xF4, 0x04, 0xa3, 0x40, 0x71, 0xa0, 0xb5, 0x35, 0x85, 0x3e, 0xb0, 0x83, 0x07}

// Element index of gatt service table
enum
{
    SP_IDX_SERVICE,

    SP_IDX_CHAR1_DECLARATION,
    SP_IDX_CHAR1_VALUE,
    SP_IDX_CHAR1_CFG,
    SP_IDX_CHAR1_USER_DESCRIPTION,

    SP_IDX_CHAR2_DECLARATION,
    SP_IDX_CHAR2_VALUE,
    SP_IDX_CHAR2_USER_DESCRIPTION,

    SP_IDX_NB,
};

// Simple GATT Profile Service UUID: 0xFFF0
static const uint8_t sp_svc_uuid[]   = UUID16_ARR(SP_SVC_UUID);

/******************************* Characteristic 1 defination *******************************/
// Characteristic 1 UUID: 0xFFF3
// Characteristic 1 data
#define SP_CHAR1_VALUE_LEN  20
static uint8_t sp_char1_value[SP_CHAR1_VALUE_LEN] = {0};

#define SP_CHAR1_USER_DESC_LEN  17
static char sp_char1_user_desc_data[] = "Characteristic 1";

static uint8_t desc_data[2] = {0};

/******************************* Characteristic 2 defination *******************************/
// Characteristic 2 UUID: 0xBA5C-FFF4-04A3-4071-A0B5-3585-3EB0-8307
// Characteristic 2 data
#define SP_CHAR2_VALUE_LEN  5
static uint8_t sp_char2_value[SP_CHAR2_VALUE_LEN] = {0x11, 0x22, 0x33, 0x44, 0x55};

#define SP_CHAR2_USER_DESC_LEN  17
static char sp_char2_user_desc_data[] = "Characteristic 2";

static const gatt_attribute_t simple_profile_att_table[] =
{
    // Simple gatt Service Declaration
    [SP_IDX_SERVICE]                        =   {
        { UUID_SIZE_2, UUID16_ARR(GATT_PRIMARY_SERVICE_UUID) },     /* UUID */
        GATT_PROP_READ,                                             /* Permissions */
        UUID_SIZE_2,                                                /* Max size of the value */     /* Service UUID size in service declaration */
        (uint8_t*)sp_svc_uuid,                                      /* Value of the attribute */    /* Service UUID value in service declaration */
    },

    /******************************* Characteristic 1 defination *******************************/
    // Characteristic 1 Declaration
    [SP_IDX_CHAR1_DECLARATION]          =   {
        { UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID) },           /* UUID */
        GATT_PROP_READ,                                             /* Permissions */
        0,                                                          /* Max size of the value */
        NULL,                                                       /* Value of the attribute */
    },
    // Characteristic 1 Value
    [SP_IDX_CHAR1_VALUE]                =   {
        { UUID_SIZE_2, UUID16_ARR(SP_CHAR1_UUID) },                 /* UUID */
        GATT_PROP_READ | GATT_PROP_NOTI | GATT_PROP_WRITE_REQ,      /* Permissions */
        SP_CHAR1_VALUE_LEN,                                         /* Max size of the value */
        NULL,                                                       /* Value of the attribute */    /* Can assign a buffer here, or can be assigned in the application by user */
                                                                    /* When the buffer is null, if a read request is received,
                                                                     * the lower layer will report a read event to the gatt callback.
                                                                     * The user must assign a value to the data pointer in the callback event
                                                                     * to reply to the read request
                                                                     */
    },

    // Characteristic 1 client characteristic configuration
    [SP_IDX_CHAR1_CFG]                  =   {
        { UUID_SIZE_2, UUID16_ARR(GATT_CLIENT_CHAR_CFG_UUID) },     /* UUID */
        GATT_PROP_READ | GATT_PROP_WRITE_REQ,                       /* Permissions */
        2,                                                          /* Max size of the value */
        desc_data,                                                  /* Value of the attribute */    /* Can assign a buffer here, or can be assigned in the application by user */
    },

    // Characteristic 1 User Description
    [SP_IDX_CHAR1_USER_DESCRIPTION]     =   {
        { UUID_SIZE_2, UUID16_ARR(GATT_CHAR_USER_DESC_UUID) },      /* UUID */
        GATT_PROP_READ,                                             /* Permissions */
        SP_CHAR1_USER_DESC_LEN,                                     /* Max size of the value */
        (uint8_t *)sp_char1_user_desc_data,                         /* Value of the attribute */
    },

    /******************************* Characteristic 2 defination *******************************/
    // Characteristic 2 Declaration
    [SP_IDX_CHAR2_DECLARATION]          =   {
        { UUID_SIZE_2, UUID16_ARR(GATT_CHARACTER_UUID) },           /* UUID */
        GATT_PROP_READ,                                             /* Permissions */
        0,                                                          /* Max size of the value */
        NULL,                                                       /* Value of the attribute */
    },

    // Characteristic 2 Value
    [SP_IDX_CHAR2_VALUE]                =   {
        { UUID_SIZE_16, SP_CHAR2_UUID },                            /* UUID */
        GATT_PROP_READ ,                                            /* Permissions */
        SP_CHAR2_VALUE_LEN,                                         /* Max size of the value */
        sp_char2_value,                                             /* Value of the attribute */    /* Can assign a buffer here, or can be assigned in the application by user */
                                                                    /* When the buffer is null, if a read request is received,
                                                                     * the lower layer will report a read event to the gatt callback.
                                                                     * The user must assign a value to the data pointer in the callback event
                                                                     * to reply to the read request
                                                                     */
    },

    // Characteristic 2 User Description
    [SP_IDX_CHAR2_USER_DESCRIPTION]     =   {
        { UUID_SIZE_2, UUID16_ARR(GATT_CHAR_USER_DESC_UUID) },      /* UUID */
        GATT_PROP_READ,                                             /* Permissions */
        SP_CHAR2_USER_DESC_LEN,                                     /* Max size of the value */
        (uint8_t *)sp_char2_user_desc_data,                         /* Value of the attribute */
    },
};

static void app_ble_start_advertising(void);

static char local_device_name[] = "30xx_Ble_Periphreal";
static adv_handle adv;

/*
 *  Advertising data, max size is 28 bytes
*/
static uint8_t adv_data[] = {
    /* gatt service information */
    0x03,                                //length of this AD
    GAP_ADVTYPE_16BIT_MORE,              //16bit service uuid AD type
    0xff, 0xf0,                          //value.service uuid:0xFFF0

    /* local device name information */
    0x14,                                                                           //length of this AD
    GAP_ADVTYPE_LOCAL_NAME_COMPLETE,                                                //complete name AD type
    '3','0','x','x','_','B','l','e','_','P','e','r','i','p','h','r','e','a','l',    //value.local device name
};

/*
 *  Advertising scan response data, max size is 31 bytes
*/
static uint8_t adv_scan_rsp_data[] = {
    /* local device name information */
    0x14,                                                                           //length of this AD
    GAP_ADVTYPE_LOCAL_NAME_COMPLETE,                                                //complete name AD type
    '3','0','x','x','_','B','l','e','_','P','e','r','i','p','h','r','e','a','l',    //value.local device name
};

uint8_t service_id;

static uint16_t gap_callback(struct gap_event *event)
{
    et100_scan_event(event);
    et100_gatt_gap(event);
//    printf("gap_callback: type = %d\r\n", event->type);

    switch(event->type) {
    case GATT_EVT_PROFILE_ADDED:
        {
            printf("gap_callback: GATT_EVT_PROFILE_ADDED: 0x%02X\r\n", event->param.profile_added_status);
            static bool first_add = false;
            /* service profile has been added successfully, then the advertising can be started */
            if(first_add == false) {
                app_ble_start_advertising();
                first_add = true;
            }
        }
        break;

    case GAP_EVT_ADV_SET_PARAM:
        printf("adv param set: 0x%02X\r\n", event->param.adv_set_param.status);
        break;

    case GAP_EVT_ADV_SET_ADV_DATA:
        printf("adv data set: 0x%02X\r\n", event->param.adv_set_adv_data.status);
        break;

    case GAP_EVT_ADV_SET_SCAN_RSP:
        printf("adv scan rsp data set: 0x%02X\r\n", event->param.adv_set_scan_rsp.status);
        break;

    case GAP_EVT_ADV_START:
        if (event->param.adv_start.status == 0 && et100_gatt_state() == 0) et100_scan_start();
        printf("adv start :0x%02X\r\n", event->param.adv_start.status);
        break;

    case GAP_EVT_ADV_END:
        printf("adv end: 0x%02X\r\n", event->param.adv_end.status);
        break;

    case GAP_EVT_SLAVE_CONNECT:
        {
            //gap_get_link_version(event->param.connect.conidx);
            //gap_get_link_rssi(event->param.connect.conidx);
            //gap_get_link_features(event->param.connect.conidx);
            printf("slave connect[%d], connect num: %d\r\n", event->param.connect.conidx, gap_get_connect_num());
            gatt_mtu_exchange_req(service_id, event->param.connect.conidx, 247);
        }
        break;

    case GAP_EVT_DISCONNECT:
        {
            printf("gap_callback: GAP_EVT_DISCONNECT, conidx:%d, reason:0x%02X, role:%d\r\n", event->param.disconnect.conidx,
                                                                                              event->param.disconnect.reason,
                                                                                              event->param.disconnect.local_role);
            gap_adv_start(adv, 0, 0);
        }
        break;

    case GATT_EVT_MTU:
		printf("gap_callback: conidx: %d, GATT_EVT_MTU: %d\r\n", event->param.mtu_ind.conidx, event->param.mtu_ind.mtu);
        break;

    case GAP_EVT_NAME_REQ:
        {
            gap_name_req_rsp(event->param.name_req.conidx,
                                event->param.name_req.token,
                                sizeof(local_device_name),
                                (uint8_t *)local_device_name);
        }
        break;

    case GAP_EVT_APPEARANCE_REQ:
        {
            gap_appearance_req_rsp(event->param.appearance_req.conidx,
                                    event->param.appearance_req.token,
                                    GAP_APPEARE_UNKNOWN);
        }
        break;

    case GAP_EVT_LINK_PARAM_REQ:
        {
            struct gap_link_param_update_rsp rsp;
            rsp.accept = true;
            rsp.conidx = event->param.link_param_update_req.conidx;
            rsp.ce_len_max = 2;
            rsp.ce_len_min = 2;
            gap_param_update_rsp(&rsp);
        }
        break;

    case GAP_EVT_LINK_PARAM_UPDATE:
        {
            printf("conn param update,conidx:%d, con_int:%d, latency:%d, timeout%d\r\n", event->param.link_param_update.conidx,
                                                                                            event->param.link_param_update.con_interval,
                                                                                            event->param.link_param_update.con_latency,
                                                                                            event->param.link_param_update.sup_to);
        }
        break;

    case GAP_EVT_LINK_RSSI:
        printf("gap_callback: conidx: %d, GAP_EVT_LINK_RSSI: %d\r\n", event->param.gap_link_rssi.conidx, event->param.gap_link_rssi.link_rssi);
        break;

    case GAP_EVT_PHY_IND:
        printf("gap_callback: conidx: %d, GAP_EVT_PHY_IND: %d\r\n", event->param.gap_phy_ind.conidx, event->param.gap_phy_ind.tx_phy);
        break;

    case GAP_EVT_PHY_REJECT:
        printf("gap_callback: conidx: %d, GAP_EVT_PHY_REJECT, status: %d\r\n", event->param.gap_phy_update_reject.conidx, event->param.gap_phy_update_reject.status);
        break;

    case GAP_EVT_LINK_VER:
        printf("gap_callback: conidx: %d, GAP_EVT_LINK_VER\r\n", event->param.gap_link_ver.conidx);
        break;

    case GAP_EVT_LINK_FEATURE:
        printf("gap_callback: conidx: %d, GAP_EVT_LINK_FEATURE:%d\r\n", event->param.gap_link_feature.conidx, event->param.gap_link_feature.features[0]);
        {
            struct gap_ll_pkt_size_exchange_req req;
            req.conidx = event->param.gap_link_feature.conidx;
            req.tx_octets = 251;
            req.tx_time = 2120;
            gap_ll_pkt_size_exchange_req(&req);
        }
        break;

    case GAP_EVT_LL_PKT_SIZE_IND:
        printf("gap_callback: conidx: %d, GAP_EVT_LL_PKT_SIZE_IND, size: TX(%d), RX(%d).\r\n", event->param.ll_pkt_size.conidx,
                                                                                    event->param.ll_pkt_size.tx_octets,
                                                                                    event->param.ll_pkt_size.rx_octets);
        break;

    default:
        break;
    }
    return 0;
}

static uint16_t gatt_callback(struct gatt_msg *p_msg)
{
    uint8_t uuid_temp[16];
    uint16_t uuid_2 = 0;

    switch(p_msg->msg_evt) {
    case GATTC_MSG_CMP_EVT:
        {
            switch(p_msg->param.gatt_op_cmp.operation) {
            case GATT_OP_NOTIFY:
                /*opearation of notification is complete */
                printf("notify cmp, conidx:%d, status:0x%02X\r\n", p_msg->conn_idx, p_msg->param.gatt_op_cmp.status);
                break;

            default:
                break;
            }
        }
        break;

    /* Received a read request from the peer device */
    case GATTS_MSG_READ_REQ:
        {
            printf("GATTS_MSG_READ_REQ, conidx:%d, att idx:%d\r\n", p_msg->conn_idx, p_msg->att_idx);

            if(p_msg->att_idx == SP_IDX_CHAR1_VALUE)
            {
                /*
                 * Because the buffer pointer of SP_IDX_CHAR1_VALUE is NULL,
                 * read requests will be report to the application layer for user response
                */
                uint8_t read_rsp_data[] = {0x00, 0x01, 0x02, 0x03, 0x04};
                memcpy(p_msg->param.gatt_data.p_msg_data, read_rsp_data, sizeof(read_rsp_data));

                /* Return the length of response data */
                return (sizeof(read_rsp_data));
            }
        }
        break;

    /* Received a write request from the peer device */
    case GATTS_MSG_WRITE_REQ:
        {
            printf("GATTS_MSG_WRITE_REQ, conidx:%d, att idx:%d\r\n", p_msg->conn_idx, p_msg->att_idx);

            if(p_msg->att_idx == SP_IDX_CHAR1_VALUE)
            {
                printf("recv data: 0x");
                for(uint8_t i=0; i<p_msg->param.gatt_data.msg_len; i++)
                    printf("%02X", p_msg->param.gatt_data.p_msg_data[i]);
                printf("\r\n");
            }
            else if(p_msg->att_idx == SP_IDX_CHAR1_CFG)
            {
                uint8_t data[2];
                memcpy(data, p_msg->param.gatt_data.p_msg_data, 2);
                if(data[0] & 0x01)
                {
                    /* peer device enable notify */
                    printf("ntf enable, att_idx:%d\r\n", p_msg->att_idx);
                    uint8_t send_data[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
                    struct gatt_send_event ntf;
                    ntf.conidx  = p_msg->conn_idx;
                    ntf.att_idx = SP_IDX_CHAR1_VALUE;
                    ntf.p_data  = send_data;
                    ntf.data_len = sizeof(send_data);
                    ntf.svc_id = service_id;

                    /* Send a notification to the peer device */
                    gatt_notification(&ntf);
                }
            }
        }
        break;

    case GATTC_MSG_LINK_CREATE:
        printf("gatt linkk create, conidx:%d\r\n", p_msg->conn_idx);
        break;

    case GATTC_MSG_LINK_LOST:
        printf("gatt linkk lost, conidx:%d\r\n", p_msg->conn_idx);
        break;

    default:
        break;
    }

    return 0;
}

static void app_ble_add_service(void)
{
    struct gatt_service service;

    service.att_nb = SP_IDX_NB;
    service.p_att_tb = simple_profile_att_table;
    service.gatt_msg_handler = gatt_callback;   //set GATT event callback

    service_id = gatt_add_service(&service);
}

static void app_ble_start_advertising(void)
{
    /* creat a handle of advertising*/
    adv = gap_adv_create();

    gap_adv_param_t adv_param = {
        .own_addr_type = GAP_ADDR_TYPE_STATIC,          //own address type
        .adv_mode = GAP_ADV_MODE_UNDIRECT,
        .disc_mode = GAP_ADV_DISC_MODE_GEN_DISC,
        .adv_chnl_map = GAP_ADV_CHAN_ALL,
        .filt_policy = GAP_ADV_FILTER_SCAN_ANY_CON_ANY, //Policy for filtering scanning or connection requests from peer devices
        .phy_mode = GAPM_PHY_TYPE_LE_1M,
        .adv_intv_min = 160,  //advertising min interval, in unit of 0.625ms
        .adv_intv_max = 160,  //advertising max interval, in unit of 0.625ms
    };

    /* set advertising param */
    gap_adv_set_param(adv, &adv_param);
    /* set advertising data */
    gap_adv_set_adv_data(adv, adv_data, sizeof(adv_data));
    /* set advertising scan response data */
    gap_adv_set_scan_rsp(adv, adv_scan_rsp_data, sizeof(adv_scan_rsp_data));
    /* start sadvertising */
    gap_adv_start(adv, 0, 0);
}

void app_ble_init(void)
{
    printf("app_ble_init\r\n");

    /* set GAP event callback*/
    gap_set_cb_func(gap_callback);
    et100_gatt_add_profile();

    /* set security param */
    struct gap_security_param smp_param;
    smp_param.mitm = false;
    smp_param.secure_connection = false;
    smp_param.bond = true;
    smp_param.rsp_mode = ENABLE_AUTO_RSP;
    smp_param.oob_used = GAP_OOB_AUTH_DATA_NOT_PRESENT;
    smp_param.io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT;

    gap_security_param_init(&smp_param);

    /* add service profile, The GAP callback event is GATT_EVT_PROFILE_ADDED*/
    app_ble_add_service();
    ota_gatt_add_service();
}
