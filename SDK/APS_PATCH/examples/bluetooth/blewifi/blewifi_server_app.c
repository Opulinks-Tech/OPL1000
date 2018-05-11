/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#include "ble_hci_if.h"
#include "ble_cm_if.h"
#include "ble_smp_if.h"
#include "ble_gap_if.h"
#include "ble_gatt_if.h"
#include "ble_util.h"
#include "blewifi_server_app.h"
#include "blewifi_server_app_gatt.h"
#include "blewifi_ctrl.h"

static BLE_APP_DATA_T gTheBle;

static UINT8 gBleAdvertData[] =
{
	0x02,
	GAP_ADTYPE_FLAGS,
	GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
	// connection interval range
	0x05,
	GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
	UINT16_LO(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
	UINT16_HI(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
	UINT16_LO(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
	UINT16_HI(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
	0x02,
	GAP_ADTYPE_POWER_LEVEL,
	0
};

static UINT8 gBleScanRspData[] =
{
	0xC,   // length of this data
	GAP_ADTYPE_LOCAL_NAME_COMPLETE,
	'B', 'L', 'E', 'W', 'I', 'F', 'I', ' ', 'A', 'P', 'P'
};


static void BleAppSetAdvtisingPara(UINT8 type, UINT8 own_addr_type, LE_BT_ADDR_T *peer_addr, UINT8 filter)
{
	LE_GAP_ADVERTISING_PARAM_T para;

	para.interval_min = 0x100;
	para.interval_max = 0x100;
	para.type = type;
	para.own_addr_type = own_addr_type;

	if (peer_addr)
    {
	    para.peer_addr_type = peer_addr->type;
        MemCopy(para.peer_addr, peer_addr->addr, 6);
    }
    else
    {
	    para.peer_addr_type = LE_HCI_ADV_PEER_ADDR_PUBLIC;
		MemSet(para.peer_addr, 0, 6);
    }

	para.channel_map = 0x7;
    para.filter_policy = filter;

	LeGapSetAdvParameter(&para);
}

static void BleAppSmMsgHandler(TASK task, MESSAGEID id, MESSAGE message)
{
	switch (id)
	{
		case LE_SMP_MSG_PAIRING_ACTION_IND:
		{
			LE_SMP_MSG_PAIRING_ACTION_IND_T *ind = (LE_SMP_MSG_PAIRING_ACTION_IND_T *)message;
			BLE_APP_PRINT("LE_SMP_MSG_PAIRING_ACTION_IND hdl = %x sc = %d action = %d\r\n", ind->conn_hdl, ind->sc, ind->action);

			LeSmpSecurityRsp(ind->conn_hdl, TRUE);
		}
		break;

		case LE_SMP_MSG_ENCRYPTION_CHANGE_IND:
        {
			LE_SMP_MSG_ENCRYPTION_CHANGE_IND_T *ind = (LE_SMP_MSG_ENCRYPTION_CHANGE_IND_T *)message;

            BLE_APP_PRINT("LE_SMP_MSG_ENCRYPTION_CHANGE_IND enable = %d\r\n", ind->enable);
			gTheBle.encrypted = ind->enable;
        }
        break;

		case LE_SMP_MSG_ENCRYPTION_REFRESH_IND:
		{
			LE_SMP_MSG_ENCRYPTION_REFRESH_IND_T *ind = (LE_SMP_MSG_ENCRYPTION_REFRESH_IND_T *)message;

            BLE_APP_PRINT("LE_SMP_MSG_ENCRYPTION_REFRESH_IND status = %x\r\n", ind->status);
        }
        break;

		case LE_SMP_MSG_PAIRING_COMPLETE_IND:
		{
			LE_SMP_MSG_PAIRING_COMPLETE_IND_T *ind = (LE_SMP_MSG_PAIRING_COMPLETE_IND_T *)message;

			BLE_APP_PRINT("LE_SMP_MSG_PAIRING_COMPLETE_IND status = %x\r\n", ind->status);

			if (ind->status == SYS_ERR_SUCCESS)
			{
				gTheBle.paired = TRUE;
            }
        }
		break;

		default:
		break;
    }
}

static void BleCmMsgHandler(TASK task, MESSAGEID id, MESSAGE message)
{
	switch (id)
	{
		case LE_CM_MSG_INIT_COMPLETE_CFM:
        {
			BLE_APP_PRINT("LE_CM_MSG_INIT_COMPLETE_CFM\r\n");
            LeGattInit(&gTheBle.task);
            LeSmpInit(&gTheBle.task);
            LeGapSetAdvData(sizeof(gBleAdvertData), gBleAdvertData);
            LeSetScanRspData(sizeof(gBleScanRspData), gBleScanRspData);
            BleAppSetAdvtisingPara(LE_HCI_ADV_TYPE_ADV_IND, LE_HCI_OWN_ADDR_PUBLIC, 0, LE_HCI_ADV_FILT_NONE);
        }
		break;

		case LE_CM_MSG_SET_ADVERTISING_DATA_CFM:
        {
			BLE_APP_PRINT("LE_CM_MSG_SET_ADVERTISING_DATA_CFM - Status = %x\r\n", ((LE_CM_MSG_SET_ADVERTISING_DATA_CFM_T *)message)->status);
        }
		break;

		case LE_CM_MSG_SET_SCAN_RSP_DATA_CFM:
        {
			BLE_APP_PRINT("LE_CM_MSG_SET_SCAN_RSP_DATA_CFM - Status = %x\r\n", ((LE_CM_MSG_SET_SCAN_RSP_DATA_CFM_T *)message)->status);
            BleAppSetAdvtisingPara(LE_HCI_ADV_TYPE_ADV_NONCONN_IND, LE_HCI_OWN_ADDR_PUBLIC, 0, LE_HCI_ADV_FILT_NONE);
            LeSmpSetDefaultConfig(LE_SM_IO_CAP_NO_IO, FALSE, FALSE, TRUE);
        }
		break;

		case LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM:
        {
			BLE_APP_PRINT("LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM - Status = %x\r\n", ((LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM_T *)message)->status);

            gTheBle.state = APP_STATE_IDLE;
            //LeSendMessageAfter(&gTheBle.task, BLEWIFI_APP_MSG_ENTER_ADVERTISING, 0, 500);

            blewifi_ctrl_msg_send(BLEWIFI_CTRL_MSG_BLE_INIT_COMPLETE, 0, NULL);
        }
        break;

		case LE_CM_MSG_ENTER_ADVERTISING_CFM:
		{
			LE_CM_MSG_ENTER_ADVERTISING_CFM_T *cfm = (LE_CM_MSG_ENTER_ADVERTISING_CFM_T *)message;
			BLE_APP_PRINT("LE_CM_MSG_ENTER_ADVERTISING_CFM Status = %x\r\n", cfm->status);

			if (cfm->status == SYS_ERR_SUCCESS)
			{
				gTheBle.state = APP_STATE_ADVERTISING;

				blewifi_ctrl_msg_send(BLEWIFI_CTRL_MSG_BLE_ADVERTISING_CMF, 0, NULL);
            }
        }
		break;

		case LE_CM_MSG_EXIT_ADVERTISING_CFM:
		{
			LE_CM_MSG_EXIT_ADVERTISING_CFM_T *cfm = (LE_CM_MSG_EXIT_ADVERTISING_CFM_T *)message;
            BLE_APP_PRINT("LE_CM_MSG_EXIT_ADVERTISING_CFM Status = %x\r\n", cfm->status);

			if (cfm->status == SYS_ERR_SUCCESS)
			{
				gTheBle.state = APP_STATE_IDLE;
            }
        }
		break;

        case LE_CM_CONNECTION_COMPLETE_IND:
        {
			LE_CM_CONNECTION_COMPLETE_IND_T *ind = (LE_CM_CONNECTION_COMPLETE_IND_T *)message;

			BLE_APP_PRINT("LE_CM_CONNECTION_COMPLETE_IND status = %x\r\n", ind->status);

			if (ind->status == SYS_ERR_SUCCESS)
			{
				gTheBle.state = APP_STATE_CONNECTED;

				gTheBle.conn_hdl = ind->conn_hdl;
	            gTheBle.bt_addr.type = ind->peer_addr_type;
	            MemCopy(gTheBle.bt_addr.addr, ind->peer_addr, 6);

				gTheBle.max_itvl = ind->conn_interval;
				gTheBle.latency = ind->conn_latency;
				gTheBle.sv_tmo = ind->supervison_timeout;

                BleWifiGattIndicateServiceChange(ind->conn_hdl);

                //LeSendMessageAfter(&gTheBle.task, BLEWIFI_APP_MSG_SEND_DATA_PERIODIC, 0, 1000);
                blewifi_ctrl_msg_send(BLEWIFI_CTRL_MSG_BLE_CONNECTION_COMPLETE, 0, NULL);
            }
            else
            {
				gTheBle.state = APP_STATE_IDLE;
            }
        }
        break;

        case LE_CM_MSG_SIGNAL_UPDATE_REQ:
		{
			LE_CM_MSG_SIGNAL_UPDATE_REQ_T *req = (LE_CM_MSG_SIGNAL_UPDATE_REQ_T *)message;
            BLE_APP_PRINT("LE_CM_MSG_SIGNAL_UPDATE_REQ identifier = %d\r\n", req->identifier);
			BLE_APP_PRINT("    min = %x max = %x latency = %x timeout = %x\r\n", req->interval_min, req->interval_max, req->slave_latency, req->timeout_multiplier);
            LeGapConnUpdateResponse(req->conn_hdl, req->identifier, TRUE);
        }
        break;

		case LE_CM_MSG_CONN_PARA_REQ:
		{
            LE_CM_MSG_CONN_PARA_REQ_T *req = (LE_CM_MSG_CONN_PARA_REQ_T *)message;
			BLE_APP_PRINT("LE_CM_MSG_CONN_PARA_REQ min = %x max = %x latency = %x timeout = %x\r\n", req->itv_min, req->itv_max, req->latency, req->sv_tmo);
            LeGapConnParaRequestRsp(req->conn_hdl, TRUE);
        }
        break;

		case LE_CM_MSG_CONN_UPDATE_COMPLETE_IND:
		{
			LE_CM_MSG_CONN_UPDATE_COMPLETE_IND_T *ind = (LE_CM_MSG_CONN_UPDATE_COMPLETE_IND_T *)message;
			BLE_APP_PRINT("LE_CM_MSG_CONN_UPDATE_COMPLETE_IND status = %x, itv = %x, latency = %x svt = %x\r\n", ind->status, ind->interval, ind->latency, ind->supervision_timeout);

			if (ind->status == SYS_ERR_SUCCESS)
			{
				gTheBle.max_itvl = ind->interval;
				gTheBle.latency = ind->latency;
				gTheBle.sv_tmo = ind->supervision_timeout;
            }
			else
            {
				LeGapDisconnectReq(ind->conn_hdl);
            }
        }
		break;

        case LE_CM_MSG_SET_DISCONNECT_CFM:
        {
			LE_CM_MSG_SET_DISCONNECT_CFM_T *cfm = (LE_CM_MSG_SET_DISCONNECT_CFM_T *)message;
			BLE_APP_PRINT("LE_CM_MSG_SET_DISCONNECT_CFM conn_hdl = %x status = %x\r\n", cfm->handle, cfm->status);
        }
        break;

		case LE_CM_MSG_DISCONNECT_COMPLETE_IND:
		{
			LE_CM_MSG_DISCONNECT_COMPLETE_IND_T *ind = (LE_CM_MSG_DISCONNECT_COMPLETE_IND_T *)message;
			BLEWIFI_DATA_OUT_STORE_T *s = &gTheBle.store;

			BLE_APP_PRINT("LE_CM_MSG_DISCONNECT_COMPLETE_IND conn_hdl = %x status = %x reason = %x \r\n", ind->conn_hdl, ind->status, ind->reason);

			gTheBle.state = APP_STATE_IDLE;
            //LeSendMessageAfter(&gTheBle.task, BLEWIFI_APP_MSG_ENTER_ADVERTISING, 0, 100);
            s->pidx = s->ridx;
            s->sending = 0;

            blewifi_ctrl_msg_send(BLEWIFI_CTRL_MSG_BLE_DISCONNECT, 0, NULL);
        }
        break;

		default:
        break;
    }
}

static void BleAppCopyToBuf(UINT16 len, UINT8 *data)
{
	if (gTheBle.state != APP_STATE_CONNECTED) return;

	if (len && data)
	{
		BLEWIFI_DATA_OUT_STORE_T *s = &gTheBle.store;
		UINT16 ridx = s->ridx;
        UINT16 remain = 0;
		UINT16 copyLen;

		if (len >= LE_GATT_DATA_OUT_BUF_SIZE) return;

		if ((ridx + len) > LE_GATT_DATA_OUT_BUF_SIZE) remain = ridx + len - LE_GATT_DATA_OUT_BUF_SIZE;

        copyLen = len - remain;

		MemCopy(&s->buf[ridx], data, copyLen);

		if (remain) MemCopy(s->buf, &data[copyLen], remain);

		ridx = (ridx + len) & (LE_GATT_DATA_OUT_BUF_SIZE - 1);

		s->ridx = ridx;

        LeSendMessage(&gTheBle.task, BLEWIFI_APP_MSG_SEND_TO_PEER, 0);
    }
}

static void BleAppSendToPeer(void)
{
	BLEWIFI_DATA_OUT_STORE_T *s = &gTheBle.store;

	if (s->sending < 4)
	{
		LE_ERR_STATE status;
		UINT16 ridx = s->ridx;
		UINT16 pidx = s->pidx;
		UINT16 sendLen;

		while (pidx != ridx)
		{
			if (pidx > ridx)
				sendLen = LE_GATT_DATA_OUT_BUF_SIZE - pidx;
			else
				sendLen = ridx - pidx;

			if (sendLen > (gTheBle.curr_mtu - 3)) sendLen = gTheBle.curr_mtu - 3;

		    status = LeGattCharValNotify(gTheBle.conn_hdl, s->send_hdl, sendLen, s->buf + pidx);

	    	BLE_APP_PRINT("BleAppSendToPeer pidx = %d ridx = %d status = %x sending = %d\r\n", pidx, ridx, status, s->sending);
			//APP_MEMDUMP(s->buf + pidx, sendLen);

			if (status != SYS_ERR_SUCCESS)
			{
				s->pidx = s->ridx;
                s->sending = 0;
                return;
		    }
			else
			{
				pidx = (pidx + sendLen) & (LE_GATT_DATA_OUT_BUF_SIZE - 1);
				s->sending++;
            }

            if (s->sending == 4) break;
	    }

		s->pidx = pidx;
    }
}

static void BleAppMsgHandler(TASK task, MESSAGEID id, MESSAGE message)
{
	BLE_APP_PRINT("BleAppMsgHandler id = %x\r\n", id);
	switch (id)
	{
		case BLEWIFI_APP_MSG_INITIALIZING:
        {
			LeCmInit(&gTheBle.task);
        }
        break;

		case BLEWIFI_APP_MSG_ENTER_ADVERTISING:
		{
            LE_ERR_STATE rc = LeGapAdvertisingEnable(TRUE);

			if (rc != SYS_ERR_SUCCESS)
			{
				BLE_APP_PRINT("BLEWIFI_APP_MSG_ENTER_ADVERTISING fail rc = %x\r\n", rc);
            }
        }
		break;

		case BLEWIFI_APP_MSG_EXIT_ADVERTISING:
		{
            LE_ERR_STATE rc = LeGapAdvertisingEnable(FALSE);

			if (rc != SYS_ERR_SUCCESS)
			{
				BLE_APP_PRINT("BLEWIFI_APP_MSG_EXIT_ADVERTISING fail rc = %x\r\n", rc);
            }
        }
		break;

		case BLEWIFI_APP_MSG_SEND_DATA:
        {
            if (gTheBle.state == APP_STATE_CONNECTED)
            {
				BLEWIFI_MESSAGE_T *wifi_data = (BLEWIFI_MESSAGE_T *)message;

				BleAppCopyToBuf(wifi_data->len, wifi_data->data);
            }
        }
        break;

		case BLEWIFI_APP_MSG_SEND_TO_PEER:
        {
			if (gTheBle.state == APP_STATE_CONNECTED) BleAppSendToPeer();
        }
        break;

		case BLEWIFI_APP_MSG_SEND_DATA_PERIODIC:
		{
			BLEWIFI_DATA_OUT_STORE_T *s = &gTheBle.store;

			if (gTheBle.state == APP_STATE_CONNECTED)
			{
				UINT8 buf[] = "1234567";

				s->test_num++;

				if (s->test_num > 9) s->test_num = 0;

				buf[6] = s->test_num + '0';

				BleWifiSendAppMsgToBle(BLEWIFI_APP_MSG_SEND_DATA, 7, buf);

                LeSendMessageAfter(&gTheBle.task, BLEWIFI_APP_MSG_SEND_DATA_PERIODIC, 0, 5000);
            }
        }
		break;

		default:
		break;
    }
}

static void BleAppTaskHandler(TASK task, MESSAGEID id, MESSAGE message)
{
	if ((id >= LE_GATT_MSG_BASE) && (id < LE_GATT_MSG_TOP))
	{
		BleWifiServerAppGattMsgHandler(task, id, message);
    }
    else if ((id >= BLEWIFI_APP_MSG_INITIALIZING) && (id < BLEWIFI_APP_MSG_TOP))
    {
		BleAppMsgHandler(task, id, message);
    }
	else if ((id >= LE_SMP_MSG_BASE) && (id < LE_SMP_MSG_TOP))
	{
		BleAppSmMsgHandler(task, id, message);
    }
	else if ((id >= LE_CM_MSG_BASE) && (id < LE_CM_MSG_TOP))
	{
		BleCmMsgHandler(task, id, message);
    }
}

UINT16 BleWifiGetBufFreeSize(void)
{
	UINT16 freeSize;
	BLEWIFI_DATA_OUT_STORE_T *s = &gTheBle.store;
	UINT16 ridx = s->ridx;
	UINT16 pidx = s->pidx;

	if (pidx == ridx)
		freeSize = LE_GATT_DATA_OUT_BUF_SIZE - 1;
	else if (pidx > ridx)
		freeSize = pidx - ridx - 1;
	else
		freeSize = (LE_GATT_DATA_OUT_BUF_SIZE - 1) - ridx + pidx;

	return freeSize;
}

void BleWifiSendAppMsgToBle(UINT32 id, UINT16 len, void *data)
{
	if ((id >= BLEWIFI_APP_MSG_INITIALIZING) && (id < BLEWIFI_APP_MSG_TOP))
	{
		void *p = 0;

		if (len)
        {
			MESSAGE_DATA_BULID(BLEWIFI_MESSAGE, len);

			msg->len = len;
			msg->data = MESSAGE_OFFSET(BLEWIFI_MESSAGE);
			MemCopy(msg->data, data, len);
			p = msg;
        }

	    LeSendMessage(&gTheBle.task, id, p);
    }
}

BLE_APP_DATA_T* BleAppGetEntity(void)
{
	return &gTheBle;
}

void BleWifiServerAppInit(void)
{
	BLE_APP_PRINT("BleWifiServerAppInit\r\n");
	MemSet(&gTheBle, 0, sizeof(gTheBle));

	gTheBle.state = APP_STATE_INIT;
	gTheBle.curr_mtu = 23;

	gTheBle.min_itvl = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
	gTheBle.max_itvl = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
	gTheBle.latency  = DEFAULT_DESIRED_SLAVE_LATENCY;
	gTheBle.sv_tmo   = DEFAULT_DESIRED_SUPERVERSION_TIMEOUT;

	LeHostCreateTask(&gTheBle.task, BleAppTaskHandler);
	LeSendMessage(&gTheBle.task, BLEWIFI_APP_MSG_INITIALIZING, 0);
}
