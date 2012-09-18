/*****************************************************************************
**
**  Name:          gatt_utils.c
**
**  Description:   this file contains GATT utility functions
**
**
**  Copyright (c) 2009-2011, Broadcom Corp, All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
******************************************************************************/
#include "bt_target.h"

#if BLE_INCLUDED == TRUE
    #include <string.h>
    #include "stdio.h"
    #include "gki.h"

    #include "l2cdefs.h"
    #include "gatt_int.h"
    #include "gatt_api.h"
    #include "gattdefs.h"
    #include "sdp_api.h"
    #include "btm_int.h"
/* check if [x, y] and [a, b] have overlapping range */
    #define GATT_VALIDATE_HANDLE_RANGE(x, y, a, b)   (y >= a && x <= b)

    #define GATT_GET_NEXT_VALID_HANDLE(x)    (((x)/10 + 1) * 10)

const char * const op_code_name[] =
{
    "UNKNOWN",
    "ATT_RSP_ERROR",
    "ATT_REQ_MTU",
    "ATT_RSP_MTU",
    "ATT_REQ_READ_INFO",
    "ATT_RSP_READ_INFO",
    "ATT_REQ_FIND_TYPE_VALUE",
    "ATT_RSP_FIND_TYPE_VALUE",
    "ATT_REQ_READ_BY_TYPE",
    "ATT_RSP_READ_BY_TYPE",
    "ATT_REQ_READ",
    "ATT_RSP_READ",
    "ATT_REQ_READ_BLOB",
    "ATT_RSP_READ_BLOB",
    "GATT_REQ_READ_MULTI",
    "GATT_RSP_READ_MULTI",
    "GATT_REQ_READ_BY_GRP_TYPE",
    "GATT_RSP_READ_BY_GRP_TYPE",
    "ATT_REQ_WRITE",
    "ATT_RSP_WRITE",
    "ATT_CMD_WRITE",
    "ATT_SIGN_CMD_WRITE",
    "ATT_REQ_PREPARE_WRITE",
    "ATT_RSP_PREPARE_WRITE",
    "ATT_REQ_EXEC_WRITE",
    "ATT_RSP_EXEC_WRITE",
    "Reserved",
    "ATT_HANDLE_VALUE_NOTIF",
    "Reserved",
    "ATT_HANDLE_VALUE_IND",
    "ATT_HANDLE_VALUE_CONF",
    "ATT_OP_CODE_MAX"
};

static const UINT8  base_uuid[LEN_UUID_128] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


/*******************************************************************************
**
** Function         gatt_free_pending_ind
**
** Description    Free all pending indications
**
** Returns       None
**
*******************************************************************************/
void gatt_free_pending_ind(tGATT_TCB *p_tcb)
{
    GATT_TRACE_DEBUG0("gatt_free_pending_ind");
    /* release all queued indications */
    while (p_tcb->pending_ind_q.p_first)
        GKI_freebuf (GKI_dequeue (&p_tcb->pending_ind_q));
}

/*******************************************************************************
**
** Function         gatt_delete_dev_from_srv_chg_clt_list
**
** Description    Delete a device from the service changed client lit
**
** Returns       None
**
*******************************************************************************/
void gatt_delete_dev_from_srv_chg_clt_list(BD_ADDR bd_addr)
{
    tGATTS_SRV_CHG     *p_buf;
    tGATTS_SRV_CHG_REQ  req;

    GATT_TRACE_DEBUG0 ("gatt_delete_dev_from_srv_chg_clt_list");
    if ((p_buf = gatt_is_bda_in_the_srv_chg_clt_list(bd_addr)) != NULL)
    {
        if (gatt_cb.cb_info.p_srv_chg_callback)
        {
            /* delete from NV */
            memcpy(req.srv_chg.bda, bd_addr, BD_ADDR_LEN);
            (*gatt_cb.cb_info.p_srv_chg_callback)(GATTS_SRV_CHG_CMD_REMOVE_CLIENT,&req, NULL);
        }
        GKI_freebuf (GKI_remove_from_queue (&gatt_cb.srv_chg_clt_q, p_buf));
    }

}

/*******************************************************************************
**
** Function         gatt_set_srv_chg
**
** Description      Set the service changed flag to TRUE
**
** Returns        None
**
*******************************************************************************/
void gatt_set_srv_chg(void)
{
    tGATTS_SRV_CHG *p_buf = (tGATTS_SRV_CHG *)GKI_getfirst(&gatt_cb.srv_chg_clt_q);
    tGATTS_SRV_CHG_REQ req;

    GATT_TRACE_DEBUG0 ("gatt_set_srv_chg");
    while (p_buf)
    {
        GATT_TRACE_DEBUG0 ("found a srv_chg clt");
        if (!p_buf->srv_changed)
        {
            GATT_TRACE_DEBUG0 ("set srv_changed to TRUE");
            p_buf->srv_changed= TRUE;
            memcpy(&req.srv_chg, p_buf, sizeof(tGATTS_SRV_CHG));
            if (gatt_cb.cb_info.p_srv_chg_callback)
                (*gatt_cb.cb_info.p_srv_chg_callback)(GATTS_SRV_CHG_CMD_UPDATE_CLIENT,&req, NULL);
        }
        p_buf = (tGATTS_SRV_CHG *)GKI_getnext(p_buf);
    }
}

/*******************************************************************************
**
** Function         gatt_sr_is_new_srv_chg
**
** Description     Find the app id in on the new service changed list
**
** Returns     Pointer to the found new service changed item othwerwise NULL
**
*******************************************************************************/
tGATTS_PENDING_NEW_SRV_START *gatt_sr_is_new_srv_chg(tBT_UUID *p_app_uuid128, tBT_UUID *p_svc_uuid, UINT16 svc_inst)
{
    tGATTS_HNDL_RANGE *p;
    tGATTS_PENDING_NEW_SRV_START *p_buf = (tGATTS_PENDING_NEW_SRV_START *)GKI_getfirst(&gatt_cb.pending_new_srv_start_q);

    while (p_buf != NULL)
    {
        p = p_buf->p_new_srv_start;
        if (  gatt_uuid_compare (*p_app_uuid128, p->app_uuid128)
              &&  gatt_uuid_compare (*p_svc_uuid, p->svc_uuid)
              &&  (svc_inst == p->svc_inst) )
        {
            GATT_TRACE_DEBUG0 ("gatt_sr_is_new_srv_chg: Yes");
            break;
        }
        p_buf = (tGATTS_PENDING_NEW_SRV_START *)GKI_getnext(p_buf);
    }

    return p_buf;
}


/*******************************************************************************
**
** Function     gatt_add_pending_ind
**
** Description  Add a pending indication
**
** Returns    Pointer to the current pending indication buffer, NULL no buffer available
**
*******************************************************************************/
tGATT_VALUE *gatt_add_pending_ind(tGATT_TCB  *p_tcb, tGATT_VALUE *p_ind)
{
    tGATT_VALUE   *p_buf;
    GATT_TRACE_DEBUG0 ("gatt_add_pending_ind");
    if ((p_buf = (tGATT_VALUE *)GKI_getbuf((UINT16)sizeof(tGATT_VALUE))) != NULL)
    {
        GATT_TRACE_DEBUG0 ("enqueue a pending indication");
        memcpy(p_buf, p_ind, sizeof(tGATT_VALUE));
        GKI_enqueue (&p_tcb->pending_ind_q, p_buf);
    }
    return p_buf;
}


/*******************************************************************************
**
** Function     gatt_add_pending_new_srv_start
**
** Description  Add a pending new srv start to the new service start queue
**
** Returns    Pointer to the new service start buffer, NULL no buffer available
**
*******************************************************************************/
tGATTS_PENDING_NEW_SRV_START *gatt_add_pending_new_srv_start(tGATTS_HNDL_RANGE *p_new_srv_start)
{
    tGATTS_PENDING_NEW_SRV_START   *p_buf;

    GATT_TRACE_DEBUG0 ("gatt_add_pending_new_srv_start");
    if ((p_buf = (tGATTS_PENDING_NEW_SRV_START *)GKI_getbuf((UINT16)sizeof(tGATTS_PENDING_NEW_SRV_START))) != NULL)
    {
        GATT_TRACE_DEBUG0 ("enqueue a new pending new srv start");
        p_buf->p_new_srv_start = p_new_srv_start;
        GKI_enqueue (&gatt_cb.pending_new_srv_start_q, p_buf);
    }
    return p_buf;
}


/*******************************************************************************
**
** Function     gatt_add_srv_chg_clt
**
** Description  Add a service chnage client to the service change client queue
**
** Returns    Pointer to the service change client buffer; Null no buffer available
**
*******************************************************************************/
tGATTS_SRV_CHG *gatt_add_srv_chg_clt(tGATTS_SRV_CHG *p_srv_chg)
{
    tGATTS_SRV_CHG *p_buf;
    GATT_TRACE_DEBUG0 ("gatt_add_srv_chg_clt");
    if ((p_buf = (tGATTS_SRV_CHG *)GKI_getbuf((UINT16)sizeof(tGATTS_SRV_CHG))) != NULL)
    {
        GATT_TRACE_DEBUG0 ("enqueue a srv chg client");
        memcpy(p_buf, p_srv_chg, sizeof(tGATTS_SRV_CHG));
        GKI_enqueue (&gatt_cb.srv_chg_clt_q, p_buf);
    }

    return p_buf;
}


/*******************************************************************************
**
** Function     gatt_alloc_hdl_buffer
**
** Description  Allocate a handle buufer
**
** Returns    Pointer to the allocated buffer, NULL no buffer available
**
*******************************************************************************/
tGATT_HDL_LIST_ELEM *gatt_alloc_hdl_buffer(void)
{
    UINT8 i;
    tGATT_CB    *p_cb = &gatt_cb;
    tGATT_HDL_LIST_ELEM * p_elem= &p_cb->hdl_list[0];

    for (i = 0; i < GATT_MAX_SR_PROFILES; i++, p_elem ++)
    {
        if (!p_cb->hdl_list[i].in_use)
        {
            memset(p_elem, 0, sizeof(tGATT_HDL_LIST_ELEM));
            p_elem->in_use = TRUE;
            return p_elem;
        }
    }

    return NULL;
}

