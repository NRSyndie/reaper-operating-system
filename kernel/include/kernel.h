#ifndef KERNEL_H
#define KERNEL_H

#include <stdbool.h>

struct kernel_features_t {
    bool pcid_enabled;
    bool invpcid_available;
};

extern struct kernel_features_t kernel_features;

#endif /* KERNEL_H */
