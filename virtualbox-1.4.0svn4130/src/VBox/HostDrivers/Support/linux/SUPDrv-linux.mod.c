/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Linux host:
 * Linux host kernel module interfaces
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SUPDRV.h" /* for KBUILD_STR */
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = __stringify(KBUILD_MODNAME),
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};

static const struct modversion_info ____versions[]
__attribute_used__
__attribute__((section("__versions"))) = {
	{        0, "cleanup_module" },
	{        0, "init_module" },
	{        0, "struct_module" },
	{        0, "devfs_remove" },
	{        0, "strpbrk" },
	{        0, "__kmalloc" },
	{        0, "mem_map" },
	{        0, "vmalloc" },
	{        0, "malloc_sizes" },
	{        0, "vfree" },
	{        0, "change_page_attr" },
	{        0, "__might_sleep" },
	{        0, "remap_page_range" },
	{        0, "__alloc_pages" },
	{        0, "printk" },
	{        0, "__PAGE_KERNEL" },
	{        0, "rwsem_wake" },
	{        0, "copy_to_user" },
	{        0, "devfs_mk_cdev" },
	{        0, "preempt_schedule" },
	{        0, "contig_page_data" },
	{        0, "do_mmap_pgoff" },
	{        0, "find_vma" },
	{        0, "kmem_cache_alloc" },
	{        0, "__free_pages" },
	{        0, "do_munmap" },
	{        0, "get_user_pages" },
	{        0, "register_chrdev" },
	{        0, "vsnprintf" },
	{        0, "kfree" },
	{        0, "memcpy" },
	{        0, "unregister_chrdev" },
	{        0, "put_page" },
	{        0, "__up_wakeup" },
	{        0, "__down_failed" },
	{        0, "copy_from_user" },
	{        0, "rwsem_down_read_failed" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";

