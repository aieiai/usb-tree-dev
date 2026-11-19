/**
 * @Author: wj
 * @Brief:  lamp array matrix.
 * @Date:   2024/10/02
 */
#include "lamp_array.h"
#include "usbd_core.h"
#include "usbd_hid.h"
#include "usb_log.h"
#include "use_usb.h"

static const lamp_pos_t lamp_pos[LAMP_NUM] = {
    {303000, 12300, 0},     {274000, 12000, 0},     {255000, 12000, 0},     {236000, 12000, 0},     {217000, 12000, 0}, 
    {188000, 12000, 0},     {169000, 12000, 0},     {150000, 12000, 0},     {131000, 12000, 0},     {102000, 12000, 0},    
    {83000, 12000, 0},      {67000, 12000, 0},      {45000, 12000, 0},      {7000, 12000, 0},       {7600, 36000, 0},    
    {26600, 36000, 0},      {45700, 36000, 0},      {64700, 36000, 0},      {83000, 36000, 0},      {102000, 36000, 0},     
    {122000, 36000, 0},     {140000, 36000, 0},     {160000, 36000, 0},     {179000, 36000, 0},     {198000, 36000, 0},     
    {217000, 36000, 0},     {236000, 36000, 0},     {264000, 36000, 0},     {303000, 36000, 0},     {303000, 55500, 0},
    {269000, 55500, 0},     {245000, 55500, 0},     {226000, 55500, 0},     {207000, 55500, 0},     {188000, 55500, 0}, 
    {169000, 55500, 0},     {150000, 55500, 0},     {131000, 55500, 0},     {112000, 55500, 0},     {93000, 55500, 0},
    {74000, 55500, 0},      {55000, 55500, 0},      {36000, 55000, 0},      {12000, 55500, 0},      {14000, 74500, 0},
    {41000, 74500, 0},      {60000, 74500, 0},      {79000, 74500, 0},      {98000, 74500, 0},      {117000, 74500, 0},
    {136000, 74500, 0},     {155000, 74500, 0},     {174000, 74500, 0},     {193000, 74500, 0},     {212000, 74500, 0},
    {231000, 74500, 0},     {262000, 74500, 0},     {303000, 74500, 0},     {303000, 93600, 0},     {279000, 98600, 0},
    {248000, 93600, 0},     {221000, 93600, 0},     {202000, 93600, 0},     {183000, 93600, 0},     {164000, 93600, 0},
    {145000, 93600, 0},     {248000, 93600, 0},     {221000, 93600, 0},     {248000, 93600, 0},     {221000, 93600, 0},      
    {145000, 93600, 0},     {248000, 93600, 0},     
};  

static lamp_array_matrix_t lamp_array = {
    .cur_id = 0,
    .cfg = {
        .kind     = LAMP_ARRAY_KIND_KEYBOARD,
        .width    = LIGHTMAP_WIDTH,
        .height   = LIGHTMAP_HEIGHT,
        .depth    = LIGHTMAP_DEPTH,
        .pos      = (lamp_pos_t *)lamp_pos,
        .num      = LAMP_NUM,
        .bind_key = 0x39,   // Keyboard Caps Lock
        .update_interval = LIGHTMAP_UPDATE_INTERVAL,
        .handle = NULL,
    },
};

/* get reporrt */
lamp_array_attribute_report_t lamp_array_attribute_rpt;
lamp_attribute_response_report_t lamp_attribute_response_rpt;

/* set reporrt */
lamp_attribute_request_report_t lamp_attribute_request_rpt;
lamp_multi_update_report_t lamp_multi_update_rpt;
lamp_range_update_report_t lamp_range_update_rpt;
lamp_array_control_report_t lamp_array_control_rpt;


/**
 * @brief:  
 * @param:  None
 * @return: None
 */
