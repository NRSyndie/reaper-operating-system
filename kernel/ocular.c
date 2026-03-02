#include "include/ocular.h"
#include "include/limine.h"
#include "include/pmm.h"
#include "include/slab.h"
#include "include/utils.h"
#include "include/console.h"

extern struct limine_framebuffer_request framebuffer_request;

static uint32_t* fb_ptr = NULL;
static uint64_t  fb_size = 0;
static uint32_t  fb_width = 0;
static uint32_t  fb_height = 0;
static uint32_t  fb_pitch = 0;

static slab_cache_t* projection_cache = NULL;
static projection_t* projection_list = NULL;
static spinlock_t ocular_lock = 0;

void ocular_init(void) {
    if (!framebuffer_request.response || framebuffer_request.response->framebuffer_count == 0) {
        kprintf("[OCULAR] No framebuffer found. Gaze is blind.\n");
        return;
    }

    struct limine_framebuffer* fb = framebuffer_request.response->framebuffers[0];
    fb_ptr = (uint32_t*)fb->address;
    fb_width = fb->width;
    fb_height = fb->height;
    fb_pitch = fb->pitch / 4; // Pitch in pixels
    fb_size = fb->pitch * fb->height;

    projection_cache = slab_create_cache("OcularCache", sizeof(projection_t), 8);
    
    kprintf("[OCULAR] Framebuffer: %dx%d at %p (Pitch: %d)\n", fb_width, fb_height, fb_ptr, fb_pitch);
}

void ocular_bleach(void) {
    if (!fb_ptr) return;
    uint64_t flags = spinlock_irqsave(&ocular_lock);
    hyper_scrub(fb_ptr, fb_size / 8);
    spinlock_irqrestore(&ocular_lock, flags);
}

bool ocular_is_ready(void) {
    return fb_ptr != NULL && projection_cache != NULL;
}

int ocular_set_projection(lattice_t* lattice, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t reality_mask) {
    uint64_t flags = spinlock_irqsave(&ocular_lock);

    // Update existing if it matches the lattice
    projection_t* curr = projection_list;
    while (curr) {
        if (curr->lattice == lattice) {
            curr->x = x; curr->y = y;
            curr->w = w; curr->h = h;
            curr->reality_mask = reality_mask;
            curr->active = true;
            spinlock_irqrestore(&ocular_lock, flags);
            return 0;
        }
        curr = curr->next;
    }

    // Otherwise create new
    projection_t* p = (projection_t*)slab_alloc(projection_cache);
    if (!p) {
        spinlock_irqrestore(&ocular_lock, flags);
        return -1;
    }

    p->lattice = lattice;
    p->x = x; p->y = y;
    p->w = w; p->h = h;
    p->reality_mask = reality_mask;
    p->active = true;
    p->next = projection_list;
    projection_list = p;

    spinlock_irqrestore(&ocular_lock, flags);
    return 0;
}

void ocular_project(void) {
    if (!fb_ptr || !projection_list) return;

    // Use a non-blocking lock for the idle path to avoid jitter
    if (__sync_lock_test_and_set(&ocular_lock, 1)) return;

    uint8_t current_mode_mask = mode_get_current_mask();
    projection_t* p = projection_list;

    while (p) {
        if (p->active && (p->reality_mask & current_mode_mask)) {
            // Blit Lattice to Framebuffer
            // Optimization: Only blit if p->lattice->pending_bits is marked?
            // For now, we blit everything in the gaze.
            
            uint32_t* src = (uint32_t*)pmm_phys_to_virt(p->lattice->frames[0]); // Simple 1-page window
            uint32_t start_y = p->y;
            uint32_t start_x = p->x;
            
            for (uint32_t row = 0; row < p->h && (start_y + row) < fb_height; row++) {
                uint32_t* dest_row = &fb_ptr[(start_y + row) * fb_pitch + start_x];
                uint32_t* src_row = &src[row * p->w];
                
                // Use hardware-accelerated move if width is sufficient
                uint32_t pixels_to_copy = p->w;
                if (start_x + pixels_to_copy > fb_width) pixels_to_copy = fb_width - start_x;
                
                for (uint32_t px = 0; px < pixels_to_copy; px++) {
                    dest_row[px] = src_row[px];
                }
            }
        }
        p = p->next;
    }

    __sync_lock_release(&ocular_lock);
}
