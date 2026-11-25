#include "keyref.h"
#include <string.h>

#define KEY_REF_MAX (256U)

typedef struct {
    uint8_t  cnt;                   /* 活动键码数量 */
    uint8_t  ref[KEY_REF_MAX];      /* 每个键码的引用计数 */
    uint8_t  act[KEY_REF_MAX];      /* 活动键码表，cnt 为有效长度 */
    uint32_t edge;                   /* 0 <-> 1 边沿跳变次数 */
} __attribute__((aligned(4))) keyref_t;

static keyref_t keyref = { 0 };

bool keyref_clr(void)
{
    if (keyref.cnt == 0U) {
        return false;
    }

    keyref.cnt = 0U;
    memset(keyref.ref, 0, sizeof(keyref.ref));
    keyref.edge++;

    return true;
}

bool keyref_inc(uint8_t code)
{
    bool ret = false;
    keyref_t *p_ref = &keyref;

    if (p_ref->ref[code] == 0) {
        // if (p_ref->cnt < KEY_REF_MAX) {
            p_ref->act[p_ref->cnt] = code;
            p_ref->cnt++;
            p_ref->edge++;
            ret = true;
        // }
    }

    if (p_ref->ref[code] < 0xFF) {
        p_ref->ref[code]++;
    }
    return ret;
}

bool keyref_dec(uint8_t code)
{
    bool ret = false;
    keyref_t *p_ref = &keyref;

    if (p_ref->ref[code] == 0) {
        return ret;
    }

    p_ref->ref[code]--;
    if (p_ref->ref[code] != 0) {
        return ret;
    }

    for (uint8_t i = 0; i < p_ref->cnt; i++) {
        if (p_ref->act[i] == code) {
            p_ref->cnt--;
            p_ref->act[i] = p_ref->act[p_ref->cnt];
            p_ref->act[p_ref->cnt] = 0;
            p_ref->edge++;
            ret = true;
            break;
        }
    }

    return ret;
}

uint8_t keyref_get_cnt(void)
{
    return keyref.cnt;
}

const uint8_t *keyref_get_list(void)
{
    return keyref.act;
}

uint32_t keyref_get_edge(void)
{
    return keyref.edge;
}
