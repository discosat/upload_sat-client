#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct segments_ctx_t segments_ctx_t;

extern segments_ctx_t *init_segments_ctx(void);

/**
 * Maintain a (linked) list of received segments
 * @param ctx self-explanatory
 * @param received the data packet id (it's in the first uint32_t in the payload)
 * @return true if all good, false if we could not allocate a segment, in which case it's a good idea to cancel the transaction
 */
extern bool update_segments(segments_ctx_t *ctx, uint32_t received);

/**
 * Call this at the end of a transfer to close the currently open segment if any
 * @param ctx self-explanatory
 */
extern void close_segments(segments_ctx_t *ctx);

/**
 * Compute the complement segments (received segments -> missing segments)
 * @param ctx self-explanatory
 * @return segments context containing the complement segments
 * @note Remember to free_segments() the returned context !!
 */
extern segments_ctx_t *get_complement_segment(segments_ctx_t *ctx);

/**
 * Get the number of received segments
 * @param ctx self-explanatory
 * @return number of received segments
 */
extern uint32_t get_nof_segments(segments_ctx_t *ctx);

/**
 * Dumps current segments to stdout
 * @param ctx self-explanatory
 */
extern void print_segments(segments_ctx_t *ctx);

/**
 * Release memory allocated for segments
 * @param ctx self-explanatory
 */
extern void free_segments(segments_ctx_t *ctx);

/**
 * Call the given cb function for each segment, with the segment start and end packet
 */
extern void for_each_segment(segments_ctx_t *ctx, void (*cb)(uint32_t idx, uint32_t start, uint32_t end, void *cb_ctx), void *cb_ctx);

/**
 * Manually add a segment
 * @param ctx self-explanatory
 * @param start of the segment range in bytes
 * @param end of the segment range in bytes
 */
extern void add_segment(segments_ctx_t *ctx, uint32_t start, uint32_t end);

#ifdef __cplusplus
}
#endif