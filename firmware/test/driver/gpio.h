#ifndef GPIO_STUB_H
#define GPIO_STUB_H

typedef int gpio_num_t;
#define GPIO_NUM_5 5
typedef enum { GPIO_PULLUP_ONLY, GPIO_PULLDOWN_ONLY, GPIO_PULLUP_PULLDOWN, GPIO_FLOATING } gpio_pull_mode_t;
inline int gpio_set_pull_mode(gpio_num_t, gpio_pull_mode_t) { return 0; }

#endif
