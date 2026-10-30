// SPDX-License-Identifier: GPL-2.0-only
/* One-boot opt-in downstream Gunyah watchdog bite on panic. */
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/panic_notifier.h>
#include <linux/reboot.h>
#include <linux/suspend.h>

static bool armed;
module_param(armed, bool, 0444);
static bool selfcheck;
module_param(selfcheck, bool, 0444);
static bool early_bite;
module_param(early_bite, bool, 0444);
static unsigned int suspend_timeout_ms;
module_param(suspend_timeout_ms, uint, 0444);
static bool panicked;
static bool suspend_wdt_active;

static int panic_restart(struct notifier_block *nb, unsigned long event, void *ptr);

/* ayn_8550 gh_virt_wdt.c: CONTROL=5, STATUS=6, PET=7, SET_TIME=8. */
static long wdt_call(unsigned int function, unsigned long a1,
		     unsigned long a2)
{
	struct arm_smccc_res result;

	arm_smccc_smc(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,
				       ARM_SMCCC_OWNER_VENDOR_HYP, function),
		      a1, a2, 0, 0, 0, 0, 0, &result);
	return (long)result.a0;
}

static int panic_seen(struct notifier_block *nb, unsigned long event, void *ptr)
{
	WRITE_ONCE(panicked, true);
	if (early_bite)
		return panic_restart(nb, event, ptr);
	return NOTIFY_DONE;
}

static int panic_restart(struct notifier_block *nb, unsigned long event, void *ptr)
{
	long ret;

	if (!armed || !READ_ONCE(panicked))
		return NOTIFY_DONE;
	/* Establish the normal-runtime validated setup before shortening bite.
	 * Bit 1 in CONTROL must be set, as documented by downstream.
	 */
	ret = wdt_call(8, 60000, 60000);
	if (!ret && !suspend_wdt_active)
		ret = wdt_call(5, 3, 0);
	if (!ret)
		ret = wdt_call(7, 0, 0);
	if (!ret)
		ret = wdt_call(8, 0xffff, 1);
	if (!ret)
		ret = wdt_call(7, 0, 0);
	if (ret)
		return NOTIFY_DONE;
	mdelay(10000);
	pr_emerg("thorch-dump-watchdog: bite did not transfer control; preventing cold reset\n");
	return NOTIFY_STOP;
}

static struct notifier_block panic_nb = {
	.notifier_call = panic_seen,
	.priority = INT_MAX - 1,
};
static struct notifier_block restart_nb = {
	.notifier_call = panic_restart,
	.priority = 255,
};

static int suspend_probe(struct notifier_block *nb, unsigned long event, void *ptr)
{
	long ret;

	if (!armed || !suspend_timeout_ms)
		return NOTIFY_DONE;
	if (event == PM_SUSPEND_PREPARE) {
		ret = wdt_call(8, suspend_timeout_ms, suspend_timeout_ms);
		if (!ret)
			ret = wdt_call(5, 3, 0);
		if (!ret)
			ret = wdt_call(7, 0, 0);
		if (ret) {
			wdt_call(5, 2, 0);
			pr_err("thorch-dump-watchdog: suspend arm failed: %ld\n", ret);
			return NOTIFY_BAD;
		}
		suspend_wdt_active = true;
		pr_info("thorch-dump-watchdog: suspend timeout armed %u ms\n",
			suspend_timeout_ms);
	} else if (event == PM_POST_SUSPEND && suspend_wdt_active) {
		ret = wdt_call(5, 2, 0);
		if (!ret)
			suspend_wdt_active = false;
		pr_info("thorch-dump-watchdog: resume watchdog disable=%ld\n", ret);
	}
	return NOTIFY_DONE;
}

static struct notifier_block suspend_nb = {
	.notifier_call = suspend_probe,
};

static int __init dump_watchdog_init(void)
{
	struct arm_smccc_res result;
	int ret;

	if (suspend_timeout_ms &&
	    (suspend_timeout_ms < 1000 || suspend_timeout_ms > 60000))
		return -EINVAL;
	if (arm_smccc_1_1_get_conduit() != SMCCC_CONDUIT_SMC)
		return -ENODEV;
	arm_smccc_smc(ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32,
				       ARM_SMCCC_OWNER_VENDOR_HYP, 6),
		      0, 0, 0, 0, 0, 0, 0, &result);
	if (result.a0 || (result.a1 & 1))
		return -EBUSY;
	if (selfcheck) {
		long setup, enable = -1, pet = -1, disable;

		if (armed)
			return -EINVAL;
		/* Long timeout; never wait for expiry. Always attempt disable. */
		setup = wdt_call(8, 60000, 60000);
		if (!setup) {
			enable = wdt_call(5, 3, 0);
			if (!enable)
				pet = wdt_call(7, 0, 0);
		}
		disable = wdt_call(5, 2, 0);
		pr_info("thorch-dump-watchdog: selfcheck setup=%ld enable=%ld pet=%ld disable=%ld\n",
			setup, enable, pet, disable);
		/* No registration or persistent watchdog use in selfcheck mode. */
		return -ECANCELED;
	}
	ret = atomic_notifier_chain_register(&panic_notifier_list, &panic_nb);
	if (ret)
		return ret;
	ret = register_restart_handler(&restart_nb);
	if (ret) {
		atomic_notifier_chain_unregister(&panic_notifier_list, &panic_nb);
		return ret;
	}
	ret = register_pm_notifier(&suspend_nb);
	if (ret) {
		unregister_restart_handler(&restart_nb);
		atomic_notifier_chain_unregister(&panic_notifier_list, &panic_nb);
		return ret;
	}
	pr_info("thorch-dump-watchdog: registered armed=%d; no watchdog settings changed\n",
		armed);
	return 0;
}

static void __exit dump_watchdog_exit(void)
{
	unregister_pm_notifier(&suspend_nb);
	if (suspend_wdt_active)
		wdt_call(5, 2, 0);
	unregister_restart_handler(&restart_nb);
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_nb);
}
module_init(dump_watchdog_init);
module_exit(dump_watchdog_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Opt-in Thor panic virtual watchdog dump diagnostic");