/*******************************************************************************
**
** Function     gatt_find_hdl_buffer_by_handle
**
** Description  Find handle range buffer by service handle.
**
** Returns    Pointer to the buffer, NULL no buffer available
**
*******************************************************************************/
tGATT_HDL_LIST_ELEM *gatt_find_hdl_buffer_by_handle(UINT16 handle)
{
    tGATT_HDL_LIST_INFO *p_list_info= &gatt_cb.hdl_list_info;
    tGATT_HDL_LIST_ELEM      *p_list = NULL;

    p_list = p_list_info->p_first;

    while (p_list != NULL)
    {
        if (p_list->in_use && p_list->asgn_range.s_handle == handle)
        {
            return(p_list);
        }
        p_list = p_list->p_next;
    }
    return NULL;
}
/*******************************************************************************
**
** Function     gatt_find_hdl_buffer_by_app_id
**
** Description  Find handle range buffer by app ID, service and service instance ID.
**
** Returns    Pointer to the buffer, NULL no buffer available
**
*******************************************************************************/
tGATT_HDL_LIST_ELEM *gatt_find_hdl_buffer_by_app_id (tBT_UUID *p_app_uuid128,
                                                     tBT_UUID *p_svc_uuid,
                                                     UINT16 svc_inst)
{
    tGATT_HDL_LIST_INFO *p_list_info= &gatt_cb.hdl_list_info;
    tGATT_HDL_LIST_ELEM      *p_list = NULL;

    p_list = p_list_info->p_first;

    while (p_list != NULL)
    {
        if ( gatt_uuid_compare (*p_app_uuid128, p_list->asgn_range.app_uuid128)
             &&  gatt_uuid_compare (*p_svc_uuid,    p_list->asgn_range.svc_uuid)
             &&  (svc_inst == p_list->asgn_range.svc_inst) )
        {
            GATT_TRACE_DEBUG0 ("Already allocated handles for this service before!!");
            return(p_list);
        }
        p_list = p_list->p_next;
    }
    return NULL;
}
/*******************************************************************************
**
** Function         gatt_free_hdl_buffer
**
** Description     free a handle buffer
**
** Returns       None
**
*******************************************************************************/
void gatt_free_hdl_buffer(tGATT_HDL_LIST_ELEM *p)
{

    if (p)
    {
        while (p->svc_db.svc_buffer.p_first)
            GKI_freebuf (GKI_dequeue (&p->svc_db.svc_buffer));
        memset(p, 0, sizeof(tGATT_HDL_LIST_ELEM));
    }
}
/*******************************************************************************
**
** Function         gatt_free_srvc_db_buffer_app_id
**
** Description      free the service attribute database buffers by the owner of the
**                  service app ID.
**
** Returns       None
**
*******************************************************************************/
void gatt_free_srvc_db_buffer_app_id(tBT_UUID *p_app_id)
{
    tGATT_HDL_LIST_ELEM *p_elem =  &gatt_cb.hdl_list[0];
    UINT8   i;

    for (i = 0; i < GATT_MAX_SR_PROFILES; i ++, p_elem ++)
    {
        if (memcmp(p_app_id, &p_elem->asgn_range.app_uuid128, sizeof(tBT_UUID)) == 0)
        {
            while (p_elem->svc_db.svc_buffer.p_first)
                GKI_freebuf (GKI_dequeue (&p_elem->svc_db.svc_buffer));

            p_elem->svc_db.mem_free = 0;
            p_elem->svc_db.p_attr_list = p_elem->svc_db.p_free_mem = NULL;
        }
    }
}
/*******************************************************************************
**
** Function         gatt_is_last_attribute
**
** Description     Check this is the last attribute of the specified value or not
**
** Returns       TRUE - yes this is the last attribute
**
*******************************************************************************/
BOOLEAN gatt_is_last_attribute(tGATT_SRV_LIST_INFO *p_list, tGATT_SRV_LIST_ELEM *p_start, tBT_UUID value)
{
    tGATT_SRV_LIST_ELEM *p_srv= p_start->p_next;
    BOOLEAN              is_last_attribute = TRUE;
    tGATT_SR_REG        *p_rcb = NULL;
    tBT_UUID            *p_svc_uuid;

    p_list->p_last_primary = NULL;

    while (p_srv)
    {
        p_rcb = GATT_GET_SR_REG_PTR(p_srv->i_sreg);

        p_svc_uuid = gatts_get_service_uuid (p_rcb->p_db);

        if (gatt_uuid_compare(value, *p_svc_uuid))
        {
            is_last_attribute = FALSE;
            break;

        }
        p_srv = p_srv->p_next;
    }

    return is_last_attribute;

}

/*******************************************************************************
**
** Function         gatt_update_last_pri_srv_info
**
** Description     Update the the last primary info for the service list info
**
** Returns       None
**
*******************************************************************************/
void gatt_update_last_pri_srv_info(tGATT_SRV_LIST_INFO *p_list)
{
    tGATT_SRV_LIST_ELEM *p_srv= p_list->p_first;

    p_list->p_last_primary = NULL;

    while (p_srv)
    {
        if (p_srv->is_primary)
        {
            p_list->p_last_primary = p_srv;
        }
        p_srv = p_srv->p_next;
    }

}
/*******************************************************************************
**
** Function         gatts_update_srv_list_elem
**
** Description      update an element in the service list.
**
** Returns          None.
**
*******************************************************************************/
void gatts_update_srv_list_elem(UINT8 i_sreg, UINT16 handle, BOOLEAN is_primary)
{
    gatt_cb.srv_list[i_sreg].in_use         = TRUE;
    gatt_cb.srv_list[i_sreg].i_sreg    = i_sreg;
    gatt_cb.srv_list[i_sreg].s_hdl          = gatt_cb.sr_reg[i_sreg].s_hdl;
    gatt_cb.srv_list[i_sreg].is_primary     = is_primary;

    return;
}
/*******************************************************************************
**
** Function  gatt_add_a_srv_to_list
**
** Description  add an service to the list in ascending
**              order of the start handle
**
** Returns   BOOLEAN TRUE-if add is successful
**
*******************************************************************************/
BOOLEAN gatt_add_a_srv_to_list(tGATT_SRV_LIST_INFO *p_list, tGATT_SRV_LIST_ELEM *p_new)
{
    tGATT_SRV_LIST_ELEM *p_old;

    if (!p_new)
    {
        GATT_TRACE_DEBUG0("p_new==NULL");
        return FALSE;
    }

    if (!p_list->p_first)
    {
        /* this is an empty list */
        p_list->p_first =
        p_list->p_last  = p_new;
        p_new->p_next   =
        p_new->p_prev   = NULL;
    }
    else
    {
        p_old = p_list->p_first;
        while (1)
        {
            if (p_old == NULL)
            {
                p_list->p_last->p_next      = p_new;
                p_new->p_prev               = p_list->p_last;
                p_new->p_next               = NULL;
                p_list->p_last              = p_new;
                break;
            }
            else
            {
                if (p_new->s_hdl <  p_old->s_hdl)
                {
                    /* if not the first in list */
                    if (p_old->p_prev != NULL)
                        p_old->p_prev->p_next   = p_new;
                    else
                        p_list->p_first = p_new;

                    p_new->p_prev           = p_old->p_prev;
                    p_new->p_next           = p_old;
                    p_old->p_prev           = p_new;
                    break;
                }
            }
            p_old = p_old->p_next;
        }
    }
    p_list->count++;

    gatt_update_last_pri_srv_info(p_list);
    return TRUE;

}

/*******************************************************************************
**
** Function  gatt_remove_a_srv_from_list
**
** Description  Remove a service from the list
**
** Returns   BOOLEAN TRUE-if remove is successful
**
*******************************************************************************/
BOOLEAN gatt_remove_a_srv_from_list(tGATT_SRV_LIST_INFO *p_list, tGATT_SRV_LIST_ELEM *p_remove)
{
    if (!p_remove || !p_list->p_first)
    {
        GATT_TRACE_DEBUG0("p_remove==NULL || p_list->p_first==NULL");
        return FALSE;
    }

    if (p_remove->p_prev == NULL)
    {
        p_list->p_first             = p_remove->p_next;
        if (p_remove->p_next)
            p_remove->p_next->p_prev    = NULL;
    }
    else if (p_remove->p_next == NULL)
    {
        p_list->p_last              = p_remove->p_prev;
        p_remove->p_prev->p_next    = NULL;
    }
    else
    {
        p_remove->p_next->p_prev = p_remove->p_prev;
        p_remove->p_prev->p_next = p_remove->p_next;
    }
    p_list->count--;
    gatt_update_last_pri_srv_info(p_list);
    return TRUE;

}

/*******************************************************************************
**
** Function  gatt_add_an_item_to_list
**
** Description  add an service handle range to the list in decending
**              order of the start handle
**
** Returns   BOOLEAN TRUE-if add is successful
**
*******************************************************************************/
BOOLEAN gatt_add_an_item_to_list(tGATT_HDL_LIST_INFO *p_list, tGATT_HDL_LIST_ELEM *p_new)
{
    tGATT_HDL_LIST_ELEM *p_old;
    if (!p_new)
    {
        GATT_TRACE_DEBUG0("p_new==NULL");
        return FALSE;
    }

    if (!p_list->p_first)
    {
        /* this is an empty list */
        p_list->p_first =
        p_list->p_last  = p_new;
        p_new->p_next   =
        p_new->p_prev   = NULL;
    }
    else
    {
        p_old = p_list->p_first;
        while (1)
        {
            if (p_old == NULL)
            {
                p_list->p_last->p_next      = p_new;
                p_new->p_prev               = p_list->p_last;
                p_new->p_next               = NULL;
                p_list->p_last              = p_new;

                break;

            }
            else
            {
                if (p_new->asgn_range.s_handle >  p_old->asgn_range.s_handle)
                {
                    if (p_old == p_list->p_first)
                        p_list->p_first = p_new;

                    p_new->p_prev    = p_old->p_prev;
                    p_new->p_next    = p_old;


                    p_old->p_prev    = p_new;
                    break;
                }
            }
            p_old = p_old->p_next;
        }
    }
    p_list->count++;
    return TRUE;

}

