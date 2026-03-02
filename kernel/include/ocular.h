#ifndef REAPER_OCULAR_H
#define REAPER_OCULAR_H

#include <stdint.h>
#include <stddef.h>
#include "ipc.h"
#include "mode.h"

/**
 * projection_t (Ocular Rune)
 * Defines a window into a specific Reality.
 */
typedef struct projection {
    lattice_t* lattice;     /* The source of light (Shared Memory) */
    uint32_t   x, y;        /* Screen coordinates */
    uint32_t   w, h;        /* Dimensions */
    uint32_t   z_order;     /* Stacking depth */
    uint8_t    reality_mask; /* Which modes can see this? */
    bool       active;
    struct projection* next;
} projection_t;

/**
 * @brief Initialize the Ocular Projection Engine.
 */
void ocular_init(void);

/**
 * @brief Add or update a projection mapping.
 */
int ocular_set_projection(lattice_t* lattice, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t reality_mask);

/**
 * @brief The Projector: Blits active Lattices to the Framebuffer.
 * To be called from the idle path or a sidecar core.
 */
void ocular_project(void);

/**
 * @brief Clear the screen (The Great Bleach).
 */
void ocular_bleach(void);

/**
 * @brief Returns true when Ocular has a usable framebuffer and cache state.
 */
bool ocular_is_ready(void);

#endif
