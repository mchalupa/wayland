#ifndef __TC_CONFIG_H__
#define __TC_CONFIG_H__

#include <stdint.h>

/* this structure is passed to display_create(). According bits in this
 * structure are object created or not
 *
 * example:
 *
 * struct config conf = {0};
 * conf.globals |= CONF_SEAT;
 * conf.resources |= CONF_SEAT | CONF_POINTER | CONF_TOUCH
 *
 * display_create(&conf);
 *
 * This code will result in creating global seat and
 * consequently, when bind is called, resources for seat, pointer and touch.
 * But not even when bind for keyboard will be called, resource will *not* be created.
 *
 * When NULL to display_create() is passed, then default configuration is
 * used.
 * Default configuration is defined in tc-server.c
 */
struct config {
	uint32_t globals;	/* bitmap of globals */
	uint32_t resources;	/* bitmap of resources */
	uint32_t options;	/* versatile bitmap */
};

enum {
	CONF_SEAT 	= 1,
	CONF_POINTER 	= 1 << 1,
	CONF_KEYBOARD 	= 1 << 2,
	CONF_TOUCH 	= 1 << 3,
	CONF_COMPOSITOR = 1 << 4,
	CONF_SURFACE	= 1 << 5,
	CONF_SHM	= 1 << 6,
	/* FREE */

	CONF_ALL 	= ~((uint32_t) 0)
};

/* This is not the same as default config. This configuration says that
 * no objects nor resources should be created */
const struct config zero_config;

#endif /* __TC_CONFIG_H__ */