static uint16_t get_lamp_array_attribute_report(uint8_t* buf)
{
    uint16_t len = sizeof(lamp_array_attribute_rpt);
    lamp_array_attribute_rpt.rpt_id = LAMP_ARRAY_RPT_ID_ATTRIBUTE;
    lamp_array_attribute_rpt.count  = lamp_array.cfg.num;
    lamp_array_attribute_rpt.width  = lamp_array.cfg.width;
    lamp_array_attribute_rpt.height = lamp_array.cfg.height;
    lamp_array_attribute_rpt.depth  = lamp_array.cfg.depth;
    lamp_array_attribute_rpt.kind   = lamp_array.cfg.kind;
    lamp_array_attribute_rpt.min_update_interval = lamp_array.cfg.update_interval;
    if (buf != NULL)
    {
        memcpy(buf, &lamp_array_attribute_rpt, len);
    }
    return len;
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
static uint16_t get_lamp_attribute_report(uint8_t* buf)
{
    uint16_t len = sizeof(lamp_attribute_response_rpt);
    lamp_attribute_response_rpt.rpt_id = LAMP_ARRAY_RPT_ID_ATTRIBUTE_RESPONSE;
    lamp_attribute_response_rpt.attr.id = lamp_array.cur_id;
    lamp_attribute_response_rpt.attr.pos = lamp_array.cfg.pos[lamp_array.cur_id];
    lamp_attribute_response_rpt.attr.update_latency = lamp_array.cfg.update_interval;
    lamp_attribute_response_rpt.attr.purpose = LAMP_PURPOSE_CONTROL;
    lamp_attribute_response_rpt.attr.clolor.r = 255;
    lamp_attribute_response_rpt.attr.clolor.g = 255;
    lamp_attribute_response_rpt.attr.clolor.b = 255;
    lamp_attribute_response_rpt.attr.clolor.i = 1;
    lamp_attribute_response_rpt.attr.is_programmable = 1;
    lamp_attribute_response_rpt.attr.bind.key = lamp_array.cfg.bind_key;
    if (buf != NULL)
    {
        memcpy(buf, &lamp_attribute_response_rpt, len);
    }

    lamp_array.cur_id = (lamp_array.cur_id + 1) % lamp_array.cfg.num;
    return len;
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
static void set_lamp_attribute_id(const uint8_t* buf)
{
    memcpy(&lamp_attribute_request_rpt, buf, sizeof(lamp_attribute_request_report_t));
    lamp_array.cur_id = (lamp_attribute_request_rpt.id < LAMP_NUM) ? lamp_attribute_request_rpt.id : 0;
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
static void set_lamp_multiple(const uint8_t* buf)
{
    memcpy(&lamp_multi_update_rpt, buf, sizeof(lamp_multi_update_report_t));
    for (uint32_t i = 0; i < lamp_multi_update_rpt.count; i++)
    {
        // ignore update if not within bounds.
        if (lamp_multi_update_rpt.id[i] < LAMP_NUM)
        {
            /* TODO: user set led color */
            // hal_lamp_set_color(i,   lamp_multi_update_rpt.update_color[i].r, \
                                    lamp_multi_update_rpt.update_color[i].g, \
                                    lamp_multi_update_rpt.update_color[i].b);
        }
    }
    // Don't want the consumer to update before the Host has completed the batch of updates.
    if (lamp_multi_update_rpt.update_flag & LAMP_UPDATE_FLG_COMPLETE)
    {
        // hal_lamp_refresh();
    }
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
static void set_lamp_range(const uint8_t* buf)
{
    memcpy(&lamp_range_update_rpt, buf, sizeof(lamp_range_update_report_t));
    
    // ignore update if not within bounds.
    if (lamp_range_update_rpt.id_start < LAMP_NUM && lamp_range_update_rpt.id_end < LAMP_NUM &&
        lamp_range_update_rpt.id_start <= lamp_range_update_rpt.id_end)
    {
        for (uint32_t i = lamp_range_update_rpt.id_start; i < lamp_range_update_rpt.id_end; i++)
        {
            // hal_lamp_set_color(i,   lamp_range_update_rpt.update_color.r, \
                                    lamp_range_update_rpt.update_color.g, \
                                    lamp_range_update_rpt.update_color.b);
        }
        
        // Don't want the consumer to update before the Host has completed the batch of updates.
        if (lamp_range_update_rpt.update_flag & LAMP_UPDATE_FLG_COMPLETE)
        {
            // hal_lamp_refresh();
        }
    }
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
static void set_lamp_autonomous_mode(const uint8_t* buf)
{
    memcpy(&lamp_array_control_rpt, buf, sizeof(lamp_array_control_report_t));
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
uint32_t lamp_array_get_report(uint8_t report_id, uint8_t *buf)
{
    uint32_t len = 0;
    switch (report_id)
    {
        case LAMP_ARRAY_RPT_ID_ATTRIBUTE:
            len = get_lamp_array_attribute_report(buf);
//            usbd_ep_start_write(0, USBD_EPI_HID_LAMP, (uint8_t *)&lamp_array_attribute_rpt, sizeof(lamp_array_attribute_rpt));
            break;
        case LAMP_ARRAY_RPT_ID_ATTRIBUTE_RESPONSE:
            len = get_lamp_attribute_report(buf);
//            usbd_ep_start_write(0, USBD_EPI_HID_LAMP, (uint8_t *)&lamp_attribute_response_rpt, sizeof(lamp_attribute_response_rpt));
            break;
    }
    return len;
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
void lamp_array_set_report(uint8_t report_id, uint8_t *report, uint32_t report_len)
{
    switch (report_id)
    {
        case LAMP_ARRAY_RPT_ID_ATTRIBUTE_REQUEST: 
            if (report_len >= sizeof(lamp_attribute_request_report_t))
            {
                set_lamp_attribute_id(report);
            }
            break;
        case LAMP_ARRAY_RPT_ID_MULTI_UPDATE:
            if (report_len >= sizeof(lamp_multi_update_report_t))
            {
                set_lamp_multiple(report);
            }
            break;
        case LAMP_ARRAY_RPT_ID_RANGE_UPDATE:
            if (report_len >= sizeof(lamp_range_update_report_t))
            {
                set_lamp_range(report);
            }
            break;
        case LAMP_ARRAY_RPT_ID_CONTROL:
            if (report_len >= sizeof(lamp_array_control_report_t))
            {
                set_lamp_autonomous_mode(report);
            }
            break;
    }
}

/**
 * @brief:  
 * @param:  None
 * @return: None
 */
void lamp_array_init(void)
{
}