/*******************************************************************************
**
** Function  gatt_remove_an_item_from_list
**
** Description  Remove an service handle range from the list
**
** Returns   BOOLEAN TRUE-if remove is successful
**
*******************************************************************************/
BOOLEAN gatt_remove_an_item_from_list(tGATT_HDL_LIST_INFO *p_list, tGATT_HDL_LIST_ELEM *p_remove)
{
    if (!p_remove || !p_list->p_first)
    {
        GATT_TRACE_DEBUG0("p_remove==NULL || p_list->p_first==NULL");
        return FALSE;
    }

    if (p_remove->p_prev == NULL)
    {
        p_list->p_first             = p_remove->p_next;
        if (p_remove->p_next)
            p_remove->p_next->p_prev    = NULL;
    }
    else if (p_remove->p_next == NULL)
    {
        p_list->p_last              = p_remove->p_prev;
        p_remove->p_prev->p_next    = NULL;
    }
    else
    {
        p_remove->p_next->p_prev = p_remove->p_prev;
        p_remove->p_prev->p_next = p_remove->p_next;
    }
    p_list->count--;
    return TRUE;

}

/*******************************************************************************
**
** Function         gatt_find_the_connected_bda
**
** Description      This function find the connected bda
**
** Returns           TRUE if found
**
*******************************************************************************/
BOOLEAN gatt_find_the_connected_bda(UINT8 start_idx, BD_ADDR bda, UINT8 *p_found_idx)
{
    UINT8 i;
    BOOLEAN found = FALSE;
    GATT_TRACE_DEBUG1("gatt_find_the_connected_bda start_idx=%d",start_idx);

    for (i = start_idx ; i < GATT_MAX_PHY_CHANNEL; i ++)
    {
        if (gatt_cb.tcb[i].in_use)
        {
            memcpy( bda, gatt_cb.tcb[i].peer_bda, BD_ADDR_LEN);
            *p_found_idx = i;
            found = TRUE;
            GATT_TRACE_DEBUG6("gatt_find_the_connected_bda bda :%02x-%02x-%02x-%02x-%02x-%02x",
                              bda[0],  bda[1], bda[2],  bda[3], bda[4],  bda[5]);
            break;
        }
    }
    GATT_TRACE_DEBUG2("gatt_find_the_connected_bda found=%d found_idx=%d", found, i);
    return found;
}



/*******************************************************************************
**
** Function         gatt_is_srv_chg_ind_pending
**
** Description      Check whether a service chnaged is in the indication pending queue
**                  or waiting for an Ack already
**
** Returns         BOOLEAN
**
*******************************************************************************/
BOOLEAN gatt_is_srv_chg_ind_pending (tGATT_TCB *p_tcb)
{
    tGATT_VALUE *p_buf = (tGATT_VALUE *)GKI_getfirst(&p_tcb->pending_ind_q);
    BOOLEAN srv_chg_ind_pending = FALSE;

    GATT_TRACE_DEBUG1("gatt_is_srv_chg_ind_pending is_queue_empty=%d", GKI_queue_is_empty(&p_tcb->pending_ind_q) );

    if (p_tcb->indicate_handle == gatt_cb.handle_of_h_r)
    {
        srv_chg_ind_pending = TRUE;
    }
    else
    {
        while (p_buf)
        {
            if (p_buf->handle == gatt_cb.handle_of_h_r)
            {
                srv_chg_ind_pending = TRUE;
                break;
            }
            p_buf = (tGATT_VALUE *)GKI_getnext(p_buf);
        }
    }

    GATT_TRACE_DEBUG1("srv_chg_ind_pending = %d", srv_chg_ind_pending);
    return srv_chg_ind_pending;
}


/*******************************************************************************
**
** Function         gatt_is_bda_in_the_srv_chg_clt_list
**
** Description      This function check the specified bda is in the srv chg clinet list or not
**
** Returns         pointer to the found elemenet otherwise NULL
**
*******************************************************************************/
tGATTS_SRV_CHG *gatt_is_bda_in_the_srv_chg_clt_list (BD_ADDR bda)
{
    tGATTS_SRV_CHG *p_buf = (tGATTS_SRV_CHG *)GKI_getfirst(&gatt_cb.srv_chg_clt_q);

    GATT_TRACE_DEBUG6("gatt_is_bda_in_the_srv_chg_clt_list :%02x-%02x-%02x-%02x-%02x-%02x",
                      bda[0],  bda[1], bda[2],  bda[3], bda[4],  bda[5]);

    while (p_buf != NULL)
    {
        if (!memcmp( bda, p_buf->bda, BD_ADDR_LEN))
        {
            GATT_TRACE_DEBUG0("bda is in the srv chg clt list");
            break;
        }
        p_buf = (tGATTS_SRV_CHG *)GKI_getnext(p_buf);
    }

    return p_buf;
}


/*******************************************************************************
**
** Function         gatt_is_bda_connected
**
** Description
**
** Returns           GATT_INDEX_INVALID if not found. Otherwise index to the tcb.
**
*******************************************************************************/
BOOLEAN gatt_is_bda_connected(BD_ADDR bda)
{
    UINT8 i = 0;
    BOOLEAN connected=FALSE;

    for ( i=0; i < GATT_MAX_PHY_CHANNEL; i ++)
    {
        if (gatt_cb.tcb[i].in_use &&
            !memcmp(gatt_cb.tcb[i].peer_bda, bda, BD_ADDR_LEN))
        {
            connected = TRUE;
            break;
        }
    }
    return connected;
}

/*******************************************************************************
**
** Function         gatt_find_i_tcb_by_addr
**
** Description      The function searches for an empty tcb entry, and return the index.
**
** Returns           GATT_INDEX_INVALID if not found. Otherwise index to the tcb.
**
*******************************************************************************/
UINT8 gatt_find_i_tcb_by_addr(BD_ADDR bda)
{
    UINT8 i = 0, j = GATT_INDEX_INVALID;

    for ( ; i < GATT_MAX_PHY_CHANNEL; i ++)
    {
        if (!memcmp(gatt_cb.tcb[i].peer_bda, bda, BD_ADDR_LEN))
        {
            j = i;
            break;
        }
    }
    return j;
}


/*******************************************************************************
**
** Function         gatt_get_tcb_by_idx
**
** Description      The function get TCB using the TCB index
**
** Returns           NULL if not found. Otherwise index to the tcb.
**
*******************************************************************************/
tGATT_TCB * gatt_get_tcb_by_idx(UINT8 tcb_idx)
{
    tGATT_TCB   *p_tcb = NULL;

    if ( (tcb_idx < GATT_MAX_PHY_CHANNEL) && gatt_cb.tcb[tcb_idx].in_use)
        p_tcb = &gatt_cb.tcb[tcb_idx];

    return p_tcb;
}

/*******************************************************************************
**
** Function         gatt_find_tcb_by_addr
**
** Description      The function searches for an empty tcb entry, and return pointer.
**
** Returns           NULL if not found. Otherwise index to the tcb.
**
*******************************************************************************/
tGATT_TCB * gatt_find_tcb_by_addr(BD_ADDR bda)
{
    tGATT_TCB   *p_tcb = NULL;
    UINT8 i = 0;

    if ((i = gatt_find_i_tcb_by_addr(bda)) != GATT_INDEX_INVALID)
        p_tcb = &gatt_cb.tcb[i];

    return p_tcb;
}
/*******************************************************************************
**
** Function         gatt_find_i_tcb_free
**
** Description      The function searches for an empty tcb entry, and return the index.
**
** Returns           GATT_INDEX_INVALID if not found. Otherwise index to the tcb.
**
*******************************************************************************/
UINT8 gatt_find_i_tcb_free(void)
{
    UINT8 i = 0, j = GATT_INDEX_INVALID;

    for (i = 0; i < GATT_MAX_PHY_CHANNEL; i ++)
    {
        if (!gatt_cb.tcb[i].in_use)
        {
            j = i;
            break;
        }
    }
    return j;
}
/*******************************************************************************
**
** Function         gatt_allocate_tcb_by_bdaddr
**
** Description      The function locate or allocate new tcb entry for matching bda.
**
** Returns           GATT_INDEX_INVALID if not found. Otherwise index to the tcb.
**
*******************************************************************************/
tGATT_TCB * gatt_allocate_tcb_by_bdaddr(BD_ADDR bda)
{
    UINT8 i = 0;
    BOOLEAN allocated = FALSE;
    tGATT_TCB    *p_tcb = NULL;

    /* search for existing tcb with matching bda    */
    i = gatt_find_i_tcb_by_addr(bda);
    /* find free tcb */
    if (i == GATT_INDEX_INVALID)
    {
        i = gatt_find_i_tcb_free();
        allocated = TRUE;
    }
    if (i != GATT_INDEX_INVALID)
    {
        p_tcb = &gatt_cb.tcb[i];

        if (allocated)
        {
            memset(p_tcb, 0, sizeof(tGATT_TCB));
            p_tcb->in_use = TRUE;
            p_tcb->tcb_idx = i;
        }
        memcpy(p_tcb->peer_bda, bda, BD_ADDR_LEN);
    }
    return p_tcb;
}

/*******************************************************************************
**
** Function         gatt_convert_uuid16_to_uuid128
**
** Description      Convert a 16 bits UUID to be an standard 128 bits one.
**
** Returns          TRUE if two uuid match; FALSE otherwise.
**
*******************************************************************************/
void gatt_convert_uuid16_to_uuid128(UINT8 uuid_128[LEN_UUID_128], UINT16 uuid_16)
{
    UINT8   *p = &uuid_128[LEN_UUID_128 - 4];

    memcpy (uuid_128, base_uuid, LEN_UUID_128);

    UINT16_TO_STREAM(p, uuid_16);
}

