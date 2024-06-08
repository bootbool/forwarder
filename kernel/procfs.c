#include <linux/proc_fs.h>
#include <linux/uaccess.h> 
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/inet.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>

#include "log.h"
#include "common.h"
#include "forward.h"



static ssize_t read_port(struct file *file, char __user *user_buffer, size_t count, loff_t *ppos)
{
    ssize_t ret;
    int len;
    char data[32] = {0};
    int port = get_port_n();
    port = ntohs(port);
    if(file->private_data == 0 ) {
        file->private_data = (void *)1;
    }else{
        file->private_data = 0;
        return 0;
    }
    len = sprintf(data, "%d", port);
    ret = copy_to_user(user_buffer, data, len);
    if(ret) return -EFAULT;
    *ppos = len;
    return len;
}

static ssize_t write_port(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
    ssize_t ret;
    char data[32] = {0};
    int port;
    int i = 0;
    ret = copy_from_user(data, user_buffer, 32);
    if (ret != 0) {
        return -EFAULT;
    }
    while(  isdigit(data[i]) ) {
        i++;
    }
    data[i] = 0;
    if( kstrtoint( data, 10, &port) != 0 ){
        logs("Failt to parse port %d", data);
        return -EFAULT;
    }
    logs("Writing port: %d", port);
    set_port_h(port);
    *ppos = count;
    return count;
}


static int parse_u32_pair( char *_str, u32 *value)
{
    int i = 0;
    int index;
    #define _LEN  32
    char str[_LEN];
    if(strlen(str) == 0) return 0;
    strncpy(str, _str, _LEN);
    // first fd
    while( isdigit(str[i]) && (i<_LEN) ){
        i++;
    }
    if( str[i] == ' ' ){
        str[i] = 0;
        i++;
    }else{
        logs("Wrong fd pair format: %s", str);
        return 1;
    }
    index = i;
    // second fd
    while( isdigit(str[i]) && (i<_LEN) ){
        i++;
    }
    if( !isgraph(str[i]) ){
        str[i] = 0;
    }else{
        logs("Wrong fd pair format: %s", str);
        return 1;
    }
    value[0] = value[1] = 0;
    if( kstrtou32( str, 10, value) == 1 ){
        logs("Failt to parse clientfd %s", str);
        return 2;
    }
    if( kstrtou32( str+index, 10, value+1) == 1 ){
        logs("Failt to parse serverfd %s", str+index);
        return 3;
    }
    return 0;
}


static ssize_t write_forward(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
    ssize_t ret;
    char data[64] = {0};
    u32 value[2] = {0};
    ret = copy_from_user(data, user_buffer, 64);
    if (ret != 0) {
        return -EFAULT;
    }
    ret = parse_u32_pair(data, value);
    if (ret != 0) {
        return -EFAULT;
    }
    ret = add_forward_fd(value); 
    *ppos = count;
    return count;
}

static ssize_t write_flush(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
    ssize_t ret;
    char data[64] = {0};
    ret = copy_from_user(data, user_buffer, 64);
    if (ret != 0) {
        return -EFAULT;
    }
    forward_flush();
    return count;
}

#define  LEN (4096*4)

static ssize_t show_conn(struct file *file, char __user *user_buffer, size_t count, loff_t *ppos)
{
    ssize_t ret;
    int len;
    static int maxlen;
    char *buff;

    if(file->private_data == 0 ) {
        maxlen = 0;
        buff = kmalloc(LEN, GFP_KERNEL);
        if( !buff ){
            logs("Fail to kmalloc");
            return 0;
        }
        file->private_data = buff;
        ret = string_all_conn_pair( buff, LEN); 
        *ppos = 0;
        maxlen = ret;

        if( ret > count ){
            len = count;
        }else{
            len = ret;
        }
        ret = copy_to_user(user_buffer, buff, len);
        if(ret) return -EFAULT;
        *ppos = *ppos + len;
        return len;
    }else{
        len = maxlen - *ppos;
        if( len > 0 ){
            if( len > count ){
                len = count;
            }
            ret = copy_to_user(user_buffer, (char *)file->private_data + *ppos, len);
            if(ret) return -EFAULT;
            *ppos = *ppos + len;
            return len;
        }else{
            kfree(file->private_data);
            file->private_data = 0;
            return 0;
        }
        return 0;
    }
}


static const struct file_operations port_fops = {
    .owner = THIS_MODULE,
    .read = read_port,
    .write = write_port,
};

static const struct file_operations forward_fops = {
    .owner = THIS_MODULE,
    .write = write_forward,
};

static const struct file_operations flush_fops = {
    .owner = THIS_MODULE,
    .write = write_flush,
};

static const struct file_operations show_fops = {
    .owner = THIS_MODULE,
    .read = show_conn,
};

static void create_proc_tree( void )
{
    struct proc_dir_entry *folder;
    folder = proc_mkdir(ROOT_PROC, NULL);

    proc_create("port", 0666, folder, &port_fops);
    proc_create("forward", 0222, folder, &forward_fops);
    proc_create("flush", 0222, folder, &flush_fops);
    proc_create("show", 0111, folder, &show_fops);
}

static void destroy_proc_tree( void )
{
    remove_proc_subtree( ROOT_PROC, NULL);
}


int proc_init( void )
{
    create_proc_tree( );
    return 0;
}

void proc_exit( void )
{
    destroy_proc_tree();
}

