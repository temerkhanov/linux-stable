// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2004 IBM Corporation
 * Authors:
 * Leendert van Doorn <leendert@watson.ibm.com>
 * Dave Safford <safford@watson.ibm.com>
 * Reiner Sailer <sailer@watson.ibm.com>
 * Kylene Hall <kjhall@us.ibm.com>
 *
 * Copyright (C) 2013 Obsidian Research Corp
 * Jason Gunthorpe <jgunthorpe@obsidianresearch.com>
 *
 * Device file system interface to the TPM
 */
#include <linux/slab.h>
#include "tpm-dev.h"

static int tpm_open(struct inode *inode, struct file *file)
{
	struct tpm_chip *chip;
	struct file_priv *priv;
	int ret = 0;

	chip = container_of(inode->i_cdev, struct tpm_chip, cdev);

	/* It's assured that the chip will be opened just once,
	 * by the check of the chip reference count.
	 */
	if (atomic_fetch_inc(&chip->refcount)) {
		dev_dbg(&chip->dev, "Another process owns this TPM\n");
		ret = -EBUSY;
		goto out;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	tpm_common_open(file, chip, priv, NULL);

	return 0;

 out:
	atomic_dec(&chip->refcount);
	wake_up_all(&chip->waitq);
	return -ENOMEM;
}

/*
 * Called on file close
 */
static int tpm_release(struct inode *inode, struct file *file)
{
	struct file_priv *priv = file->private_data;

	tpm_common_release(file, priv);
	atomic_dec(&priv->chip->refcount);
	wake_up_all(&priv->chip->waitq);
	kfree(priv);

	return 0;
}

const struct file_operations tpm_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = tpm_open,
	.read = tpm_common_read,
	.write = tpm_common_write,
	.poll = tpm_common_poll,
	.release = tpm_release,
};