/*******************************************************************************
**
** Function         gatt_uuid_compare
**
** Description      Compare two UUID to see if they are the same.
**
** Returns          TRUE if two uuid match; FALSE otherwise.
**
*******************************************************************************/
BOOLEAN gatt_uuid_compare (tBT_UUID src, tBT_UUID tar)
{
    UINT8  su[LEN_UUID_128], tu[LEN_UUID_128];
    UINT8  *ps, *pt;

    /* any of the UUID is unspecified */
    if (src.len == 0 || tar.len == 0)
    {
        return TRUE;
    }

    /* If both are 16-bit, we can do a simple compare */
    if (src.len == 2 && tar.len == 2)
    {
        return src.uu.uuid16 == tar.uu.uuid16;
    }

    /* One or both of the UUIDs is 128-bit */
    if (src.len == LEN_UUID_16)
    {
        /* convert a 16 bits UUID to 128 bits value */
        gatt_convert_uuid16_to_uuid128(su, src.uu.uuid16);
        ps = su;
    }
    else
        ps = src.uu.uuid128;

    if (tar.len == LEN_UUID_16)
    {
        /* convert a 16 bits UUID to 128 bits value */
        gatt_convert_uuid16_to_uuid128(tu, tar.uu.uuid16);
        pt = tu;
    }
    else
        pt = tar.uu.uuid128;

    return(memcmp(ps, pt, LEN_UUID_128) == 0);
}

/*******************************************************************************
**
** Function         gatt_build_uuid_to_stream
**
** Description      Add UUID into stream.
**
** Returns          UUID length.
**
*******************************************************************************/
UINT8 gatt_build_uuid_to_stream(UINT8 **p_dst, tBT_UUID uuid)
{
    UINT8   *p = *p_dst;
    UINT8   len = 0;

    if (uuid.len == LEN_UUID_16)
    {
        UINT16_TO_STREAM (p, uuid.uu.uuid16);
        len = LEN_UUID_16;
    }
    else if (uuid.len == LEN_UUID_128)
    {
        ARRAY_TO_STREAM (p, uuid.uu.uuid128, LEN_UUID_128);
        len = LEN_UUID_128;
    }

    *p_dst = p;
    return len;
}

/*******************************************************************************
**
** Function         gatt_parse_uuid_from_cmd
**
** Description      Convert a 128 bits UUID into a 16 bits UUID.
**
** Returns          TRUE if command sent, otherwise FALSE.
**
*******************************************************************************/
BOOLEAN gatt_parse_uuid_from_cmd(tBT_UUID *p_uuid_rec, UINT16 uuid_size, UINT8 **p_data)
{
    BOOLEAN is_base_uuid, ret = TRUE;
    UINT8  xx;
    UINT8 *p_uuid = *p_data;

    memset(p_uuid_rec, 0, sizeof(tBT_UUID));

    switch (uuid_size)
    {
        case LEN_UUID_16:
            p_uuid_rec->len = uuid_size;
            STREAM_TO_UINT16 (p_uuid_rec->uu.uuid16, p_uuid);
            *p_data += LEN_UUID_16;
            break;

        case LEN_UUID_128:
            /* See if we can compress his UUID down to 16 or 32bit UUIDs */
            is_base_uuid = TRUE;
            for (xx = 0; xx < LEN_UUID_128 - 4; xx++)
            {
                if (p_uuid[xx] != base_uuid[xx])
                {
                    is_base_uuid = FALSE;
                    break;
                }
            }
            if (is_base_uuid)
            {
                if ((p_uuid[LEN_UUID_128 - 1] == 0) && (p_uuid[LEN_UUID_128 - 2] == 0))
                {
                    p_uuid += (LEN_UUID_128 - 4);
                    p_uuid_rec->len = LEN_UUID_16;
                    STREAM_TO_UINT16(p_uuid_rec->uu.uuid16, p_uuid);
                }
                else
                    is_base_uuid = FALSE;
            }
            if (!is_base_uuid)
            {
                p_uuid_rec->len = LEN_UUID_128;
                memcpy(p_uuid_rec->uu.uuid128, p_uuid, LEN_UUID_128);
            }
            *p_data += LEN_UUID_128;
            break;

        case 0:
        default:
            if (uuid_size != 0) ret = FALSE;
            GATT_TRACE_WARNING0("gatt_parse_uuid_from_cmd invalid uuid size");
            break;
    }

    return( ret);
}

/*******************************************************************************
**
** Function         gatt_start_rsp_timer
**
** Description      Start a wait_for_response timer.
**
** Returns          TRUE if command sent, otherwise FALSE.
**
*******************************************************************************/
void gatt_start_rsp_timer(tGATT_TCB    *p_tcb)
{
    p_tcb->rsp_timer_ent.param  = (TIMER_PARAM_TYPE)p_tcb;
    btu_start_timer (&p_tcb->rsp_timer_ent, BTU_TTYPE_ATT_WAIT_FOR_RSP,
                     GATT_WAIT_FOR_RSP_TOUT);
}
/*******************************************************************************
**
** Function         gatt_start_conf_timer
**
** Description      Start a wait_for_confirmation timer.
**
** Returns          TRUE if command sent, otherwise FALSE.
**
*******************************************************************************/
void gatt_start_conf_timer(tGATT_TCB    *p_tcb)
{
    p_tcb->conf_timer_ent.param  = (TIMER_PARAM_TYPE)p_tcb;
    btu_start_timer (&p_tcb->conf_timer_ent, BTU_TTYPE_ATT_WAIT_FOR_RSP,
                     GATT_WAIT_FOR_RSP_TOUT);
}
/*******************************************************************************
**
** Function         gatt_start_ind_ack_timer
**
** Description      start the application ack timer
**
** Returns          void
**
*******************************************************************************/
void gatt_start_ind_ack_timer(tGATT_TCB *p_tcb)
{
    p_tcb->ind_ack_timer_ent.param  = (TIMER_PARAM_TYPE)p_tcb;
    /* start notification cache timer */
    btu_start_timer (&p_tcb->ind_ack_timer_ent, BTU_TTYPE_ATT_WAIT_FOR_IND_ACK,
                     GATT_WAIT_FOR_RSP_TOUT);

}
/*******************************************************************************
**
** Function         gatt_rsp_timeout
**
** Description      Called when GATT wait for ATT command response timer expires
**
** Returns          void
**
*******************************************************************************/
void gatt_rsp_timeout(TIMER_LIST_ENT *p_tle)
{
    GATT_TRACE_WARNING0("gatt_rsp_timeout disconnecting...");
    gatt_disconnect (((tGATT_TCB *)p_tle->param)->peer_bda);
}

/*******************************************************************************
**
** Function         gatt_ind_ack_timeout
**
** Description      Called when GATT wait for ATT handle confirmation timeout
**
** Returns          void
**
*******************************************************************************/
void gatt_ind_ack_timeout(TIMER_LIST_ENT *p_tle)
{
    tGATT_TCB * p_tcb = (tGATT_TCB *)p_tle->param;

    GATT_TRACE_WARNING0("gatt_ind_ack_timeout send ack now");

    if (p_tcb != NULL)
        p_tcb->ind_count = 0;

    attp_send_cl_msg(((tGATT_TCB *)p_tle->param), 0, GATT_HANDLE_VALUE_CONF, NULL);
}
/*******************************************************************************
**
** Function         gatt_sr_find_i_rcb_by_handle
**
** Description      The function searches for a service that owns a specific handle.
**
** Returns          GATT_MAX_SR_PROFILES if not found. Otherwise index of th eservice.
**
*******************************************************************************/
UINT8 gatt_sr_find_i_rcb_by_handle(UINT16 handle)
{
    UINT8  i_rcb = 0;

    for ( ; i_rcb < GATT_MAX_SR_PROFILES; i_rcb++)
    {
        if (gatt_cb.sr_reg[i_rcb].in_use &&
            gatt_cb.sr_reg[i_rcb].s_hdl <= handle &&
            gatt_cb.sr_reg[i_rcb].e_hdl >= handle )
        {
            break;
        }
    }
    return i_rcb;
}

/*******************************************************************************
**
** Function         gatt_sr_find_i_rcb_by_handle
**
** Description      The function searches for a service that owns a specific handle.
**
** Returns          0 if not found. Otherwise index of th eservice.
**
*******************************************************************************/
UINT8 gatt_sr_find_i_rcb_by_app_id(tBT_UUID *p_app_uuid128, tBT_UUID *p_svc_uuid, UINT16 svc_inst)
{
    UINT8           i_rcb = 0;
    tGATT_SR_REG    *p_sreg;
    tBT_UUID        *p_this_uuid;

    for (i_rcb = 0, p_sreg = gatt_cb.sr_reg; i_rcb < GATT_MAX_SR_PROFILES; i_rcb++, p_sreg++)
    {
        if ( p_sreg->in_use )
        {
            p_this_uuid = gatts_get_service_uuid (p_sreg->p_db);

            if (p_this_uuid &&
                gatt_uuid_compare (*p_app_uuid128, p_sreg->app_uuid ) &&
                gatt_uuid_compare (*p_svc_uuid, *p_this_uuid) &&
                (svc_inst == p_sreg->service_instance))
            {
                GATT_TRACE_ERROR0 ("Active Service Found ");
                gatt_dbg_display_uuid(*p_svc_uuid);

                break;
            }
        }
    }
    return i_rcb;
}
/*******************************************************************************
**
** Function         gatt_sr_find_i_rcb_by_handle
**
** Description      The function searches for a service that owns a specific handle.
**
** Returns          0 if not found. Otherwise index of th eservice.
**
*******************************************************************************/
UINT8 gatt_sr_alloc_rcb(tGATT_HDL_LIST_ELEM *p_list )
{
    UINT8   ii = 0;
    tGATT_SR_REG    *p_sreg = NULL;

    /*this is a new application servoce start */
    for (ii = 0, p_sreg = gatt_cb.sr_reg; ii < GATT_MAX_SR_PROFILES; ii++, p_sreg++)
    {
        if (!p_sreg->in_use)
        {
            memset (p_sreg, 0, sizeof(tGATT_SR_REG));

            p_sreg->in_use = TRUE;
            memcpy (&p_sreg->app_uuid, &p_list->asgn_range.app_uuid128, sizeof(tBT_UUID));

            p_sreg->service_instance    = p_list->asgn_range.svc_inst;
            p_sreg->type                = p_list->asgn_range.is_primary ? GATT_UUID_PRI_SERVICE: GATT_UUID_SEC_SERVICE;
            p_sreg->s_hdl               = p_list->asgn_range.s_handle;
            p_sreg->e_hdl               = p_list->asgn_range.e_handle;
            //p_sreg->sr_cb               = *p_cback;
            p_sreg->p_db                = &p_list->svc_db;

            GATT_TRACE_DEBUG1 ("total GKI buffer in db [%d]",p_sreg->p_db->svc_buffer.count);
            break;
        }
    }

    return ii;
}
/*******************************************************************************
**
** Function         gatt_sr_get_sec_info
**
** Description      Get the security flag and key size information for the peer
**                  device.
**
** Returns          void
**
*******************************************************************************/
void gatt_sr_get_sec_info(BD_ADDR rem_bda, BOOLEAN le_conn, UINT8 *p_sec_flag, UINT8 *p_key_size)
{
    UINT8           sec_flag = 0;

    BTM_GetSecurityFlags(rem_bda, &sec_flag);

    sec_flag &= (GATT_SEC_FLAG_LKEY_UNAUTHED | GATT_SEC_FLAG_LKEY_AUTHED | GATT_SEC_FLAG_ENCRYPTED);

    *p_key_size = btm_ble_read_sec_key_size(rem_bda);
    *p_sec_flag = sec_flag;
}
/*******************************************************************************
**
** Function         gatt_sr_send_req_callback
**
** Description
**
**
** Returns          void
**
*******************************************************************************/
void gatt_sr_send_req_callback(UINT16 conn_id,
                               UINT32 trans_id,
                               tGATTS_REQ_TYPE type, tGATTS_DATA *p_data)
{
    tGATT_IF        gatt_if = GATT_GET_GATT_IF(conn_id);
    tGATT_REG       *p_reg = gatt_get_regcb(gatt_if);

    if (!p_reg )
    {
        GATT_TRACE_ERROR0 ("p_reg not found discard request");
        return;
    }

    if ( p_reg->in_use &&
         p_reg->app_cb.p_req_cb)
    {
        (*p_reg->app_cb.p_req_cb)(conn_id, trans_id, type, p_data);
    }
    else
    {
        GATT_TRACE_WARNING1("Call back not found for application conn_id=%d", conn_id);
    }

}

