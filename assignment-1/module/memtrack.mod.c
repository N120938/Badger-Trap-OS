#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x94a217da, "module_layout" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xa4903754, "kmalloc_caches" },
	{ 0xd2b09ce5, "__kmalloc" },
	{ 0x406f469a, "rsvd_fault_hook" },
	{ 0xead54b84, "device_destroy" },
	{ 0x6ef2f545, "__register_chrdev" },
	{ 0x91715312, "sprintf" },
	{ 0xcfc2dc8, "sysfs_remove_group" },
	{ 0xc2d04def, "current_task" },
	{ 0x7c32d0f0, "printk" },
	{ 0x20c55ae0, "sscanf" },
	{ 0x19c297b3, "sysfs_create_group" },
	{ 0xd70eb3ab, "device_create" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0xe63f0760, "module_put" },
	{ 0x4e3ae67c, "find_vma" },
	{ 0xad6a4db9, "page_fault_pid" },
	{ 0xdb7305a1, "__stack_chk_fail" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x48444fc0, "kmem_cache_alloc_trace" },
	{ 0x40537ce6, "kernel_kobj" },
	{ 0x37a0cba, "kfree" },
	{ 0x8f94cbee, "pv_mmu_ops" },
	{ 0xb5b62085, "class_destroy" },
	{ 0x7032d921, "__class_create" },
	{ 0x524727dd, "try_module_get" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

