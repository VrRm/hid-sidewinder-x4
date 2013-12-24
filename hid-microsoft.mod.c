#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xf951821a, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x94c28798, __VMLINUX_SYMBOL_STR(hid_unregister_driver) },
	{ 0xa98ea8da, __VMLINUX_SYMBOL_STR(__hid_register_driver) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x11bf278c, __VMLINUX_SYMBOL_STR(input_event) },
	{ 0xaa57df64, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0x185d4dc8, __VMLINUX_SYMBOL_STR(dev_get_drvdata) },
	{ 0x5e35eeb2, __VMLINUX_SYMBOL_STR(hid_connect) },
	{ 0xf63b399a, __VMLINUX_SYMBOL_STR(hid_open_report) },
	{ 0xee23dab4, __VMLINUX_SYMBOL_STR(usb_alloc_coherent) },
	{ 0x558dd12c, __VMLINUX_SYMBOL_STR(usb_alloc_urb) },
	{ 0x57438dd1, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x88c450e0, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x41b61fc8, __VMLINUX_SYMBOL_STR(dev_set_drvdata) },
	{ 0x56d7b552, __VMLINUX_SYMBOL_STR(dev_warn) },
	{ 0x579d80dd, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0xf97456ea, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x42288d84, __VMLINUX_SYMBOL_STR(usb_submit_urb) },
	{ 0x21fb443e, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=hid,usbcore";

MODULE_ALIAS("hid:b0003g*v0000045Ep0000003B");
MODULE_ALIAS("hid:b0003g*v0000045Ep00000768");
MODULE_ALIAS("hid:b0003g*v0000045Ep000000DB");
MODULE_ALIAS("hid:b0003g*v0000045Ep000000DC");
MODULE_ALIAS("hid:b0003g*v0000045Ep000000F9");
MODULE_ALIAS("hid:b0003g*v0000045Ep00000713");
MODULE_ALIAS("hid:b0003g*v0000045Ep00000730");
MODULE_ALIAS("hid:b0003g*v0000045Ep0000009D");
MODULE_ALIAS("hid:b0003g*v0000045Ep0000076C");
MODULE_ALIAS("hid:b0005g*v0000045Ep00000701");