/*******************************************************************************
**
** Function         gatt_send_error_rsp
**
** Description      This function sends an error response.
**
** Returns          void
**
*******************************************************************************/
tGATT_STATUS gatt_send_error_rsp (tGATT_TCB *p_tcb, UINT8 err_code, UINT8 op_code,
                                  UINT16 handle, BOOLEAN deq)
{
    tGATT_ERROR      error;
    tGATT_STATUS     status;
    BT_HDR           *p_buf;

    error.cmd_code = op_code;
    error.reason = err_code;
    error.handle =handle;

    if ((p_buf = attp_build_sr_msg(p_tcb, GATT_RSP_ERROR, (tGATT_SR_MSG *)&error)) != NULL)
    {
        status = attp_send_sr_msg (p_tcb, p_buf);
    }
    else
        status = GATT_INSUF_RESOURCE;

    if (deq)
        gatt_dequeue_sr_cmd(p_tcb);

    return status;
}


/*******************************************************************************
**
** Function         gatt_add_sdp_record
**
** Description      This function add a SDP record for a GATT primary service
**
** Returns          0 if error else sdp handle for the record.
**
*******************************************************************************/
UINT32 gatt_add_sdp_record (tBT_UUID *p_uuid, UINT16 start_hdl, UINT16 end_hdl)
{
    tSDP_PROTOCOL_ELEM  proto_elem_list[2];
    UINT32              sdp_handle;
    UINT16              list = UUID_SERVCLASS_PUBLIC_BROWSE_GROUP;
    UINT8               buff[60];
    UINT8               *p = buff;

    GATT_TRACE_DEBUG2("gatt_add_sdp_record s_hdl=0x%x  s_hdl=0x%x",start_hdl, end_hdl);

    if ((sdp_handle = SDP_CreateRecord()) == 0)
        return 0;

    switch (p_uuid->len)
    {
        case LEN_UUID_16:
            SDP_AddServiceClassIdList(sdp_handle, 1, &p_uuid->uu.uuid16);
            break;
        case LEN_UUID_128:
            UINT8_TO_BE_STREAM (p, (UUID_DESC_TYPE << 3) | SIZE_SIXTEEN_BYTES);
            ARRAY_TO_BE_STREAM (p, p_uuid->uu.uuid128, LEN_UUID_128);
            SDP_AddAttribute (sdp_handle, ATTR_ID_SERVICE_CLASS_ID_LIST, DATA_ELE_SEQ_DESC_TYPE,
                              (UINT32) (p - buff), buff);
            break;

        default:
            GATT_TRACE_ERROR1("inavlid UUID len=%d", p_uuid->len);
            SDP_DeleteRecord(sdp_handle);
            return 0;
            break;
    }

    /*** Fill out the protocol element sequence for SDP ***/
    proto_elem_list[0].protocol_uuid = UUID_PROTOCOL_L2CAP;
    proto_elem_list[0].num_params    = 1;
    proto_elem_list[0].params[0]     = BT_PSM_ATT;
    proto_elem_list[1].protocol_uuid = UUID_PROTOCOL_ATT;
    proto_elem_list[1].num_params    = 2;
    proto_elem_list[1].params[0]     = start_hdl;
    proto_elem_list[1].params[1]     = end_hdl;

    SDP_AddProtocolList(sdp_handle, 2, proto_elem_list);

    /* Make the service browseable */
    SDP_AddUuidSequence (sdp_handle, ATTR_ID_BROWSE_GROUP_LIST, 1, &list);

    return(sdp_handle);
}


    #if GATT_CONFORMANCE_TESTING == TRUE
/*******************************************************************************
**
** Function         gatt_set_err_rsp
**
** Description      This function is called to set the test confirm value
**
** Returns          void
**
*******************************************************************************/
void gatt_set_err_rsp(BOOLEAN enable, UINT8 req_op_code, UINT8 err_status)
{
    GATT_TRACE_DEBUG3("gatt_set_err_rsp enable=%d op_code=%d, err_status=%d", enable, req_op_code, err_status);
    gatt_cb.enable_err_rsp  = enable;
    gatt_cb.req_op_code     = req_op_code;
    gatt_cb.err_status      = err_status;
}
    #endif



/*******************************************************************************
**
** Function         gatt_get_regcb
**
** Description      The function returns the registration control block.
**
** Returns          pointer to the registration control block or NULL
**
*******************************************************************************/
tGATT_REG *gatt_get_regcb (tGATT_IF gatt_if)
{
    UINT8           ii = (UINT8)gatt_if;
    tGATT_REG       *p_reg = NULL;

    if (ii)
    {
        ii--; /* convert from one based to zero based */
        p_reg = &gatt_cb.cl_rcb[ii];
        if ( (ii < GATT_MAX_APPS)  && (p_reg->in_use) )
            return(p_reg);
    }

    return NULL;
}


/*******************************************************************************
**
** Function         gatt_is_clcb_allocated
**
** Description      The function check clcb for conn_id is allocated or not
**
** Returns           True already allocated
**
*******************************************************************************/

BOOLEAN gatt_is_clcb_allocated (UINT16 conn_id)
{
    UINT8         i = 0;
    BOOLEAN       is_allocated= FALSE;

    for (i = 0; i < GATT_CL_MAX_LCB; i++)
    {
        if (gatt_cb.clcb[i].in_use && (gatt_cb.clcb[i].conn_id == conn_id))
        {
            is_allocated = TRUE;
            break;
        }
    }

    return is_allocated;
}

/*******************************************************************************
**
** Function         gatt_clcb_alloc
**
** Description      The function allocates a GATT  connection link control block
**
** Returns           NULL if not found. Otherwise pointer to the connection link block.
**
*******************************************************************************/
tGATT_CLCB *gatt_clcb_alloc (UINT16 conn_id)
{
    UINT8           i = 0;
    tGATT_CLCB      *p_clcb = NULL;
    tGATT_IF        gatt_if=GATT_GET_GATT_IF(conn_id);
    UINT8           tcb_idx = GATT_GET_TCB_IDX(conn_id);
    tGATT_TCB       *p_tcb = gatt_get_tcb_by_idx(tcb_idx);
    tGATT_REG       *p_reg = gatt_get_regcb(gatt_if);

    for (i = 0; i < GATT_CL_MAX_LCB; i++)
    {
        if (!gatt_cb.clcb[i].in_use)
        {
            p_clcb = &gatt_cb.clcb[i];

            p_clcb->in_use      = TRUE;
            p_clcb->conn_id     = conn_id;
            p_clcb->clcb_idx    = i;
            p_clcb->p_reg       = p_reg;
            p_clcb->p_tcb       = p_tcb;
            break;
        }
    }
    return p_clcb;
}

/*******************************************************************************
**
** Function         gatt_clcb_dealloc
**
** Description      The function de allocates a GATT  connection link control block
**
** Returns         None
**
*******************************************************************************/
void gatt_clcb_dealloc (tGATT_CLCB *p_clcb)
{
    if (p_clcb && p_clcb->in_use)
    {
        memset(p_clcb, 0, sizeof(tGATT_CLCB));
    }
}



/*******************************************************************************
**
** Function         gatt_find_tcb_by_cid
**
** Description      The function searches for an empty entry
**                   in registration info table for GATT client
**
** Returns           NULL if not found. Otherwise pointer to the rcb.
**
*******************************************************************************/
tGATT_TCB * gatt_find_tcb_by_cid (UINT16 lcid)
{
    UINT16       xx = 0;
    tGATT_TCB    *p_tcb = NULL;

    for (xx = 0; xx < GATT_MAX_PHY_CHANNEL; xx++)
    {
        if (gatt_cb.tcb[xx].in_use && gatt_cb.tcb[xx].att_lcid == lcid)
        {
            p_tcb = &gatt_cb.tcb[xx];
            break;
        }
    }
    return p_tcb;
}


