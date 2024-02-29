
#include "message_slot.h"
#include <linux/slab.h>

static slot_node *slot_list = NULL;

static int device_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    message_slot *slot = get_slot(slot_list, minor);
    if (slot == NULL)
    {
        printk(KERN_DEBUG "Could not find node with minor number %d\n", minor);
        slot = add_slot(&slot_list, minor)->data;
        if (slot == NULL)
        {
            return -1;
        }
        printk(KERN_DEBUG "Added node with minor number %d\n", minor);
    }
    file->private_data = slot;
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    unsigned long flags;
    message_slot *msg_slot = (message_slot *)file->private_data;
    channel_node *channeln = get_channel(msg_slot->channel_list, msg_slot->active_channel);
    char *channel_buffer;
    size_t buffer_ln, message_length;

    if (channeln == NULL)
    {
        return -EINVAL;
    }

    channel_buffer = channeln->data->buffer;
    if (channel_buffer == NULL)
    {
        return -EWOULDBLOCK;
    }
    buffer_ln = channeln->data->buffer_len;
    if (buffer_ln > length)
    {
        return -ENOSPC;
    }

    spin_lock_irqsave(&(channeln->data->lock), flags);
    if (channeln->data->devopen_flag)
    {
        spin_unlock_irqrestore(&(channeln->data->lock), flags);
        return -EBUSY;
    }
    channeln->data->devopen_flag = 1;
    spin_unlock_irqrestore(&(channeln->data->lock), flags);

    message_length = channeln->data->message_length;
    if (copy_to_user(buffer, channel_buffer, message_length))
    {
        spin_lock_irqsave(&(channeln->data->lock), flags);
        channeln->data->devopen_flag = 0;
        spin_unlock_irqrestore(&(channeln->data->lock), flags);
        return -EFAULT;
    }
    /*
    for (i = 0; i < message_length; i++, bytes_read++)
    {
        // put user can fail, since its atomic we need to revert back changes.
        int success = put_user(channel_buffer[i], &buffer[i]);
        if (success == -EFAULT)
        {
            bytes_read -= 1;
        }
    }
    */

    spin_lock_irqsave(&(channeln->data->lock), flags);
    channeln->data->devopen_flag = 0;
    spin_unlock_irqrestore(&(channeln->data->lock), flags);

    return message_length;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
    unsigned long flags;
    message_slot *msg_slot;
    channel_node *channeln;
    char *channel_buffer;
    size_t buffer_ln;

    if (length == 0 || length > 128)
    {
        return -EMSGSIZE;
    }
    msg_slot = (message_slot *)file->private_data;
    channeln = get_channel(msg_slot->channel_list, msg_slot->active_channel);

    if (channeln == NULL)
    {
        return -EINVAL;
    }

    spin_lock_irqsave(&(channeln->data->lock), flags);
    if (channeln->data->devopen_flag)
    {
        spin_unlock_irqrestore(&(channeln->data->lock), flags);
        return -EBUSY;
    }
    channeln->data->devopen_flag = 1;
    spin_unlock_irqrestore(&(channeln->data->lock), flags);

    channel_buffer = channeln->data->buffer;
    buffer_ln = channeln->data->buffer_len;
    if (buffer_ln < length)
    {
        char *new_buffer = (char *)kmalloc(sizeof(char) * length, GFP_KERNEL);
        int i = 0;
        if (new_buffer == NULL)
        {
            spin_lock_irqsave(&(channeln->data->lock), flags);
            channeln->data->devopen_flag = 0;
            spin_unlock_irqrestore(&(channeln->data->lock), flags);
            return -ENOMEM;
        }
        memset(new_buffer, 0, sizeof(char) * length);
        for (i = 0; i < buffer_ln; i++)
        {
            new_buffer[i] = channel_buffer[i];
        }
        kfree(channel_buffer);
        channel_buffer = new_buffer;
        buffer_ln = length;
        channeln->data->buffer = new_buffer;
        channeln->data->buffer_len = length;
    }

    if (copy_from_user(channel_buffer, buffer, length))
    {
        spin_lock_irqsave(&(channeln->data->lock), flags);
        channeln->data->devopen_flag = 0;
        spin_unlock_irqrestore(&(channeln->data->lock), flags);
        return -EFAULT;
    }
    /*
    bytes_written = 0;
    for (i = 0; i < length; i++, bytes_written++)
    {
        int success = get_user(channel_buffer[i], &buffer[i]);
        if (success == -EFAULT)
        {
            bytes_written -= 1;
        }
    }
    */
    channeln->data->message_length = length;

    spin_lock_irqsave(&(channeln->data->lock), flags);
    channeln->data->devopen_flag = 0;
    spin_unlock_irqrestore(&(channeln->data->lock), flags);

    return length;
}

static long int device_ioctl(struct file *file, unsigned int cmd, unsigned long param)
{
    int channelno = (int)param;
    message_slot *msg_slot;
    channel_node *channeln;

    if (cmd != MSG_SLOT_CHANNEL || channelno == 0)
    {
        return -EINVAL;
    }
    printk(KERN_DEBUG "requested channel change to channel: %d\n", channelno);
    msg_slot = (message_slot *)file->private_data;
    msg_slot->active_channel = channelno;
    channeln = get_channel(msg_slot->channel_list, channelno);
    if (channeln == NULL)
    {
        add_channel(msg_slot->channel_list, msg_slot, channelno);
        channeln = get_channel(msg_slot->channel_list, channelno);
    }
    if (channeln == NULL)
    {
        printk(KERN_ALERT "Couldnt find channel, this shouldnt happen\n");
        return -EFAULT;
    }

    return 0;
}

