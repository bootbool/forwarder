#include <linux/kernel.h>
#include <linux/module.h>
#include "procfs.h" 
#include "netfilter.h" 
#include "forward.h"


static int __init mod_init(void)
{
    forward_init();
    proc_init();
    nf_init();
    printk(KERN_INFO "Forwarder initialized^_^\n");
    return 0;
}

static void __exit mod_exit(void)
{   
    nf_exit();
    proc_exit();
    forward_exit();
    printk(KERN_INFO "Forwarder exited\n");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Forwarder, hooked in netfilter, created by foolfoot");