/*******************************************************************************
**
** Function         gatt_num_apps_hold_link
**
** Description      The function find the number of applcaitions is holding the link
**
** Returns          total number of applications holding this acl link.
**
*******************************************************************************/
UINT8 gatt_num_apps_hold_link(tGATT_TCB *p_tcb)
{
    UINT8 i, num = 0;

    for (i = 0; i < GATT_MAX_APPS; i ++)
    {
        if (p_tcb->app_hold_link[i])
            num ++;
    }

    GATT_TRACE_DEBUG1("gatt_num_apps_hold_link   num=%d",  num);
    return num;
}


/*******************************************************************************
**
** Function         gatt_num_clcb_by_bd_addr
**
** Description      The function searches all LCB with macthing bd address
**
** Returns          total number of clcb found.
**
*******************************************************************************/
UINT8 gatt_num_clcb_by_bd_addr(BD_ADDR bda)
{
    UINT8 i, num = 0;

    for (i = 0; i < GATT_CL_MAX_LCB; i ++)
    {
        if (gatt_cb.clcb[i].in_use && memcmp(gatt_cb.clcb[i].p_tcb->peer_bda, bda, BD_ADDR_LEN) == 0)
            num ++;
    }
    return num;
}

/*******************************************************************************
**
** Function         gatt_sr_update_cback_cnt
**
** Description      The function searches all LCB with macthing bd address
**
** Returns          total number of clcb found.
**
*******************************************************************************/
void gatt_sr_copy_prep_cnt_to_cback_cnt(tGATT_TCB *p_tcb )
{
    UINT8 i;

    if (p_tcb)
    {
        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            if (p_tcb->prep_cnt[i])
            {
                p_tcb->sr_cmd.cback_cnt[i]=1;
            }
        }
    }

}

/*******************************************************************************
**
** Function         gatt_sr_is_cback_cnt_zero
**
** Description      The function searches all LCB with macthing bd address
**
** Returns          True if thetotal application callback count is zero
**
*******************************************************************************/
BOOLEAN gatt_sr_is_cback_cnt_zero(tGATT_TCB *p_tcb )
{
    BOOLEAN status = TRUE;
    UINT8   i;

    if (p_tcb)
    {
        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            if (p_tcb->sr_cmd.cback_cnt[i])
            {
                status = FALSE;
                break;
            }
        }
    }
    else
    {
        status = FALSE;
    }
    return status;
}

/*******************************************************************************
**
** Function         gatt_sr_is_prep_cnt_zero
**
** Description      Check the prepare write request count is zero or not
**
** Returns          True no prepare write request
**
*******************************************************************************/
BOOLEAN gatt_sr_is_prep_cnt_zero(tGATT_TCB *p_tcb)
{
    BOOLEAN status = TRUE;
    UINT8   i;

    if (p_tcb)
    {
        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            if (p_tcb->prep_cnt[i])
            {
                status = FALSE;
                break;
            }
        }
    }
    else
    {
        status = FALSE;
    }
    return status;
}


/*******************************************************************************
**
** Function         gatt_sr_reset_cback_cnt
**
** Description      Reset the application callback count to zero
**
** Returns         None
**
*******************************************************************************/
void gatt_sr_reset_cback_cnt(tGATT_TCB *p_tcb )
{
    UINT8 i;

    if (p_tcb)
    {
        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            p_tcb->sr_cmd.cback_cnt[i]=0;
        }
    }
}

/*******************************************************************************
**
** Function         gatt_sr_reset_prep_cnt
**
** Description     Reset the prep write count to zero
**
** Returns        None
**
*******************************************************************************/
void gatt_sr_reset_prep_cnt(tGATT_TCB *p_tcb )
{
    UINT8 i;
    if (p_tcb)
    {
        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            p_tcb->prep_cnt[i]=0;
        }
    }
}


/*******************************************************************************
**
** Function         gatt_sr_update_cback_cnt
**
** Description    Update the teh applicaiton callback count
**
** Returns           None
**
*******************************************************************************/
void gatt_sr_update_cback_cnt(tGATT_TCB *p_tcb, tGATT_IF gatt_if, BOOLEAN is_inc, BOOLEAN is_reset_first)
{

    UINT8 idx = ((UINT8) gatt_if) - 1 ;

    if (p_tcb)
    {
        if (is_reset_first)
        {
            gatt_sr_reset_cback_cnt(p_tcb);
        }
        if (is_inc)
        {
            p_tcb->sr_cmd.cback_cnt[idx]++;
        }
        else
        {
            if ( p_tcb->sr_cmd.cback_cnt[idx])
            {
                p_tcb->sr_cmd.cback_cnt[idx]--;
            }
        }
    }
}


/*******************************************************************************
**
** Function         gatt_sr_update_prep_cnt
**
** Description    Update the teh prepare write request count
**
** Returns           None
**
*******************************************************************************/
void gatt_sr_update_prep_cnt(tGATT_TCB *p_tcb, tGATT_IF gatt_if, BOOLEAN is_inc, BOOLEAN is_reset_first)
{
    UINT8 idx = ((UINT8) gatt_if) - 1 ;

    GATT_TRACE_DEBUG4("gatt_sr_update_prep_cnt tcb idx=%d gatt_if=%d is_inc=%d is_reset_first=%d",
                      p_tcb->tcb_idx, gatt_if, is_inc, is_reset_first);

    if (p_tcb)
    {
        if (is_reset_first)
        {
            gatt_sr_reset_prep_cnt(p_tcb);
        }
        if (is_inc)
        {
            p_tcb->prep_cnt[idx]++;
        }
        else
        {
            if (p_tcb->prep_cnt[idx])
            {
                p_tcb->prep_cnt[idx]--;
            }
        }
    }
}
/*******************************************************************************
**
** Function         gatt_cancel_open
**
** Description      Cancel open request
**
** Returns         Boolean
**
*******************************************************************************/
BOOLEAN gatt_cancel_open(tGATT_IF gatt_if, BD_ADDR bda)
{
    tGATT_TCB *p_tcb=NULL;
    BOOLEAN status= TRUE;

    p_tcb = gatt_find_tcb_by_addr(bda);
    if (p_tcb)
    {
        if (gatt_get_ch_state(p_tcb) == GATT_CH_OPEN)
        {
            GATT_TRACE_ERROR0("GATT_CancelConnect - link connected Too late to cancel");
            status = FALSE;
        }
        else
        {
            gatt_update_app_use_link_flag(gatt_if, p_tcb, FALSE, FALSE);
            if (!gatt_num_apps_hold_link(p_tcb))
            {
                gatt_disconnect(p_tcb->peer_bda);
            }
        }
    }

    return status;
}

/*******************************************************************************
**
** Function         gatt_find_app_hold_link
**
** Description      find the applicaiton that is holding the specified link
**
** Returns         Boolean
**
*******************************************************************************/
BOOLEAN gatt_find_app_hold_link(tGATT_TCB *p_tcb, UINT8 start_idx, UINT8 *p_found_idx, tGATT_IF *p_gatt_if)
{
    UINT8 i;
    BOOLEAN found= FALSE;

    for (i = start_idx; i < GATT_MAX_APPS; i ++)
    {
        if (p_tcb->app_hold_link[i])
        {
            *p_gatt_if = gatt_cb.clcb[i].p_reg->gatt_if;
            *p_found_idx = i;
            found = TRUE;
            break;
        }
    }
    return found;
}

/*******************************************************************************
**
** Function         gatt_cmd_enq
**
** Description      Enqueue this command.
**
** Returns          None.
**
*******************************************************************************/
BOOLEAN gatt_cmd_enq(tGATT_TCB *p_tcb, UINT16 clcb_idx, BOOLEAN to_send, UINT8 op_code, BT_HDR *p_buf)
{
    tGATT_CMD_Q  *p_cmd = &p_tcb->cl_cmd_q[p_tcb->next_slot_inq];

    p_cmd->to_send = to_send; /* waiting to be sent */
    p_cmd->op_code  = op_code;
    p_cmd->p_cmd    = p_buf;
    p_cmd->clcb_idx = clcb_idx;

    if (!to_send)
    {
        p_tcb->pending_cl_req = p_tcb->next_slot_inq;
    }

    p_tcb->next_slot_inq ++;
    p_tcb->next_slot_inq %= GATT_CL_MAX_LCB;

    return TRUE;
}

/*******************************************************************************
**
** Function         gatt_cmd_dequeue
**
** Description      dequeue the command in the client CCB command queue.
**
** Returns          total number of clcb found.
**
*******************************************************************************/
tGATT_CLCB * gatt_cmd_dequeue(tGATT_TCB *p_tcb, UINT8 *p_op_code)
{
    tGATT_CMD_Q  *p_cmd = &p_tcb->cl_cmd_q[p_tcb->pending_cl_req];
    tGATT_CLCB *p_clcb = NULL;

    if (p_tcb->pending_cl_req != p_tcb->next_slot_inq)
    {
        p_clcb = &gatt_cb.clcb[p_cmd->clcb_idx];

        *p_op_code = p_cmd->op_code;

        p_tcb->pending_cl_req ++;
        p_tcb->pending_cl_req %= GATT_CL_MAX_LCB;
    }

    return p_clcb;
}

/*******************************************************************************
**
** Function         gatt_send_write_msg
**
** Description      This real function send out the ATT message for write.
**
** Returns          status code
**
*******************************************************************************/
UINT8 gatt_send_write_msg (tGATT_TCB *p_tcb, UINT16 clcb_idx, UINT8 op_code,
                           UINT16 handle, UINT16 len,
                           UINT16 offset, UINT8 *p_data)
{
    tGATT_CL_MSG     msg;

    msg.attr_value.handle = handle;
    msg.attr_value.len = len;
    msg.attr_value.offset = offset;

    memcpy (msg.attr_value.value, p_data, len);

    /* write by handle */
    return attp_send_cl_msg(p_tcb, clcb_idx, op_code, &msg);
}

/*******************************************************************************
**
** Function         gatt_act_send_browse
**
** Description      This function ends a browse command request, including read
**                  information request and read by type request.
**
** Returns          status code
**
*******************************************************************************/
UINT8 gatt_act_send_browse(tGATT_TCB *p_tcb, UINT16 index, UINT8 op, UINT16 s_handle,
                           UINT16 e_handle, tBT_UUID uuid)
{
    tGATT_CL_MSG     msg;

    msg.browse.s_handle = s_handle;
    msg.browse.e_handle   = e_handle;
    memcpy(&msg.browse.uuid, &uuid, sizeof(tBT_UUID));

    /* write by handle */
    return attp_send_cl_msg(p_tcb, index, op, &msg);
}

