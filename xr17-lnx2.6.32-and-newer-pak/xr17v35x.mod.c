#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x6e1cca31, "module_layout" },
	{ 0x986f2452, "pci_unregister_driver" },
	{ 0x528beb48, "uart_unregister_driver" },
	{ 0xfb1ae01f, "__pci_register_driver" },
	{ 0x50c3d14f, "uart_register_driver" },
	{ 0xedc03953, "iounmap" },
	{ 0x27aaf715, "uart_remove_one_port" },
	{ 0x19a663da, "pci_disable_device" },
	{ 0xdcb764ad, "memset" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x7ce94494, "pci_enable_device" },
	{ 0xf1954572, "uart_add_one_port" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0xc3a6546b, "uart_match_port" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x54a1a844, "uart_write_wakeup" },
	{ 0x7647726c, "handle_sysrq" },
	{ 0x572a9ee4, "uart_try_toggle_sysrq" },
	{ 0x4a17ed66, "sysrq_mask" },
	{ 0x2c9875a0, "tty_flip_buffer_push" },
	{ 0xf5d9ccd2, "uart_insert_char" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x528d180b, "uart_update_timeout" },
	{ 0x555b725d, "uart_get_baud_rate" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0x65708d15, "kmem_cache_alloc" },
	{ 0xbe5c6425, "kmalloc_caches" },
	{ 0xc1514a3b, "free_irq" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0xf9a482f9, "msleep" },
	{ 0xe09783c2, "uart_handle_cts_change" },
	{ 0x1db5e160, "uart_handle_dcd_change" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0x37a0cba, "kfree" },
	{ 0x4b0a3f52, "gic_nonsecure_priorities" },
	{ 0x9c1e5bf5, "queued_spin_lock_slowpath" },
	{ 0x69f38847, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x6b4b2933, "__ioremap" },
	{ 0xaf56600a, "arm64_use_ng_mappings" },
};

MODULE_INFO(depends, "");

