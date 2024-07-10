/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024, Unikraft GmbH and The Unikraft Authors.
 * Licensed under the BSD-3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 */

#include <uk/essentials.h>
#include <uk/arch/types.h>
#include <uk/list.h>
#include <uk/console.h>
#include <uk/console/driver.h>
#include <errno.h>

#if CONFIG_LIBUKDEBUG_PRINTK
#include <uk/print.h>
#endif /* CONFIG_LIBUKDEBUG_PRINTK */

/* List of dynamically registered devices */
static UK_LIST_HEAD(uk_console_device_list);
static __u16 uk_console_device_count;

static int uk_console_set_standard_once;

struct uk_console *uk_console_get(__u16 id)
{
	struct uk_console *dev = __NULL;

	uk_list_for_each_entry(dev, &uk_console_device_list, _list) {
		if (dev->id == id)
			return dev;
	}

	return __NULL;
}

__u16 uk_console_count(void)
{
	return uk_console_device_count;
}

__ssz uk_console_out(const char *buf, __sz len)
{
	struct uk_console *dev = __NULL;

	if (unlikely(!len))
		return 0;

	if (unlikely(!buf))
		return -EINVAL;

	/* Output to all STDOUT devices */
	uk_list_for_each_entry(dev, &uk_console_device_list, _list) {
		if ((dev->flags & UK_CONSOLE_FLAG_STDOUT) && dev->ops->out)
			uk_console_out_direct(dev, buf, len);
	}

	return len;
}

__ssz uk_console_in(char *buf, __sz len)
{
	struct uk_console *dev = __NULL;
	__sz leftover = len;
	__ssz rc = 0;

	if (unlikely(!len))
		return 0;

	if (unlikely(!buf))
		return -EINVAL;

	/* Collect input from all STDIN devices and append it */
	uk_list_for_each_entry(dev, &uk_console_device_list, _list) {
		if ((dev->flags & UK_CONSOLE_FLAG_STDIN) && dev->ops->in) {
			rc = uk_console_in_direct(dev, buf, leftover);
			if (rc >= 0 && (__sz)rc <= leftover) {
				leftover -= rc;
				buf += rc;
			}
			if (leftover == 0)
				break;
		}
	}

	return len - leftover;
}

__ssz uk_console_out_direct(struct uk_console *dev, const char *buf, __sz len)
{
	if (unlikely(!len))
		return 0;

	if (unlikely(!buf || !dev || !dev->ops || !dev->ops->out))
		return -EINVAL;

	return dev->ops->out(dev, buf, len);
}

__ssz uk_console_in_direct(struct uk_console *dev, char *buf, __sz len)
{
	if (unlikely(!len))
		return 0;

	if (unlikely(!buf || !dev || !dev->ops || !dev->ops->in))
		return -EINVAL;


	return dev->ops->in(dev, buf, len);
}

void uk_console_register(struct uk_console *dev)
{
	struct uk_console *known_dev = __NULL;

	UK_ASSERT(dev);
	UK_ASSERT(dev->ops);

#if CONFIG_LIBUKDEBUG_ENABLE_ASSERT
	uk_list_for_each_entry(known_dev, &uk_console_device_list, _list)
		UK_ASSERT(dev != known_dev);
#endif /* CONFIG_LIBUKDEBUG_ENABLE_ASSERT */

	/* We want to make sure that one of the registered devices has the
	 * STDOUT and STDIN flags set. `uk_console_set_standard_once` is used
	 * to track that. If a device already has these flags set is
	 * registered, we're happy
	 */
	if (dev->flags)
		uk_console_set_standard_once = 1;

	/* Otherwise, if the current device doesn't have any flags set and
	 * there has not yet been another device with any flags set, we give
	 * the current device flags. Now we have at least one device with flags
	 */
	if (!uk_console_set_standard_once &&
	    !(dev->flags & (UK_CONSOLE_FLAG_STDOUT | UK_CONSOLE_FLAG_STDIN))) {
		uk_console_set_standard_once = 1;
		dev->flags |= UK_CONSOLE_FLAG_STDOUT | UK_CONSOLE_FLAG_STDIN;
	}

	uk_list_add_tail(&dev->_list, &uk_console_device_list);
	dev->id = uk_console_device_count++;

#if CONFIG_LIBUKDEBUG_PRINTK
	uk_pr_info("Registered console %"__PRIu16": %p (%s), flags: %c%c\n",
		   dev->id, dev, dev->name,
		   (dev->flags & UK_CONSOLE_FLAG_STDIN) ? 'I' : '-',
		   (dev->flags & UK_CONSOLE_FLAG_STDOUT) ? 'O' : '-');
#endif /* CONFIG_LIBUKDEBUG_PRINTK */
}