/*******************************************************************************
**
** Function         gatt_end_operation
**
** Description      This function ends a discovery, send callback and finalize
**                  some control value.
**
** Returns          16 bits uuid.
**
*******************************************************************************/
void gatt_end_operation(tGATT_CLCB *p_clcb, tGATT_STATUS status, void *p_data)
{
    tGATT_CL_COMPLETE   cb_data;
    tGATT_CMPL_CBACK    *p_cmpl_cb = (p_clcb->p_reg) ? p_clcb->p_reg->app_cb.p_cmpl_cb : NULL;
    UINT8               op = p_clcb->operation, disc_type=GATT_DISC_MAX;
    tGATT_DISC_CMPL_CB  *p_disc_cmpl_cb = (p_clcb->p_reg) ? p_clcb->p_reg->app_cb.p_disc_cmpl_cb : NULL;
    UINT16              conn_id;
    UINT8               operation;

    GATT_TRACE_DEBUG3 ("gatt_end_operation status=%d op=%d subtype=%d",
                       status, p_clcb->operation, p_clcb->op_subtype);

    if (p_cmpl_cb != NULL && p_clcb->operation != 0)
    {
        if (p_clcb->operation == GATTC_OPTYPE_READ)
        {
            memset(&cb_data.att_value, 0, sizeof(tGATT_VALUE));
            cb_data.att_value.handle   = p_clcb->s_handle;
            cb_data.att_value.len      = p_clcb->counter;
            if (p_data)
                memcpy (cb_data.att_value.value, p_data, cb_data.att_value.len);
        }

        if (p_clcb->operation == GATTC_OPTYPE_WRITE)
        {
            memset(&cb_data.att_value, 0, sizeof(tGATT_VALUE));
            cb_data.handle           =
            cb_data.att_value.handle = p_clcb->s_handle;
            if (p_clcb->op_subtype == GATT_WRITE_PREPARE)
            {
                if (p_data)
                {
                    cb_data.att_value = *((tGATT_VALUE *) p_data);
                }
                else
                {
                    GATT_TRACE_DEBUG0("Rcv Prepare write rsp but no data");
                }
            }
        }

        if (p_clcb->operation == GATTC_OPTYPE_CONFIG)
            cb_data.mtu = p_clcb->p_tcb->payload_size;

        if (p_clcb->operation == GATTC_OPTYPE_DISCOVERY)
        {
            disc_type = p_clcb->op_subtype;
        }
    }

    if (p_clcb->p_attr_buf)
    {
        GKI_freebuf(p_clcb->p_attr_buf);
    }

    operation =  p_clcb->operation;
    conn_id = p_clcb->conn_id;

    gatt_clcb_dealloc(p_clcb);

    if (p_disc_cmpl_cb && (op == GATTC_OPTYPE_DISCOVERY))
        (*p_disc_cmpl_cb)(conn_id, disc_type, status);
    else if (p_cmpl_cb && op)
        (*p_cmpl_cb)(conn_id, op, status, &cb_data);
    else
        GATT_TRACE_WARNING3 ("gatt_end_operation not sent out op=%d p_disc_cmpl_cb:%p p_cmpl_cb:%p",
                             operation, p_disc_cmpl_cb, p_cmpl_cb);
}

/*******************************************************************************
**
** Function         gatt_cleanup_upon_disc
**
** Description      This function cleans up the control blocks when L2CAP channel
**                  disconnect.
**
** Returns          16 bits uuid.
**
*******************************************************************************/
void gatt_cleanup_upon_disc(BD_ADDR bda, UINT16 reason)
{
    tGATT_TCB       *p_tcb = NULL;
    tGATT_CLCB      *p_clcb;
    UINT8           i;
    UINT16          conn_id;
    tGATT_REG        *p_reg=NULL;


    GATT_TRACE_DEBUG0 ("gatt_cleanup_upon_disc ");

    if ((p_tcb = gatt_find_tcb_by_addr(bda)) != NULL)
    {
        GATT_TRACE_DEBUG0 ("found p_tcb ");
        for (i = 0; i < GATT_CL_MAX_LCB; i ++)
        {
            p_clcb = &gatt_cb.clcb[i];
            if (p_clcb->in_use && p_clcb->p_tcb == p_tcb)
            {
                GATT_TRACE_DEBUG2 ("found p_clcb conn_id=%d clcb_idx=%d", p_clcb->conn_id, p_clcb->clcb_idx);
                if (p_clcb->operation != GATTC_OPTYPE_NONE)
                    gatt_end_operation(p_clcb, GATT_ERROR, NULL);

                gatt_clcb_dealloc(p_clcb);

            }
        }

        btu_stop_timer (&p_tcb->rsp_timer_ent);
        btu_stop_timer (&p_tcb->ind_ack_timer_ent);
        btu_stop_timer (&p_tcb->conf_timer_ent);
        gatt_free_pending_ind(p_tcb);

        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            p_reg = &gatt_cb.cl_rcb[i];
            if (p_reg->in_use && p_reg->app_cb.p_conn_cb)
            {
                conn_id = GATT_CREATE_CONN_ID(p_tcb->tcb_idx, p_reg->gatt_if);
                GATT_TRACE_DEBUG3 ("found p_reg tcb_idx=%d gatt_if=%d  conn_id=0x%x", p_tcb->tcb_idx, p_reg->gatt_if, conn_id);
                (*p_reg->app_cb.p_conn_cb)(p_reg->gatt_if,  bda, conn_id, FALSE, reason);
            }
        }
        memset(p_tcb, 0, sizeof(tGATT_TCB));

    }
    GATT_TRACE_DEBUG0 ("exit gatt_cleanup_upon_disc ");
}
/*******************************************************************************
**
** Function         gatt_dbg_req_op_name
**
** Description      Get op code description name, for debug information.
**
** Returns          UINT8 *: name of the operation.
**
*******************************************************************************/
UINT8 * gatt_dbg_op_name(UINT8 op_code)
{
    UINT8 pseduo_op_code_idx = op_code & (~GATT_WRITE_CMD_MASK);

    if (op_code == GATT_CMD_WRITE )
    {
        pseduo_op_code_idx = 0x14; /* just an index to op_code_name */

    }

    if (op_code == GATT_SIGN_CMD_WRITE)
    {
        pseduo_op_code_idx = 0x15; /* just an index to op_code_name */
    }

    if (pseduo_op_code_idx <= GATT_OP_CODE_MAX)
        return(UINT8*) op_code_name[pseduo_op_code_idx];
    else
        return(UINT8 *)"Op Code Exceed Max";
}

/*******************************************************************************
**
** Function         gatt_dbg_display_uuid
**
** Description      Disaplay the UUID
**
** Returns          None
**
*******************************************************************************/
void gatt_dbg_display_uuid(tBT_UUID bt_uuid)
{
    char str_buf[50];
    int x = 0;

    if (bt_uuid.len == LEN_UUID_16)
    {
        sprintf(str_buf, "0x%04x", bt_uuid.uu.uuid16);
    }
    else if (bt_uuid.len == LEN_UUID_128)
    {
        x += sprintf(&str_buf[x], "0x%02x%02x%02x%02x%02x%02x%02x%02x",
                     bt_uuid.uu.uuid128[15], bt_uuid.uu.uuid128[14],
                     bt_uuid.uu.uuid128[13], bt_uuid.uu.uuid128[12],
                     bt_uuid.uu.uuid128[11], bt_uuid.uu.uuid128[10],
                     bt_uuid.uu.uuid128[9], bt_uuid.uu.uuid128[8]);
        sprintf(&str_buf[x], "%02x%02x%02x%02x%02x%02x%02x%02x",
                bt_uuid.uu.uuid128[7], bt_uuid.uu.uuid128[6],
                bt_uuid.uu.uuid128[5], bt_uuid.uu.uuid128[4],
                bt_uuid.uu.uuid128[3], bt_uuid.uu.uuid128[2],
                bt_uuid.uu.uuid128[1], bt_uuid.uu.uuid128[0]);
    }
    else
        BCM_STRNCPY_S(str_buf, sizeof(str_buf), "Unknown UUID 0", 15);

    GATT_TRACE_DEBUG1 ("UUID=[%s]", str_buf);

}


/*******************************************************************************
**
** Function         gatt_is_bg_dev_for_app
**
** Description      find is this one of the background devices for the application
**
** Returns          TRUE this is one of the background devices for the  application
**
*******************************************************************************/
BOOLEAN gatt_is_bg_dev_for_app(tGATT_BG_CONN_DEV *p_dev, tGATT_IF gatt_if)
{
    UINT8   i;

    for (i = 0; i < GATT_MAX_APPS; i ++ )
    {
        if (p_dev->in_use && (p_dev->gatt_if[i] == gatt_if))
        {
            return TRUE;
        }
    }
    return FALSE;
}
/*******************************************************************************
**
** Function         gatt_find_bg_dev
**
** Description      find background connection device from the list.
**
** Returns          pointer to the device record
**
*******************************************************************************/
tGATT_BG_CONN_DEV * gatt_find_bg_dev(BD_ADDR remote_bda)
{
    tGATT_BG_CONN_DEV    *p_dev_list = &gatt_cb.bgconn_dev[0];
    UINT8   i;

    for (i = 0; i < GATT_MAX_BG_CONN_DEV; i ++, p_dev_list ++)
    {
        if (p_dev_list->in_use && !memcmp(p_dev_list->remote_bda, remote_bda, BD_ADDR_LEN))
        {
            return p_dev_list;
        }
    }
    return NULL;
}
/*******************************************************************************
**
** Function         gatt_alloc_bg_dev
**
** Description      allocate a background connection device record
**
** Returns          pointer to the device record
**
*******************************************************************************/
tGATT_BG_CONN_DEV * gatt_alloc_bg_dev(BD_ADDR remote_bda)
{
    tGATT_BG_CONN_DEV    *p_dev_list = &gatt_cb.bgconn_dev[0];
    UINT8   i;

    for (i = 0; i < GATT_MAX_BG_CONN_DEV; i ++, p_dev_list ++)
    {
        if (!p_dev_list->in_use)
        {
            p_dev_list->in_use = TRUE;
            memcpy(p_dev_list->remote_bda, remote_bda, BD_ADDR_LEN);

            return p_dev_list;
        }
    }
    return NULL;
}

