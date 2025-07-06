#ifndef HARPOON_CONFIG_H
#define HARPOON_CONFIG_H

#include "harpoon.h"

// Save harpoon slots to separate config file (~/.config/cofi_harpoon.json)
void save_harpoon_slots(const HarpoonManager *harpoon);

// Load harpoon slots from separate config file (~/.config/cofi_harpoon.json)
void load_harpoon_slots(HarpoonManager *harpoon);

#endif /* HARPOON_CONFIG_H */
