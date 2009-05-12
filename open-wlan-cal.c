#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

const char DSME_DEFAULT_SOCKET_PATH[] = "/tmp/dsmesock";

int get_from_dsme(const void *request, const int bytes_send, 
		void *buff, const int bytes_read) {
	int sock;
	int ret;
	struct sockaddr_un sockaddr;

	/* create socket */
	if ((sock = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
		perror("Could not create socket");
		return -1;
	}

	/* connect */
	sockaddr.sun_family = AF_LOCAL;
	/* TODO: allow changing socket path */
	strcpy(sockaddr.sun_path, DSME_DEFAULT_SOCKET_PATH);
	ret = strlen(sockaddr.sun_path) + sizeof(sockaddr.sun_family);
	if (connect(sock, (struct sockaddr *)&sockaddr, ret) == -1) {
		perror("Could not connect to socket");
		close(sock);
		return -1;
	}

	/* Didn't read what this stuff does,
		but without this call things do not work */
	if (fcntl(sock, F_SETFL, O_RDONLY|O_NONBLOCK) == -1) {
		perror("Could not fnctl");
		close(sock);
		return -1;
	}

	/* send request */
	ret = write(sock, request, bytes_send);
	if (ret == -1) {
		perror("Could not send dsme request");
		close(sock);
		return -1;
	} else if (ret != bytes_send) {
		fprintf(stderr, "Could write only %d bytes out of %d", ret, bytes_send);
		perror(NULL);
		close(sock);
		return -1;
	}

	/* read response */
	ret = read(sock, buff, bytes_read);
	if (ret == -1) {
		perror("Could not read dsme response");
	} else if (ret != bytes_read) {
		fprintf(stderr, "Could read only %d bytes out of %d", ret, bytes_read);
		perror(NULL);
	}

	/* cleanup */
	close(sock);
	return ret;
}

int write_to(const char filename[], const void *value, const int len) {
	int f;
	int ret;
	if ((f = open(filename, O_WRONLY)) == -1) {
		fprintf(stderr, "Could not open file %s", filename);
		perror(NULL);
		return -1;
	}
	ret = write(f, value, len);
	if (ret == -1) {
		fprintf(stderr, "Could not write data to %s", filename);
		perror(NULL);
	} else if (ret != len) {
		fprintf(stderr, "Could write only %d bytes out of %d", ret, len);
		perror(NULL);
	}
	close(f);
	return ret;
}

int set_mac(const void *value, const int len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_mac_address", value, len);
}

int set_default_country(const void *value, const int len) {
	return write_to("sys/devices/platform/wlan-omap/default_country", value, len);
}

int set_tx_limits(const void *value, const int len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_output_limits",
		value, len);
}

int set_iq_values(const void *value, const int len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_iq", value, len);
}

int set_tx_tuned_data(const void *value, const int len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_pa_curve_data",
		value, len);
}

int set_rx_tuned_data(const void *value, const int len) {
	return write_to("/sys/devices/platform/wlan-omap/cal_rssi", value, len);
}

void load_from_dsme() {
	const char mac_request[]
		= " \0\0\0\0\22\0\0wlan-mac\0\0\0\0\0\0\0\0\0\0\0\0\10 \1\0";
	char mac_address_data[60];
	char mac_address[6];
	int len;
	int ret;
	len = sizeof(mac_address_data);
	printf("Pushing MAC address...");
	ret = get_from_dsme(mac_request, sizeof(mac_request), mac_address_data, len);
	if (ret == len) {
		unsigned int i;
		for (i = 0; i < sizeof(mac_address); i++) {
			mac_address[i] = mac_address_data[32 + 4 * i];
		}
		if (set_mac(mac_address, sizeof(mac_address))) {
			printf("OK\n");
		}
	}
}

int main() {
	/* Many cool things can be done here. Saving/loading data to/from files,
	commandline args, direct /dev/mtd1 access, etc... */
	load_from_dsme();
	return 0;
}
