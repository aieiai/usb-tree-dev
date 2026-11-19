/**
 * @Author: wj
 * @Brief:  lamp array matrix.
 * @Date:   2024/10/02
 */

#ifndef _LAMP_ARRAY_H_
#define _LAMP_ARRAY_H_
#include "utils.h"
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LAMP_NUM                        ( HAL_LAMP_NUM )
/* unit: um */
#define LIGHTMAP_WIDTH                  ( 310000 )
#define LIGHTMAP_HEIGHT                 ( 120000 )
#define LIGHTMAP_DEPTH                  ( 25000 )
/* unit: us */
#define LIGHTMAP_UPDATE_INTERVAL        ( 50*1000 )

#define LAMP_MULTI_UPDATE_LAMP_MAX      ( 0x08 )

typedef void (*lamp_cb_func_t)(uint16_t *buf, uint8_t len, void *user);

typedef enum lamp_array_report_id {
    LAMP_ARRAY_RPT_ID_ATTRIBUTE = 0x01,
    LAMP_ARRAY_RPT_ID_ATTRIBUTE_REQUEST,
    LAMP_ARRAY_RPT_ID_ATTRIBUTE_RESPONSE,
    LAMP_ARRAY_RPT_ID_MULTI_UPDATE,
    LAMP_ARRAY_RPT_ID_RANGE_UPDATE,
    LAMP_ARRAY_RPT_ID_CONTROL,
} lamp_array_rpt_id_t;

typedef enum lamp_array_kind {
    LAMP_ARRAY_KIND_UNDEFINED       = 0x00,
    LAMP_ARRAY_KIND_KEYBOARD        = 0x01,
    LAMP_ARRAY_KIND_MOUSE           = 0x02,
    LAMP_ARRAY_KIND_GAME_CONTROLLER = 0x03,
    LAMP_ARRAY_KIND_PERIPHERAL      = 0x04,
    LAMP_ARRAY_KIND_SCENE           = 0x05,
    LAMP_ARRAY_KIND_NOTIFICATION    = 0x06,
    LAMP_ARRAY_KIND_CHASSIS         = 0x07,
    LAMP_ARRAY_KIND_WEARABLE        = 0x08,
    LAMP_ARRAY_KIND_FURNITURE       = 0x09,
    LAMP_ARRAY_KIND_ART             = 0x0A,
} lamp_array_kind_t;

typedef enum lamp_purpose {
    LAMP_PURPOSE_CONTROL      = 0x01 << 0,
    LAMP_PURPOSE_ACCENT       = 0x01 << 1,
    LAMP_PURPOSE_BRANDING     = 0x01 << 2,
    LAMP_PURPOSE_STATUS       = 0x01 << 3,
    LAMP_PURPOSE_ILLUMINATION = 0x01 << 4,
    LAMP_PURPOSE_PRESENTATION = 0x01 << 5,
} lamp_purpose_t;

typedef enum lamp_update_flag {
    LAMP_UPDATE_FLG_COMPLETE = 0x01,
} lamp_update_flg_t;

typedef struct lamp_color {
    uint8_t r; /* Red */
    uint8_t g; /* Green */
    uint8_t b; /* Blue */
    uint8_t i; /* Intensity */
} __attribute__ ((packed)) lamp_color_t;

typedef struct lamp_position {
    uint32_t x; /* X position in micrometers */
    uint32_t y; /* Y position in micrometers */
    uint32_t z; /* Z position in micrometers */
} __attribute__ ((packed)) lamp_pos_t;

typedef struct LampAttributes {
    uint16_t id;                /* The ID of the lamp for which these attributes apply */
    lamp_pos_t pos;             /* Positions in micrometers, must be in the bounding box */
    uint32_t update_latency;    /* Each LED can have different update latency. */
    uint32_t purpose;           /* lamp_purpose_t - The purpose of this LED, as per enum LampPurposes */
    lamp_color_t clolor;        /* The color of this LED */
    uint8_t is_programmable;    /* Can change color */
    union {
        uint8_t input;
        uint8_t key;    /* Keybord key that is mapped to this LED. If not a keyboard, must be 0. */
    } __attribute__ ((__packed__)) bind;
} __attribute__ ((__packed__)) lamp_attribute_t;

/* LAMP_ARRAY_RPT_ID_ATTRIBUTE - From device to PC (when requested). Descibes the configuration of the LED strip. */
typedef struct LampArrayAttributesReport {
    lamp_array_rpt_id_t rpt_id;
    uint16_t count;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t kind;  /* lamp_array_kind_t */
    uint32_t min_update_interval;
} __attribute__ ((__packed__)) lamp_array_attribute_report_t;

/* LAMP_ARRAY_RPT_ID_ATTRIBUTE_REQUEST - From PC to device. The PC sets the iterator to this ID. (Is usually followed by a request for the selected LED). */
typedef struct LampAttributesRequestReport {
    lamp_array_rpt_id_t rpt_id;
    uint16_t id;
} __attribute__ ((__packed__)) lamp_attribute_request_report_t;

/* LAMP_ARRAY_RPT_ID_ATTRIBUTE_RESPONSE - From device to PC (when requested). Describes the configuration of the selected LED. */
typedef struct LampAttributesResponseReport {
    lamp_array_rpt_id_t rpt_id;
    lamp_attribute_t attr;
} __attribute__ ((__packed__)) lamp_attribute_response_report_t;

/* LAMP_ARRAY_RPT_ID_MULTI_UPDATE - From PC to device. Sets arbitrary LEDs to some colors. */
typedef struct LampMultiUpdateReport {
    lamp_array_rpt_id_t rpt_id;
    uint8_t count;
    lamp_update_flg_t update_flag;  /* If the strip should be updated now, or wait for the rest. */
    uint16_t id[LAMP_MULTI_UPDATE_LAMP_MAX];
    lamp_color_t update_color[LAMP_MULTI_UPDATE_LAMP_MAX];
} __attribute__ ((packed)) lamp_multi_update_report_t;

/* LAMP_ARRAY_RPT_ID_RANGE_UPDATE - From PC to device - sets fixed color to a consecutive range of LEDs. */
typedef struct LampRangeUpdateReport {
    lamp_array_rpt_id_t rpt_id;
    lamp_update_flg_t update_flag;  /* If the strip should be updated now, or wait for the rest. */
    uint16_t id_start;
    uint16_t id_end;
    lamp_color_t update_color;
} __attribute__ ((packed)) lamp_range_update_report_t;

/* LAMP_ARRAY_RPT_ID_CONTROL - From PC to device - specifies if the PC is in charge of the strip, or the device can do whatever it wants. */
typedef struct LampArrayControlReport {
    lamp_array_rpt_id_t rpt_id;
    uint8_t autonomous_mode;
} __attribute__ ((packed)) lamp_array_control_report_t;

typedef struct lamp_array_matrix_config {
    lamp_array_kind_t kind;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    lamp_pos_t *pos;
    uint32_t num;
    uint32_t bind_key;
    uint32_t update_interval;
    lamp_cb_func_t handle;
} __attribute__ ((aligned(4))) lamp_array_matrix_cfg_t;

typedef struct lamp_array_matrix {
    uint16_t cur_id;
    lamp_array_matrix_cfg_t cfg;
} __attribute__ ((aligned(4))) lamp_array_matrix_t;



void lamp_array_init(void);
uint32_t lamp_array_get_report(uint8_t report_id, uint8_t *buf);
void lamp_array_set_report(uint8_t report_id, uint8_t *report, uint32_t report_len);


#ifdef __cplusplus
}
#endif



#endif /* _LAMP_ARRAY_H_ */