struct file_operations ops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = device_ioctl,
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write};

static int __init initialize(void)
{

    int major = register_chrdev(MAJOR_NUM, "message_slot", &ops);
    if (major < 0)
    {
        printk(KERN_ALERT "An error ocurred registering the module.\n");
        return -major;
    }

    printk(KERN_DEBUG "The module has been successfully loaded\n");
    return 0;
}

static void __exit cleanup(void)
{
    slot_node *slot;
    while ((slot = slot_list) != NULL)
    {
        int success = delete_slot(&slot_list, slot->data->minor);
        if (success < 0)
        {
            printk(KERN_ERR "an error occured freeing a unit memory allocation");
        }
    }
    unregister_chrdev(MAJOR_NUM, "message_slot");
    printk(KERN_DEBUG "The module has been successfully deloaded\n");
}

static channel_node *add_channel(channel_node *head, message_slot *slot, int channel_num)
{
    channel_node *new_node = (channel_node *)kmalloc(sizeof(channel_node), GFP_KERNEL);
    slot_channel *new_channel = (slot_channel *)kmalloc(sizeof(slot_channel), GFP_KERNEL);
    channel_node *node;

    memset(new_channel, 0, sizeof(slot_channel));
    memset(new_node, 0, sizeof(channel_node));
    new_channel->channel_num = channel_num;
    spin_lock_init(&(new_channel->lock));
    new_node->next = NULL;
    new_node->data = new_channel;

    if (head == NULL)
    {
        slot->channel_list = new_node;
        return new_node;
    }

    // head is not null so theres no point from double pointer
    node = head;
    while (node->next != NULL)
    {
        node = node->next;
    }

    node->next = new_node;
    return new_node;
}

static channel_node *get_channel(channel_node *head, int channel_num)
{
    while (head != NULL && head->data->channel_num != channel_num)
    {
        head = head->next;
    }
    return head;
}

static int delete_channel(channel_node *head, message_slot *slot, int channel_num)
{
    channel_node *channeln;
    if (head == NULL)
    {
        return NODE_NOT_FOUND;
    }
    if ((head)->data->channel_num == channel_num)
    {
        channeln = head;
        slot->channel_list = head->next;
    }
    else
    {
        channel_node *prev = head;
        channel_node *curr = head->next;
        while (curr != NULL && curr->data->channel_num != channel_num)
        {
            prev = curr;
            curr = curr->next;
        }
        if (curr == NULL)
        {
            return NODE_NOT_FOUND;
        }
        channeln = curr;
        prev->next = curr->next;
    }

    kfree(channeln->data->buffer);
    kfree(channeln->data);
    kfree(channeln);
    return NODE_DEL_SUCCESS;
}

static slot_node *add_slot(slot_node **head, int minor)
{
    slot_node *new_slot_node = (slot_node *)kmalloc(sizeof(slot_node), GFP_KERNEL);
    message_slot *data = (message_slot *)kmalloc(sizeof(message_slot), GFP_KERNEL);
    slot_node *cur;

    if (new_slot_node == NULL || data == NULL)
    {
        if (new_slot_node != NULL)
        {
            kfree(new_slot_node);
        }
        if (data != NULL)
        {
            kfree(data);
        }
        return NULL;
    }
    memset(new_slot_node, 0, sizeof(slot_node));
    memset(data, 0, sizeof(message_slot));
    new_slot_node->data = data;
    new_slot_node->data->minor = minor;

    if (*head == NULL)
    {
        *head = new_slot_node;
        return new_slot_node;
    }
    cur = *head;
    while (cur->next != NULL)
    {
        cur = cur->next;
    }
    cur->next = new_slot_node;

    return new_slot_node;
}

static message_slot *get_slot(slot_node *head, int minor)
{

    slot_node *cur_node = head;
    while (cur_node != NULL && cur_node->data->minor != minor)
    {
        cur_node = cur_node->next;
    }
    if (cur_node == NULL)
        return NULL;
    return cur_node->data;
}

static int delete_slot(slot_node **head, int minor)
{
    slot_node *slot;
    channel_node *channel;

    if (*head == NULL)
    {
        return NODE_NOT_FOUND;
    }
    if ((*head)->data->minor == minor)
    {
        slot = *head;
        *head = (*head)->next;
    }
    else
    {
        slot_node *prev = *head;
        slot_node *curr = (*head)->next;
        while (curr != NULL && curr->data->minor != minor)
        {
            prev = curr;
            curr = curr->next;
        }
        if (curr == NULL)
        {
            return NODE_NOT_FOUND;
        }
        slot = curr;
        prev->next = curr->next;
    }

    while ((channel = slot->data->channel_list) != NULL)
    {
        delete_channel(channel, slot->data, channel->data->channel_num);
    }
    kfree(slot->data);
    kfree(slot);
    return NODE_DEL_SUCCESS;
}

module_init(initialize);
module_exit(cleanup)