/*******************************************************************************
**
** Function         gatt_add_bg_dev_list
**
** Description      add/remove device from the back ground connection device list
**
** Returns          pointer to the device record
**
*******************************************************************************/
BOOLEAN gatt_add_bg_dev_list(tGATT_IF gatt_if, BD_ADDR bd_addr)
{
    tGATT_BG_CONN_DEV   *p_dev = NULL;
    UINT8   i;

    if ((p_dev = gatt_find_bg_dev(bd_addr)) == NULL)
    {
        p_dev = gatt_alloc_bg_dev(bd_addr);
    }

    if (p_dev)
    {
        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            if (p_dev->gatt_if[i] == gatt_if)
            {
                GATT_TRACE_ERROR0("device already in list");
                return FALSE;
            }
            else if (p_dev->gatt_if[i] == 0)
            {
                GATT_TRACE_DEBUG0("add device into list");
                p_dev->gatt_if[i] = gatt_if;
                return TRUE;
            }
        }
    }

    GATT_TRACE_ERROR0("no device record available");

    return FALSE;
}


/*******************************************************************************
**
** Function         gatt_remove_bg_dev_for_app
**
** Description      Remove the application interface for the specified background device
**
** Returns          Boolean
**
*******************************************************************************/
BOOLEAN gatt_remove_bg_dev_for_app(tGATT_IF gatt_if, BD_ADDR bd_addr)
{
    tGATT_TCB    *p_tcb = gatt_find_tcb_by_addr(bd_addr);
    BOOLEAN       status;

    if (p_tcb)
        gatt_update_app_use_link_flag(gatt_if, p_tcb, FALSE, FALSE);
    status = gatt_update_auto_connect_dev(gatt_if, FALSE, bd_addr);
    return status;
}


/*******************************************************************************
**
** Function         gatt_get_num_apps_for_bg_dev
**
** Description      Gte the number of applciations for the specified background device
**
** Returns          UINT8 total number fo applications
**
*******************************************************************************/
UINT8 gatt_get_num_apps_for_bg_dev(BD_ADDR bd_addr)
{
    tGATT_BG_CONN_DEV   *p_dev = NULL;
    UINT8   i;
    UINT8   cnt = 0;

    if ((p_dev = gatt_find_bg_dev(bd_addr)) != NULL)
    {
        for (i = 0; i < GATT_MAX_APPS; i ++)
        {
            if (p_dev->gatt_if[i])
                cnt++;
        }
    }
    return cnt;
}

/*******************************************************************************
**
** Function         gatt_find_app_for_bg_dev
**
** Description      find the application interface for the specified background device
**
** Returns          Boolean
**
*******************************************************************************/
BOOLEAN gatt_find_app_for_bg_dev(BD_ADDR bd_addr, tGATT_IF *p_gatt_if)
{
    tGATT_BG_CONN_DEV   *p_dev = NULL;
    UINT8   i;
    BOOLEAN ret = FALSE;

    if ((p_dev = gatt_find_bg_dev(bd_addr)) == NULL)
    {
        return ret;
    }

    for (i = 0; i < GATT_MAX_APPS; i ++)
    {
        if (p_dev->gatt_if[i] != 0 )
        {
            *p_gatt_if = p_dev->gatt_if[i];
            ret = TRUE;
            break;
        }
    }
    return ret;
}


/*******************************************************************************
**
** Function         gatt_remove_bg_dev_from_list
**
** Description      add/remove device from the back ground connection device list
**
** Returns          pointer to the device record
**
*******************************************************************************/
BOOLEAN gatt_remove_bg_dev_from_list(tGATT_IF gatt_if, BD_ADDR bd_addr)
{
    tGATT_BG_CONN_DEV   *p_dev = NULL;
    UINT8   i, j;
    BOOLEAN ret = FALSE;

    if ((p_dev = gatt_find_bg_dev(bd_addr)) == NULL)
    {
        return ret;
    }

    for (i = 0; i < GATT_MAX_APPS && p_dev->gatt_if[i] > 0; i ++)
    {
        if (p_dev->gatt_if[i] == gatt_if)
        {
            p_dev->gatt_if[i] = 0;

            for (j = i + 1; j < GATT_MAX_APPS; j ++)
                p_dev->gatt_if[j - 1] = p_dev->gatt_if[j];

            if (p_dev->gatt_if[0] == 0)
            {
                ret = BTM_BleUpdateBgConnDev(FALSE, p_dev->remote_bda);
                memset(p_dev, 0, sizeof(tGATT_BG_CONN_DEV));
            }
            else
                ret = TRUE;

            break;
        }
    }

    return ret;
}
/*******************************************************************************
**
** Function         gatt_deregister_bgdev_list
**
** Description      deregister all related back ground connetion device.
**
** Returns          pointer to the device record
**
*******************************************************************************/
void gatt_deregister_bgdev_list(tGATT_IF gatt_if)
{
    tGATT_BG_CONN_DEV    *p_dev_list = &gatt_cb.bgconn_dev[0];
    UINT8 i , j, k;

    for (i = 0 ; i <GATT_MAX_BG_CONN_DEV; i ++, p_dev_list ++ )
    {
        if (p_dev_list->in_use)
        {
            for (j = 0; j < GATT_MAX_APPS; j ++)
            {
                if (p_dev_list->gatt_if[j] == 0)
                    break;
                else if (p_dev_list->gatt_if[j] == gatt_if)
                {
                    for (k = j + 1; k < GATT_MAX_APPS; k ++)
                        p_dev_list->gatt_if[k - 1] = p_dev_list->gatt_if[k];

                    if (p_dev_list->gatt_if[0] == 0)
                    {
                        BTM_BleUpdateBgConnDev(FALSE, p_dev_list->remote_bda);
                        memset(p_dev_list, 0, sizeof(tGATT_BG_CONN_DEV));
                        break;
                    }
                }
            }
        }
    }
}


/*******************************************************************************
**
** Function         gatt_reset_bgdev_list
**
** Description      reset bg device list
**
** Returns          pointer to the device record
**
*******************************************************************************/
void gatt_reset_bgdev_list(void)
{
    memset(&gatt_cb.bgconn_dev, 0 , sizeof(tGATT_BG_CONN_DEV)*GATT_MAX_BG_CONN_DEV);

}
/*******************************************************************************
**
** Function         gatt_update_auto_connect_dev
**
** Description      This function add or remove a device for background connection
**                  procedure.
**
** Parameters       gatt_if: Application ID.
**                  add: add peer device
**                  bd_addr: peer device address.
**
** Returns          TRUE if connection started; FALSE if connection start failure.
**
*******************************************************************************/
BOOLEAN gatt_update_auto_connect_dev (tGATT_IF gatt_if, BOOLEAN add, BD_ADDR bd_addr)
{
    BOOLEAN         ret = FALSE, exist_dev = FALSE;
    tGATT_REG        *p_reg;
    tGATT_TCB       *p_tcb = gatt_find_tcb_by_addr(bd_addr);

    GATT_TRACE_API0 ("gatt_update_auto_connect_dev ");
    /* Make sure app is registered */
    if ((p_reg = gatt_get_regcb(gatt_if)) == NULL)
    {
        GATT_TRACE_ERROR1("gatt_update_auto_connect_dev - gatt_if is not registered", gatt_if);
        return(FALSE);
    }

    if (add)
    {
        /* new device */
        if (gatt_find_bg_dev(bd_addr))
            exist_dev = TRUE;

        if (gatt_add_bg_dev_list(gatt_if, bd_addr))
        {
            if (!exist_dev)
            {
                ret = BTM_BleUpdateBgConnDev(TRUE, bd_addr);
            }
            else
                ret = TRUE;

            /* if a connected device, update the link holding number */
            if (p_tcb != NULL)
                gatt_update_app_use_link_flag(gatt_if, p_tcb, TRUE, TRUE);
        }
    }
    else
    {
        ret = gatt_remove_bg_dev_from_list(gatt_if, bd_addr);
    }
    return ret;
}



/*******************************************************************************
**
** Function         gatt_get_conn_id
**
** Description      This function returns a connecttion handle to a ATT server
**                  if the server is already connected
**
** Parameters       gatt_if: client interface.
**                  bd_addr: peer device address.
**
** Returns          Connection handle or invalid handle value
**
*******************************************************************************/
UINT16 gatt_get_conn_id (tGATT_IF gatt_if, BD_ADDR bd_addr)
{
    tGATT_REG       *p_reg;
    tGATT_CLCB      *p_clcb;
    tGATT_TCB       *p_tcb;
    UINT8           i;

    GATT_TRACE_API1 ("GATTC_GetConnIfConnected gatt_if=%d", gatt_if);
    /* Do we have a transport to the peer ? If not, we are not connected */
    if ((p_tcb = gatt_find_tcb_by_addr(bd_addr)) == NULL)
    {
        GATT_TRACE_EVENT0 ("GATTC_GetConnIfConnected - no TCB found");
        return(GATT_INVALID_CONN_ID);
    }

    /* Make sure app is registered */
    if ((p_reg = gatt_get_regcb(gatt_if)) == NULL)
    {
        GATT_TRACE_ERROR1("GATTC_GetConnIfConnected - gatt_if is not registered", gatt_if);
        return(GATT_INVALID_CONN_ID);
    }

    /* Now see if the app already has a client control block to that peer */
    for (i = 0, p_clcb = gatt_cb.clcb; i < GATT_CL_MAX_LCB; i++, p_clcb++)
    {
        if ( p_clcb->in_use && (p_clcb->p_reg == p_reg) && (p_clcb->p_tcb == p_tcb) )
        {
            return(p_clcb->conn_id);
        }
    }

    /* If here, failed to allocate a client control block */
    GATT_TRACE_ERROR1 ("gatt_get_conn_id: not connected- gatt_if: %u", gatt_if);
    return(GATT_INVALID_CONN_ID);
}




#endif


