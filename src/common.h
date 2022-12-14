/*
 * License: Dual MIT/GPL
 * Copyright (c) 2020 Microchip Corporation
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

/* Make debugging of user interface possible
 * by faking an ok return when running on a PC */
#if defined(__i386__) || defined(__x86_64__)
#define RETURN_IF_PC return 0
#else
#define RETURN_IF_PC
#endif

/* COUNT_OF() is more type safe than traditional ARRAY_SIZE() */
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

int mchp_genl_start(const char *family_name, uint8_t cmd,
		       uint8_t version, struct nl_sock **skp,
		       struct nl_msg **msgp);

#endif /* _COMMON_H_ */
