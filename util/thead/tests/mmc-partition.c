/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <commonlib/storage.h>
#include "mmc.c"

static uint8_t partition;
static int send(struct sd_mmc_ctrlr *ctrlr, struct mmc_command *cmd, struct mmc_data *data)
{
	if (cmd->cmdidx == MMC_CMD_SEND_EXT_CSD) {
		uint8_t *ext = (void *)data->dest;

		memset(ext, 0, 512);
		ext[EXT_CSD_REV] = 8;
		ext[EXT_CSD_PART_CONF] = partition;
		ext[EXT_CSD_SEC_CNT + 3] = 1;
		ext[EXT_CSD_BOOT_SIZE_MULT] = 32;
		ext[EXT_CSD_HC_ERASE_GRP_SIZE] = 1;
		return 0;
	}
	assert(cmd->cmdidx == MMC_CMD_SWITCH);
	if (((cmd->cmdarg >> 16) & 255) == EXT_CSD_PART_CONF)
		partition = cmd->cmdarg >> 8;
	return 0;
}
int sd_mmc_send_status(struct storage_media *media, ssize_t tries) { return 0; }
uint64_t sd_mmc_extract_uint32_bits(const uint32_t *data, int start, int size) { return 0; }
void udelay(unsigned int us) {}
void mdelay(unsigned int ms) {}

static void set_ios(struct sd_mmc_ctrlr *ctrlr) {}

int main(void)
{
	struct sd_mmc_ctrlr ctrlr = { .send_cmd = send, .set_ios = set_ios,
		.caps = DRVR_CAP_8BIT | DRVR_CAP_4BIT };
	struct storage_media media = { .ctrlr = &ctrlr, .version = MMC_VERSION_4,
		.write_bl_len = 512, .read_bl_len = 512 };

	partition = 0x48;
	assert(mmc_update_capacity(&media) == 0);
	assert(media.partition_config == 0x48);
	assert(mmc_set_bus_width(&media) == 0 && ctrlr.bus_width == 8);
	assert(mmc_set_partition(&media, 1) == 0 && partition == 0x49);
	assert(mmc_set_partition(&media, 0) == 0 && partition == 0x48);
	assert(mmc_set_partition(&media, 2) == 0 && partition == 0x4a);
	puts("PASS: eMMC width validation and preservation of boot-enable/boot-ack bits");
}
