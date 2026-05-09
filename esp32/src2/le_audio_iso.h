#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ISO_MAX_SDU_SIZE 120
#define ISO_QUEUE_DEPTH 16

typedef struct {
  uint16_t conn_handle;
  uint8_t cig_id;
  uint8_t cis_id;
  uint16_t max_sdu_size;
} le_audio_iso_config_t;

typedef void (*le_audio_iso_rx_cb_t)(uint16_t conn_handle, const uint8_t *data,
                                     uint16_t length);
typedef void (*le_audio_iso_tx_done_cb_t)(uint16_t conn_handle, uint8_t status);

void le_audio_iso_init(void);
int le_audio_iso_setup_data_path(const le_audio_iso_config_t *cfg);
int le_audio_iso_remove_data_path(uint16_t conn_handle, uint8_t dir);
int le_audio_iso_send(uint16_t conn_handle, const uint8_t *data,
                      uint16_t length);
void le_audio_iso_rx_handler(uint16_t conn_handle, const uint8_t *data,
                             uint16_t length);
bool le_audio_iso_receive(uint16_t conn_handle, uint8_t *out_buf,
                          uint16_t *out_len, uint32_t timeout_ticks);
void le_audio_iso_register_rx_cb(le_audio_iso_rx_cb_t cb);
void le_audio_iso_register_tx_done_cb(le_audio_iso_tx_done_cb_t cb);
void le_audio_iso_start_tasks(void);
int le_audio_iso_is_ready(uint16_t conn_handle);
void le_audio_iso_print_stats(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif
