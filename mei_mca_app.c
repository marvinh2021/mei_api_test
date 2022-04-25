/**
 * MEI IOCTL interface test
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/mei.h>
#include <linux/uuid.h>

#define MEI_DEVICE "/dev/mei0"

/* gcc -std=c11 */
#define ct_assert(x) _Static_assert((x), #x)

#define MCHI_MAX_BUFFER_SIZE    (4*1024)
#define MAX_MCA_COMMAND_SIZE	( MCHI_MAX_BUFFER_SIZE )
#define MAX_MCA_RESPONSE_SIZE	( MAX_MCA_COMMAND_SIZE )

/* MCA command ID */
#define     GET_FILE_ATTRIBUTES_COMMAND_ID                  0x01
#define     READ_FILE_COMMAND_ID                            0x02
#define     SET_FILE_COMMAND_ID                             0x03
#define     COMMIT_FILES_COMMAND_ID                         0x04
#define     CORE_BIOS_DONE_COMMAND_ID                       0x05
#define     COMMIT_TO_FPF_COMMAND_ID                        0x06
#define     RP_BIND_COMMAND_ID                              0x07// RP BIND cmand removed:
#define     GET_RP_PROVISIONING_STATE_COMMAND_ID            0x08

extern char const * errno_name(int errno_);

//MKHI host message header. This header is part of HECI message sent from MEBx via
//Host Configuration Interface (HCI). ME Configuration Manager or Power Configuration
//Manager also include this header with appropriate fields set as part of the
//response message to the HCI.

typedef union _mkhi_message_header_t
{
    uint32_t      data;
    struct
    {
        uint32_t  group_id    :8;   /// the target client id registered to process the message
        uint32_t  command     :7;   /// command specific to HECI client
        uint32_t  is_response :1;   /// response from client
        uint32_t  reserved    :8;   /// reserved bit field
        uint32_t  result      :8;   /// result
    } fields;
} mkhi_message_header_t;
ct_assert(sizeof(mkhi_message_header_t) == 4);

typedef union _rp_status
{
    uint32_t    data;
    struct
    {
        uint32_t    device_type              : 2; // 00 - EMMC , 01 - UFS , 10 - SPI,   11 - No device
        uint32_t    rp_support               : 1; // 0 - nvm does not supprt relay protection, 1 -  nvm supports replay protection
        uint32_t    bind_status              : 1; // 0 - pre_bin, 1 - post_bind
        uint32_t    rp_rebinding_allowed     : 1; // 0 - rebind does not allowed, 1 - rebind allowed
        uint32_t    binding_counter          : 5; // current binding counter
        uint32_t    max_rebinds              : 4; // number of max rebinds allowed
        uint32_t    spi_rpmc_counters        : 3; // number of RPMC counters avaiable ( in case of RPMC ) , 0 if EMMC/UFS
        uint32_t    spi_rpmc_chipselect      : 1; // 0 if EMMC/UFS
        uint32_t    spi_rpmc_fatal_error     : 8; // 0 if EMMC/UFS
        uint32_t    reserved                 : 6;
    }bits;
} rp_status;

typedef struct _mca_general_command_t
{
    mkhi_message_header_t   mkhi_header;
} mca_general_command_t;
ct_assert(sizeof(mca_general_command_t) <= MAX_MCA_COMMAND_SIZE);

/*
 * get rp status response from MCA to Hostcomm.
 * GET_RP_PROVISIONING_STATE_COMMAND_ID
 */
typedef struct _mca_rp_status_response_t
{
    mkhi_message_header_t   mkhi_header;
    rp_status               current_status;
} mca_rp_status_response_t;
ct_assert(sizeof(mca_rp_status_response_t) <= MAX_MCA_RESPONSE_SIZE);

struct uuid_entry {
	const char* name;
	const uuid_le uuid;
};

static struct uuid_entry uuid_list[] = {
	{"MCA_DYNAMIC_GUID", GUID_INIT(0xdd17041c, 0x09ea, 0x4b17, \
		0xa2, 0x71, 0x5b, 0x98, 0x98, 0x67, 0xec, 0x65) },
};

int main()
{
	int fd;
	struct mei_connect_client_data data;
	mca_general_command_t mca_req = {
		.mkhi_header = {
			.fields = {
				.command = GET_RP_PROVISIONING_STATE_COMMAND_ID,
			},
		},
	};
	mca_rp_status_response_t mca_result;

	/* Open device file */
	fd = open(MEI_DEVICE, O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		printf("Open file %s error!\n", MEI_DEVICE);
		return -1;
	}

	/* MEI connect client */
	memset((void *)&data, 0U, sizeof(data));
	data.in_client_uuid = uuid_list[0].uuid;
	if (ioctl(fd, IOCTL_MEI_CONNECT_CLIENT, &data) == -1) {
		char prompt[128];
		int err = errno;
		sprintf(prompt, "\n%s(%s)", uuid_list[0].name, errno_name(err));
		perror(prompt);
	} else {
		printf("\n%s: Ver=%d, MaxLen=%d\n",
				uuid_list[0].name,
				data.out_client_properties.protocol_version,
				data.out_client_properties.max_msg_length);
	}

	/* Write MCA req */
	write(fd, &mca_req, sizeof(mca_req));

	/* Read resp */
	read(fd, &mca_result, sizeof(mca_result));

exit:
	close(fd);

	return 0;
}
