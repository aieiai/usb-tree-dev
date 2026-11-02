#include "ymodem.h"



#define PACKET_HEADER       (3)     /* start, block, block-complement */
#define PACKET_TRAILER      (2)     /* CRC bytes */
#define PACKET_OVERHEAD     (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZ          (128)
#define PACKET_1K_SIZ       (1024)
// file
#define FILE_NAME_LEN       (256)
#define FILE_SIZE_LEN       (16)



#define NAK_TIMEOUT         (0x100000)


// YMODEM ASCII control codes
enum {
    SOH = 0x01      /* start of 128-byte data packet */  
    STX = 0x02      /* start of 1024-byte data packet */  
    EOT = 0x04      /* end of transmission */  
    ACK = 0x06      /* receive OK */  
    NAK = 0x15      /* receiver error; retry */  
    CA  = 0x18      /* two of these in succession aborts transfer */  
    C   = 0x43      /* character 'C' */  
};

enum {
    YMODEM_STAGE_NONE = 0x00,
    YMODEM_STAGE_ESTABLISHING,  /* set when C is send */
    YMODEM_STAGE_ESTABLISHED,   /* set when we've got the packet 0 and sent ACK and second C */
    YMODEM_STAGE_TRANSMITTING,  /* set when the sender respond to our second C and recviever got a real * data packet. */
    YMODEM_STAGE_FINISHING,     /* set when the sender send a EOT */
    YMODEM_STAGE_FINISHED,      /* set when transmission is really finished, i.e., after the NAK, C, final * NULL packet stuff. */
};

typedef struct {
    uint8_t fn[FILE_NAME_LEN];
    uint32_t file_size;
    uint8_t *tx_data;
    uint8_t stage;

    int (*putchar)(uint8_t ch);
    int (*getchar)(void);
    ymodem_xfer_cb_t begin_cb;
    ymodem_xfer_cb_t data_cb;
    ymodem_xfer_cb_t end_cb;

} ymodem_t;

