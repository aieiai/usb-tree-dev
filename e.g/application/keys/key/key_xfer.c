/**
 * @Author: wj
 * @Brief:  key .
 * @Date:   2024/07/02
 */
#include "key_xfer.h"
#include "hal.h"
#include "board.h"





/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline void kxfer_rx_full_cb(void *user)
{
//    phy_spi_t *msg_spi = (phy_spi_t *)user;
}
/**
 * @brief:
 * @param:  None
 * @return: None
 */
static __inline void kxfer_tx_full_cb(void *user)
{
//    phy_spi_t *msg_spi = (phy_spi_t *)user;
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kxfer_init(void)
{
 //   hal_register_spi_rx_full_cb(kxfer_rx_full_cb);
  //  hal_register_spi_tx_full_cb(kxfer_tx_full_cb);
}

/**
 * @brief:
 * @param:  None
 * @return: None
 */
void kxfer_deinit(void)
{
  //  hal_register_spi_rx_full_cb(NULL);
  //  hal_register_spi_tx_full_cb(NULL);
}
